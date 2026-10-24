#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Rewrite GateServer/HttpConnection.cpp for streaming uploads.

The parser moves from dynamic_body (buffers the whole request body in RAM)
to buffer_body (caller supplied chunk), which is what makes 300MB uploads
possible without ~1.1GB of RAM per connection.
"""
PATH = r"D:\QTcode\HChat\GateServer\HttpConnection.cpp"

NEW = r'''#include "HttpConnection.h"
#include "LogicSystem.h"
#include "logger.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc)
    : _socket(ioc), _scratch(kBodyChunkSize) {
}

HttpConnection::~HttpConnection()
{
    // An abandoned streaming upload must not leave a .part file behind:
    // upload/ would slowly fill with garbage on every dropped connection.
    if (_is_upload && !_upload_finished && !_upload_path.empty()) {
        if (_upload_ofs.is_open()) {
            _upload_ofs.close();
        }
        boost::system::error_code ec;
        boost::filesystem::remove(_upload_path, ec);
    }
}

void HttpConnection::Start()
{
    // body_limit is compared against the declared Content-Length while the
    // REQUEST HEADER is being parsed (boost/beast/http/impl/basic_parser.ipp),
    // and it behaves as a decrementing budget rather than a running total.
    // So it has to be armed before the first async_read and must be at least
    // as large as the biggest body we accept. Beast's default for requests
    // is only 1MB, which silently rejected anything bigger.
    _parser.body_limit(kMaxHttpBodyBytes);

    _last_activity = std::chrono::steady_clock::now();
    ArmDeadline();
    DoReadBody();
}

void HttpConnection::ArmDeadline()
{
    auto self = shared_from_this();
    // Patrol on an interval and only close after kIdleTimeoutSec of NO
    // activity. Refreshing from inside the read handler is not enough: a peer
    // that stalls produces no read completions at all, so a one-shot timer
    // armed at accept time still kills a slow-but-progressing upload.
    deadline_.expires_after(std::chrono::seconds(kPatrolIntervalSec));
    deadline_.async_wait([self](beast::error_code ec) {
        if (ec) {
            return;   // cancelled
        }
        auto idle = std::chrono::steady_clock::now() - self->_last_activity;
        if (idle >= std::chrono::seconds(kIdleTimeoutSec)) {
            LOG_INFO("http connection idle for {}s, closing", kIdleTimeoutSec);
            beast::error_code ig;
            self->_socket.close(ig);
            return;
        }
        self->ArmDeadline();
    });
}

void HttpConnection::DoReadBody()
{
    auto self = shared_from_this();
    // buffer_body copies body bytes into a caller supplied buffer, so the
    // destination has to be re-armed before every read.
    _parser.get().body().data = _scratch.data();
    _parser.get().body().size = _scratch.size();

    http::async_read(_socket, _buffer, _parser,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            self->OnReadBody(ec, bytes_transferred);
        });
}

void HttpConnection::OnReadBody(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    _last_activity = std::chrono::steady_clock::now();

    if (ec == http::error::body_limit) {
        // The declared Content-Length is over the cap. Answer 413 instead of
        // leaving the peer waiting for a response that would never arrive.
        LOG_ERROR("request body over limit, rejected");
        _response.version(11);
        _response.keep_alive(false);
        _response.result(http::status::payload_too_large);
        _response.set(http::field::content_type, "text/plain");
        beast::ostream(_response.body()) << "request body too large";
        WriteResponse();
        return;
    }

    if (ec && ec != http::error::need_buffer) {
        LOG_ERROR("HTTP read error: {}", ec.what());
        return;
    }

    // The header is always parsed before the first body byte is delivered,
    // so this branch runs exactly once: pick the route and, for uploads,
    // open the output file before any chunk arrives.
    if (!_header_done && _parser.is_header_done()) {
        _header_done = true;
        PreParseGetParam();

        auto target = _parser.get().target();
        std::string t(target.data(), target.size());
        auto method = _parser.get().method();
        if (method == http::verb::post &&
            (t.rfind("/upload_image", 0) == 0 || t.rfind("/upload_file", 0) == 0)) {
            _is_upload = true;
            if (_parser.content_length()) {
                _has_content_length = true;
                _content_length = *_parser.content_length();
            }
            auto ct = _parser.get().find(http::field::content_type);
            if (ct != _parser.get().end()) {
                _content_type = std::string(ct->value().data(), ct->value().size());
            }
            if (!LogicSystem::GetInstance()->PrepareUpload(shared_from_this())) {
                return;   // error response already written
            }
        }
    }

    // How many body bytes landed in the scratch buffer this round. The parser
    // advances body().data and decrements body().size as it copies.
    std::size_t written = _scratch.size() - _parser.get().body().size;
    if (written > 0) {
        if (_is_upload) {
            _upload_ofs.write(_scratch.data(), static_cast<std::streamsize>(written));
            if (!_upload_ofs) {
                LOG_ERROR("upload write to disk failed");
                if (_upload_ofs.is_open()) {
                    _upload_ofs.close();
                }
                boost::system::error_code fec;
                boost::filesystem::remove(_upload_path, fec);
                _response.result(http::status::internal_server_error);
                _response.set(http::field::content_type, "text/plain");
                beast::ostream(_response.body()) << "disk write failed";
                WriteResponse();
                return;
            }
            _upload_written += written;
            // FNV-1a, updated incrementally so the content hash is available
            // at the end without ever holding the whole file in memory.
            for (std::size_t i = 0; i < written; ++i) {
                _upload_hash ^= static_cast<unsigned char>(_scratch[i]);
                _upload_hash *= 1099511628211ULL;
            }
        }
        else {
            _body_str.append(_scratch.data(), written);
        }
    }

    if (ec == http::error::need_buffer) {
        // Scratch buffer is full and the chunk was already flushed above:
        // re-arm and read the next one.
        DoReadBody();
        return;
    }

    if (!_parser.is_done()) {
        DoReadBody();
        return;
    }

    FinishMessage();
}

void HttpConnection::FinishMessage()
{
    _last_activity = std::chrono::steady_clock::now();
    if (_is_upload) {
        LogicSystem::GetInstance()->FinishUpload(shared_from_this());
        return;
    }
    HandleReq();
}

void HttpConnection::HandleReq() {
    //set version
    _response.version(_parser.get().version());
    //set keep alive
    _response.keep_alive(false);
    //parse the url and params
    PreParseGetParam();

    //handle get req
    if (_parser.get().method() == http::verb::get) {
        //dispatch to LogicSystem HandleGet
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
        if (!success) {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url not found\r\n";

            WriteResponse();
            return;
        }

        LOG_INFO("handle GET req success");
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");

        WriteResponse();
        return;
    }

    //handle post req
    if (_parser.get().method() == http::verb::post) {
        //dispatch to LogicSystem HandlePost
        bool success = LogicSystem::GetInstance()->HandlePost(_parser.get().target(), shared_from_this());
        if (!success) {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url not found\r\n";
            WriteResponse();
            return;
        }

        LOG_INFO("handle POST req success");
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }
}

//write the response back to the client asynchronously, then close the send
//side of the connection and cancel the deadline
void HttpConnection::WriteResponse() {
    auto self = shared_from_this();
    //set the content length of the HTTP response header
    _response.content_length(_response.body().size());
    //async write
    http::async_write(
        _socket,
        _response,
        [self](beast::error_code ec, std::size_t)
        {
            LOG_INFO("async write HTTP response done");
            //only close the send side, keep the connection alive otherwise
            self->_socket.shutdown(tcp::socket::shutdown_send, ec);
            //cancel the deadline
            self->deadline_.cancel();
        });
}

tcp::socket& HttpConnection::GetSocket()
{
    return _socket;
}

//hex --> dec
unsigned char ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

//dec --> hex
unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 10 + 'A';
    else if (x >= 'a' && x <= 'z') y = x - 10 + 'a';
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}

//url encode
std::string UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        //check whether the character is a letter or a digit
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ') //space -> +
            strTemp += "+";
        else
        {
            //other characters are percent-encoded
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] & 0x0F);
        }
    }
    return strTemp;
}

//url decode
std::string UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        //check whether it is a percent-encoded sequence
        if (str[i] == '%')
        {
            assert(i + 2 < str.length());
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[i+1]);
            strTemp += high * 16 + low;
        }
        else if (str[i] == '+') //+ -> space
            strTemp += ' ';
        else
            strTemp += str[i];
    }
    return strTemp;
}

//parse the request URI, splitting the path from the query string into
//_get_url and _get_params
void HttpConnection::PreParseGetParam() {
    //get the URI
    auto uri = _parser.get().target();
    //find the start of the query string
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos) {
        _get_url = std::string(uri);
        return;
    }

    _get_url = std::string(uri.substr(0, query_pos));
    std::string query_string(uri.substr(query_pos + 1));
    std::string key;
    std::string value;
    size_t pos = 0;
    while ((pos = query_string.find('&')) != std::string::npos) {
        auto pair = query_string.substr(0, pos);
        auto eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(pair.substr(0, eq_pos));
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }

    //handle the last pair, which has no trailing '&'
    if (!query_string.empty()) {
        auto eq_pos = query_string.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(query_string.substr(0, eq_pos));
            value = UrlDecode(query_string.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        else {
            key = UrlDecode(query_string);
            _get_params[key] = std::string();
        }
    }
}
'''

data = NEW.replace("\n", "\r\n").encode("gbk")
with open(PATH, "wb") as f:
    f.write(data)
back = open(PATH, "rb").read().decode("gbk")
assert back.replace("\r\n", "\n") == NEW
print("HttpConnection.cpp rewritten:", len(data), "bytes, GBK round-trip OK")
