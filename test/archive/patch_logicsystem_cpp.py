#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Patch GateServer/LogicSystem.cpp for streaming uploads.

1. Replace the base64+JSON upload handler with PrepareUpload/FinishUpload.
2. Replace the 5 remaining buffers_to_string(_request.body().data()) call
   sites with the accumulated _body_str (buffer_body has no .data()).
"""
PATH = r"D:\QTcode\HChat\GateServer\LogicSystem.cpp"

START_MARK = "    auto upload_handler = [](std::shared_ptr<HttpConnection> connection) {"
END_MARK = '    RegPost("/upload_file", upload_handler);'

NEW_METHODS = r'''// ---- streaming upload -------------------------------------------------------
//
// The request body IS the raw file bytes: no base64, no JSON envelope. The
// metadata travels in the query string, e.g.
//
//   POST /upload_file?uid=2&file_name=report.pdf
//   <300MB of raw bytes>
//
// HttpConnection intercepts these two routes before HandlePost(), streams the
// body to disk in 64KB chunks, and calls PrepareUpload() once the request
// header is parsed (open the output file, validate parameters) then
// FinishUpload() once the last chunk has landed (finalise the name, respond).
// The response shape is unchanged so the client needs no parsing changes:
//
//   {error, url, unique_name, size, md5}
//
// Compared with the old base64-in-JSON design this removes, for a 300MB file,
// roughly 2.7GB of client RAM and 1.1GB of server RAM per connection, plus the
// jsoncpp parse of a 400MB JSON document.

// Per-process counter so concurrent uploads never share a .part name.
static std::atomic<unsigned> g_upload_seq{ 0 };

bool LogicSystem::PrepareUpload(std::shared_ptr<HttpConnection> con) {
    // NOTE: on failure this only FILLS _response. The caller (OnReadBody) keeps
    // draining the request body first and writes the response afterwards, so a
    // peer that is still uploading gets a readable JSON error instead of a
    // connection reset.
    con->_response.set(http::field::content_type, "text/json");
    Json::Value root;

    // The old protocol wrapped the file in a JSON envelope. Refuse it loudly
    // instead of happily writing the JSON text itself to disk.
    if (con->_content_type.find("json") != std::string::npos) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "expected a raw binary body, not application/json";
        beast::ostream(con->_response.body()) << root.toStyledString();
        return false;
    }

    auto name_it = con->_get_params.find("file_name");
    if (name_it == con->_get_params.end() || name_it->second.empty()) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "missing file_name";
        beast::ostream(con->_response.body()) << root.toStyledString();
        return false;
    }
    con->_upload_file_name = name_it->second;

    // Checked against the DECLARED Content-Length, so an oversized upload is
    // refused before a single byte is written. A chunked upload that declares
    // no length is caught by the running byte counter in OnReadBody().
    if (con->_has_content_length && con->_content_length > kMaxUploadBytes) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "file too large (max 300MB)";
        beast::ostream(con->_response.body()) << root.toStyledString();
        return false;
    }

    // Keep the ORIGINAL extension: non-image files (pdf/zip/docx/...) must not
    // be silently renamed. Executable and script types are neutralised to .bin
    // so they cannot be served back as runnable.
    std::string ext;
    auto dot = con->_upload_file_name.rfind('.');
    if (dot != std::string::npos && dot + 1 < con->_upload_file_name.size()) {
        auto e = con->_upload_file_name.substr(dot);
        std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) {
            return static_cast<char>(::tolower(c));
        });
        bool blocked = (e == ".exe" || e == ".bat" || e == ".cmd" || e == ".com"
                     || e == ".scr" || e == ".pif" || e == ".vbs" || e == ".vbe"
                     || e == ".js" || e == ".jse" || e == ".wsf" || e == ".wsh"
                     || e == ".ps1" || e == ".msi" || e == ".dll" || e == ".jar"
                     || e == ".php" || e == ".sh");
        ext = blocked ? std::string(".bin") : e;
    }
    con->_upload_ext = ext;

    // Write to <stamp>_<seq>.part first: the content hash is only known once
    // the whole body has streamed through, and the final name embeds it.
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%lld_%u", (long long)now,
                  g_upload_seq.fetch_add(1));

    auto cfg_path = boost::filesystem::current_path() / "config.ini";
    auto base_dir = boost::filesystem::absolute(cfg_path).parent_path();
    auto dir_path = base_dir / "upload";
    boost::system::error_code ec;
    boost::filesystem::create_directories(dir_path, ec);
    if (ec) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = std::string("cannot create upload dir: ") + ec.message();
        beast::ostream(con->_response.body()) << root.toStyledString();
        return false;
    }

    con->_upload_path = (dir_path / (std::string(stamp) + ".part")).string();
    // Windows: the output path must stay wide so a non-ASCII install directory
    // still works. Use the wstring CONSTRUCTOR (the form the previous
    // implementation already relied on) and move the result into the member:
    // the open() member has no wstring overload in libstdc++, while the
    // constructor form is the one MSVC actually ships.
    std::ofstream ofs(boost::filesystem::path(con->_upload_path).wstring(),
                      std::ios::binary | std::ios::trunc);
    if (!ofs) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "cannot open output file";
        beast::ostream(con->_response.body()) << root.toStyledString();
        return false;
    }
    con->_upload_ofs = std::move(ofs);
    return true;
}

void LogicSystem::FinishUpload(std::shared_ptr<HttpConnection> con) {
    Json::Value root;

    // Same reasoning as HandleReq(): tell the peer not to reuse the connection,
    // because WriteResponse() shuts the send side down right after this.
    con->_response.keep_alive(false);

    con->_upload_ofs.flush();
    if (!con->_upload_ofs) {
        LOG_ERROR("流式上传：flush 失败");
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "disk write failed";
        beast::ostream(con->_response.body()) << root.toStyledString();
        con->WriteResponse();
        return;
    }
    con->_upload_ofs.close();

    if (con->_upload_written == 0) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "empty file";
        beast::ostream(con->_response.body()) << root.toStyledString();
        con->WriteResponse();
        return;
    }

    // A truncated body means the peer vanished mid-upload. Drop it instead of
    // publishing a corrupt file: _upload_finished stays false, so the
    // HttpConnection destructor removes the .part file.
    if (con->_has_content_length && con->_upload_written != con->_content_length) {
        LOG_ERROR("流式上传：预期 {} 字节，实际 {} 字节",
                  (long long)con->_content_length, (long long)con->_upload_written);
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "truncated upload";
        beast::ostream(con->_response.body()) << root.toStyledString();
        con->WriteResponse();
        return;
    }

    // Final name embeds the FNV-1a hash computed while streaming, so identical
    // content always lands on the same name.
    std::ostringstream hoss;
    hoss << std::hex << std::setw(16) << std::setfill('0') << con->_upload_hash;
    auto hash_str = hoss.str();

    char stamp[32];
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::snprintf(stamp, sizeof(stamp), "%lld", (long long)now);

    auto unique_name = std::string(stamp) + "_" + hash_str + con->_upload_ext;
    auto final_path = boost::filesystem::path(con->_upload_path).parent_path() / unique_name;
    boost::system::error_code ec;
    boost::filesystem::rename(con->_upload_path, final_path, ec);
    if (ec) {
        // Rename failed: keep the .part name rather than failing the upload.
        LOG_ERROR("流式上传：重命名失败: {}", ec.message());
        unique_name = boost::filesystem::path(con->_upload_path).filename().string();
    }
    else {
        con->_upload_path = final_path.string();
    }
    con->_upload_unique_name = unique_name;
    con->_upload_finished = true;

    root["error"] = ErrorCodes::Success;
    root["url"] = std::string("upload/") + unique_name;
    root["unique_name"] = unique_name;
    root["size"] = (Json::Int64)con->_upload_written;
    root["md5"] = hash_str;
    LOG_INFO("流式上传成功: 文件名={}, 大小={}", unique_name,
             (long long)con->_upload_written);
    beast::ostream(con->_response.body()) << root.toStyledString();
    con->WriteResponse();
}

'''

text = open(PATH, "rb").read().decode("gbk").replace("\r\n", "\n")

# ---- 1. drop the old upload handler + its RegPost registrations ----
i = text.find(START_MARK)
assert i != -1, "upload handler start marker not found"
j = text.find(END_MARK, i)
assert j != -1, "upload handler end marker not found"
j += len(END_MARK)
removed = text[i:j]
assert "base64Decode" in removed, "removed block does not look like the old handler"
text = text[:i] + text[j:]
print("removed old upload handler: %d chars" % len(removed))

# ---- 2. swap the body extraction at the remaining call sites ----
OLD_BODY = "        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());"
NEW_BODY = "        auto body_str = connection->_body_str;"
n = text.count(OLD_BODY)
assert n == 5, "expected 5 remaining body_str sites, found %d" % n
text = text.replace(OLD_BODY, NEW_BODY)
print("rewrote %d body extraction sites" % n)

# ---- 3. insert the new methods just before HandleGet ----
anchor = "bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {"
assert text.count(anchor) == 1, "HandleGet anchor not unique"
text = text.replace(anchor, NEW_METHODS + anchor)

# Keep the file in its original LF form: GateServer sources are LF in git and
# core.autocrlf=input normalises on commit anyway, so there is no reason to
# flip them to CRLF here.
out = text.encode("gbk")
open(PATH, "wb").write(out)
back = open(PATH, "rb").read().decode("gbk")
assert back == text, "round trip mismatch"
print("LogicSystem.cpp patched:", len(out), "bytes")
print("non-ascii bytes:", sum(1 for b in out if b > 127))
