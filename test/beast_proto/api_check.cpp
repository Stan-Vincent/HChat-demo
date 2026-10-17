// Verify the exact Beast / boost::filesystem constructs used by the patched
// GateServer that the earlier prototype did NOT already exercise:
//   - message::base().find(http::field::content_type) + value()
//   - parser.get().method() / version() / target() -> std::string
//   - boost::filesystem::rename(from, to, error_code&)
//   - boost::filesystem::path::wstring()
//   - ofstream opened from a wstring path
//   - the FNV-1a incremental hash formatting used for the upload name
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace fs    = boost::filesystem;
using tcp = net::ip::tcp;

int main()
{
    net::io_context ioc;
    tcp::socket sock(ioc);
    http::request_parser<http::buffer_body> parser;
    std::vector<char> scratch(64 * 1024);

    // ---- the exact read-loop prologue from HttpConnection::DoReadBody ----
    parser.body_limit(320ull * 1024 * 1024);
    parser.get().body().data = scratch.data();
    parser.get().body().size = scratch.size();
    std::size_t written = scratch.size() - parser.get().body().size;

    // ---- header access from OnReadBody ----
    parser.get().base().version(11);
    parser.get().method(http::verb::post);
    parser.get().target("/upload_file?uid=2&file_name=a.bin");
    parser.get().set(http::field::content_type, "application/octet-stream");

    bool is_upload = false;
    std::string content_type;
    std::uint64_t content_length = 0;
    bool has_cl = false;

    auto target = parser.get().target();
    std::string t(target.data(), target.size());
    auto method = parser.get().method();
    if (method == http::verb::post &&
        (t.rfind("/upload_image", 0) == 0 || t.rfind("/upload_file", 0) == 0)) {
        is_upload = true;
        if (parser.content_length()) {
            has_cl = true;
            content_length = *parser.content_length();
        }
        auto ct = parser.get().base().find(http::field::content_type);
        if (ct != parser.get().base().end()) {
            content_type = std::string(ct->value().data(), ct->value().size());
        }
    }
    int ver = parser.get().version();
    bool hdr_done = parser.is_header_done();
    bool done = parser.is_done();

    std::cout << "is_upload=" << is_upload
              << " content_type=" << content_type
              << " content_length=" << content_length
              << " has_cl=" << has_cl
              << " version=" << ver
              << " header_done=" << hdr_done
              << " done=" << done
              << " written=" << written << std::endl;

    // ---- extension sanitisation + hash formatting from LogicSystem ----
    std::string file_name = "report.PDF";
    std::string ext;
    auto dot = file_name.rfind('.');
    if (dot != std::string::npos && dot + 1 < file_name.size()) {
        auto e = file_name.substr(dot);
        for (auto& c : e) c = (char)::tolower((unsigned char)c);
        ext = e;
    }
    unsigned long long h = 1469598103934665603ULL;
    const char* blob = "hello streaming world";
    for (const char* p = blob; *p; ++p) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
    }
    std::ostringstream hoss;
    hoss << std::hex << std::setw(16) << std::setfill('0') << h;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%lld", (long long)now);
    std::string unique = std::string(stamp) + "_" + hoss.str() + ext;
    std::cout << "unique_name=" << unique << std::endl;

    // ---- filesystem: create dir, open ofstream via wstring, rename ----
    fs::path dir = fs::temp_directory_path() / "hcat_proto_fs";
    fs::create_directories(dir);
    fs::path part = dir / "12345_0.part";
    {
        std::ofstream ofs(part.wstring(), std::ios::binary | std::ios::trunc);
        ofs.write(blob, 20);
        ofs.flush();
        if (!ofs) { std::cout << "!! ofstream write failed" << std::endl; return 1; }
    }
    fs::path finalp = dir / (std::string(stamp) + "_" + hoss.str() + ".part");
    boost::system::error_code ec;
    fs::rename(part, finalp, ec);
    std::cout << "rename ec=" << ec.message()
              << " exists=" << fs::exists(finalp)
              << " part_gone=" << !fs::exists(part) << std::endl;

    fs::remove(finalp, ec);
    fs::remove(dir, ec);
    std::cout << "=== ALL NEW API CONSTRUCTS COMPILE AND RUN ===" << std::endl;
    return 0;
}
