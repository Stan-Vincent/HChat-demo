// Mock GateServer：把 LogicSystem::PrepareUpload/FinishUpload 的逻辑 1:1 搬到
// MinGW 可编译的独立进程里，用来在没有 VS2022 的环境下对真实 Qt 客户端做端到端验证。
//
// 与真实 GateServer 的对应关系（改一边必须同步改另一边）：
//   常量      kMaxUploadBytes / kMaxJsonBodyBytes / kBodyChunkSize
//   路由      POST /upload_image|/upload_file?uid=&file_name=
//   拒绝      ① content-type 含 json ② 缺 file_name ③ Content-Length 超限
//             ④ 实际写入量超限（chunked 兜底） ⑤ 空文件 ⑥ 写盘失败
//   落盘      <stamp>_<seq>.part -> FNV-1a -> <stamp>_<hash><ext>
//   响应      {error, url, unique_name, size, md5}
//   行为      拒绝时先排空 body 再响应（客户端才读得到错误）
//
// 编译：g++ -std=c++17 -O2 -I/d/software/boost_1_92_0 mock_gate.cpp -o mock_gate.exe -lws2_32 -lpsapi
// 运行：./mock_gate.exe 18080 ./mock_upload
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <psapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp = net::ip::tcp;

// ---- 与 GateServer/HttpConnection.h 保持一致 ----
static const std::size_t kBodyChunkSize    = 64 * 1024;
static const std::uint64_t kMaxUploadBytes = 300ull * 1024 * 1024;
static const std::size_t   kMaxJsonBytes   = 1024 * 1024;
static const int           kIdleTimeoutSec = 60;
static const int           kPatrolSec      = 5;
// 与真实 GateServer 一致：最低速率 / 单次上传总时长 / 连续低速几次才断开。
// ★ 只靠 max_upload 挡不住"慢慢传"：一个稳定以 20KB/s 传的对端每 5 秒都有
//   数据，永远不会 idle，300MB 能拖 4 小时。这两道闸门就是补这个洞的。
static const std::uint64_t kMinUploadBps  = 64 * 1024;
static const int           kMaxUploadDur  = 600;
static const int           kSlowPatrols   = 3;

// 默认等于 kMaxUploadBytes（与真实服务端一致）；测试时可用第 3 个 argv 覆盖，
// 这样就不用真传 300MB 来验证超限拒绝路径。
static std::uint64_t g_max_upload = kMaxUploadBytes;
static std::uint64_t g_min_bps    = kMinUploadBps;
static int           g_max_dur    = kMaxUploadDur;

static std::string g_out_dir = "mock_upload";
static std::atomic<unsigned> g_seq{ 0 };

static void report_mem(const char* tag)
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        std::cout << "[mem] " << tag
                  << " WorkingSet=" << (pmc.WorkingSetSize / 1024) << " KB"
                  << " PeakWorkingSet=" << (pmc.PeakWorkingSetSize / 1024) << " KB"
                  << std::endl;
    }
}

static std::string url_decode(const std::string& s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int h = hex(s[i + 1]), l = hex(s[i + 2]);
            if (h >= 0 && l >= 0) { out.push_back(static_cast<char>(h * 16 + l)); i += 2; continue; }
        }
        if (s[i] == '+') { out.push_back(' '); continue; }
        out.push_back(s[i]);
    }
    return out;
}

// 解析 ?uid=2&file_name=a%20b.png
static std::string query_param(const std::string& target, const std::string& key)
{
    auto q = target.find('?');
    if (q == std::string::npos) return std::string();
    std::string qs = target.substr(q + 1);
    std::size_t pos = 0;
    while (pos < qs.size()) {
        auto amp = qs.find('&', pos);
        std::string pair = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        auto eq = pair.find('=');
        if (eq != std::string::npos && url_decode(pair.substr(0, eq)) == key) {
            return url_decode(pair.substr(eq + 1));
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return std::string();
}

static std::string sanitize_ext(const std::string& name)
{
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size()) return std::string();
    std::string e = name.substr(dot);
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    static const char* blocked[] = { ".exe", ".bat", ".cmd", ".com", ".scr", ".pif",
        ".vbs", ".vbe", ".js", ".jse", ".wsf", ".wsh", ".ps1", ".msi", ".dll",
        ".jar", ".php", ".sh" };
    for (auto b : blocked) if (e == b) return ".bin";
    return e;
}

static std::string now_stamp()
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld_%u", (long long)ms, g_seq.fetch_add(1));
    return buf;
}

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket sock)
        : _socket(std::move(sock))
        , _deadline(_socket.get_executor(), std::chrono::seconds(kPatrolSec))
        , _scratch(kBodyChunkSize)
    {
    }

    void run()
    {
        _parser.body_limit(1024ull * 1024 * 1024);   // 与真实服务端同一策略：故意放宽
        _last_activity = std::chrono::steady_clock::now();
        arm_deadline();
        do_read();
    }

    // 周期巡检：空闲超过 kIdleTimeoutSec 才关连接。
    // ★ 这是真实 GateServer 的修复版行为，一次性定时器会误杀慢速上传。
    void arm_deadline()
    {
        auto self = shared_from_this();
        _deadline.expires_after(std::chrono::seconds(kPatrolSec));
        _deadline.async_wait([self](beast::error_code ec) {
            if (ec) return;
            auto idle = std::chrono::steady_clock::now() - self->_last_activity;
            if (idle >= std::chrono::seconds(kIdleTimeoutSec)) {
                std::cout << "[srv] idle timeout, closing" << std::endl;
                beast::error_code ig;
                self->_socket.close(ig);
                return;
            }

            // ② 慢速闸门
            if (self->_is_upload && !self->_drain) {
                auto rate = self->_bytes_since_patrol / kPatrolSec;
                if (rate < g_min_bps) {
                    if (++self->_slow_patrols >= kSlowPatrols) {
                        std::cout << "[srv] too slow: " << rate << " B/s < "
                                  << g_min_bps << " B/s x" << kSlowPatrols
                                  << ", closing" << std::endl;
                        beast::error_code ig;
                        self->_socket.close(ig);
                        return;
                    }
                } else {
                    self->_slow_patrols = 0;
                }
            }
            self->_bytes_since_patrol = 0;

            // ③ 总时长硬上限
            if (self->_is_upload && self->_upload_start.time_since_epoch().count() != 0) {
                auto el = std::chrono::steady_clock::now() - self->_upload_start;
                if (el >= std::chrono::seconds(g_max_dur)) {
                    std::cout << "[srv] upload exceeded " << g_max_dur
                              << "s, closing" << std::endl;
                    beast::error_code ig;
                    self->_socket.close(ig);
                    return;
                }
            }
            self->arm_deadline();
        });
    }

    void do_read()
    {
        auto self = shared_from_this();
        _parser.get().body().data = _scratch.data();
        _parser.get().body().size = _scratch.size();
        http::async_read(_socket, _buffer, _parser,
            [self](beast::error_code ec, std::size_t n) { self->on_read(ec, n); });
    }

    // 对应 LogicSystem::PrepareUpload：只填 _response，不写回。
    // ★ 与真实服务端一致：拒绝时调用方必须先排空 body 再写响应，
    //   否则还在上传的客户端只会看到 RST 而不是这段 JSON。
    bool prepare_upload()
    {
        _response.result(http::status::ok);
        _response.set(http::field::content_type, "text/json");
        if (_content_type.find("json") != std::string::npos) {
            fill_json_error("expected a raw binary body, not application/json");
            return false;
        }
        if (_file_name.empty()) {
            fill_json_error("missing file_name");
            return false;
        }
        if (_has_len && _content_len > g_max_upload) {
            fill_json_error("file too large (max 300MB)");
            return false;
        }
        _ext = sanitize_ext(_file_name);
        _path = g_out_dir + "/" + _stamp + ".part";
        _ofs.open(_path, std::ios::binary | std::ios::trunc);
        if (!_ofs) {
            json_error("cannot open output file");
            return false;
        }
        return true;
    }

    // 对应 LogicSystem::FinishUpload
    void finish_upload()
    {
        _ofs.flush();
        if (!_ofs) { json_error("disk write failed"); return; }
        _ofs.close();

        if (_written == 0) { json_error("empty file"); return; }
        if (_has_len && _written != _content_len) { json_error("truncated upload"); return; }

        std::ostringstream hs;
        hs << std::hex << std::setw(16) << std::setfill('0') << _hash;
        auto hash = hs.str();

        // 最终名 = <时间戳>_<FNV-1a><扩展名>，内容寻址
        auto unique = _stamp.substr(0, _stamp.find('_')) + "_" + hash + _ext;
        auto final_path = g_out_dir + "/" + unique;
        std::error_code fec;
        std::filesystem::rename(_path, final_path, fec);
        if (fec) {
            std::cout << "[srv] rename failed: " << fec.message() << std::endl;
            unique = _path.substr(_path.find_last_of('/') + 1);
        }
        _finished = true;

        std::ostringstream os;
        os << "{\"error\":0,\"url\":\"upload/" << unique
           << "\",\"unique_name\":\"" << unique
           << "\",\"size\":" << _written
           << ",\"md5\":\"" << hash << "\"}";
        _response.set(http::field::content_type, "text/json");
        beast::ostream(_response.body()) << os.str();
        std::cout << "[srv] upload ok: " << unique << " " << _written << " bytes" << std::endl;
        report_mem("after-upload");
        write_response();
    }

    // 只填充响应体、不写回（对应真实服务端 WriteJsonError 的前半段，
    // 也是 PrepareUpload 失败时的行为）
    // 对应 LogicSystem::HandleGet 的 /upload/<file> 分支：
    // 挡住 ".." / 分隔符，别让 /upload/../../config.ini 这类请求读到别处。
    void handle_get_upload(const std::string& name)
    {
        if (name.empty() || name.find("..") != std::string::npos
            || name.find('/') != std::string::npos
            || name.find('\\') != std::string::npos) {
            _response.result(http::status::bad_request);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "bad file name";
            write_response();
            return;
        }
        auto abs = g_out_dir + "/" + name;
        std::ifstream ifs(abs, std::ios::binary);
        if (!ifs) {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "file not found";
            write_response();
            return;
        }
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        _response.result(http::status::ok);
        _response.set(http::field::content_type, "application/octet-stream");
        _response.content_length(content.size());
        beast::ostream(_response.body()).write(content.data(),
                                               static_cast<std::streamsize>(content.size()));
        std::cout << "[srv] GET " << name << " " << content.size() << " bytes" << std::endl;
        write_response();
    }

    void fill_json_error(const char* msg)
    {
        _response.result(http::status::ok);
        _response.keep_alive(false);
        _response.set(http::field::content_type, "text/json");
        std::ostringstream os;
        os << "{\"error\":" << Error_Json << ",\"msg\":\"" << msg << "\"}";
        beast::ostream(_response.body()) << os.str();
    }

    // 填充并立即写回
    void json_error(const char* msg)
    {
        fill_json_error(msg);
        write_response();
    }

    void on_read(beast::error_code ec, std::size_t /*n*/)
    {
        _last_activity = std::chrono::steady_clock::now();

        if (ec == http::error::body_limit) {
            std::cout << "[srv] body_limit exceeded, rejecting" << std::endl;
            json_error("request body too large");
            return;
        }
        if (ec && ec != http::error::need_buffer) {
            std::cout << "[srv] read error: " << ec.message() << std::endl;
            return;
        }

        if (!_header_done && _parser.is_header_done()) {
            _header_done = true;
            auto target = _parser.get().target();
            std::string t(target.data(), target.size());
            auto ct = _parser.get().base().find(http::field::content_type);
            if (ct != _parser.get().base().end()) {
                _content_type = std::string(ct->value().data(), ct->value().size());
            }
            std::cout << "[srv] " << _parser.get().method_string() << " " << t
                      << " ct=" << _content_type
                      << " len=" << (_parser.content_length()
                              ? std::to_string(*_parser.content_length()) : std::string("none"))
                      << std::endl;

            if (_parser.get().method() == http::verb::get && t.rfind("/upload/", 0) == 0) {
                _is_download = true;
                _down_name = t.substr(std::strlen("/upload/"));
            }
            else if (_parser.get().method() == http::verb::post &&
                (t.rfind("/upload_image", 0) == 0 || t.rfind("/upload_file", 0) == 0)) {
                _is_upload = true;
                if (_parser.content_length()) {
                    _has_len = true;
                    _content_len = *_parser.content_length();
                }
                _file_name = query_param(t, "file_name");
                _stamp = now_stamp();
                if (!prepare_upload()) {
                    // ★ 与真实 GateServer 一致：先排空 body 再回错误
                    _is_upload = false;
                    _drain = true;
                } else {
                    _upload_start = std::chrono::steady_clock::now();
                }
            }
        }

        std::size_t written = _scratch.size() - _parser.get().body().size;
        if (written > 0) {
            if (_drain) {
                // 丢弃
            } else if (_is_upload) {
                if (_written + written > g_max_upload) {
                    std::cout << "[srv] mid-flight over limit, aborting" << std::endl;
                    if (_ofs.is_open()) _ofs.close();
                    std::error_code fec;
                    std::filesystem::remove(_path, fec);
                    _path.clear();
                    json_error("file too large (max 300MB)");
                    return;
                }
                _ofs.write(_scratch.data(), static_cast<std::streamsize>(written));
                if (!_ofs) {
                    std::cout << "[srv] disk write failed" << std::endl;
                    if (_ofs.is_open()) _ofs.close();
                    std::error_code fec;
                    std::filesystem::remove(_path, fec);
                    _path.clear();
                    json_error("disk write failed");
                    return;
                }
                _written += written;
                _bytes_since_patrol += written;
                for (std::size_t i = 0; i < written; ++i) {
                    _hash ^= static_cast<unsigned char>(_scratch[i]);
                    _hash *= 1099511628211ULL;
                }
            } else {
                _body_str.append(_scratch.data(), written);
                if (_body_str.size() > kMaxJsonBytes) {
                    json_error("request body too large");
                    return;
                }
            }
        }

        if (_drain) {
            if (!_parser.is_done()) { do_read(); return; }
            _response.keep_alive(false);
            write_response();
            return;
        }

        if (ec == http::error::need_buffer) { do_read(); return; }
        if (!_parser.is_done()) { do_read(); return; }

        if (_is_download) handle_get_upload(_down_name);
        else if (_is_upload) finish_upload();
        else json_error("not implemented in mock");
    }

    void write_response()
    {
        auto self = shared_from_this();
        _response.version(11);
        _response.keep_alive(false);
        _response.content_length(_response.body().size());
        http::async_write(_socket, _response,
            [self](beast::error_code ec, std::size_t) {
                if (ec) { std::cout << "[srv] write error: " << ec.message() << std::endl; return; }
                beast::error_code ig;
                self->_socket.shutdown(tcp::socket::shutdown_send, ig);
                self->_deadline.cancel();
            });
    }

    ~Session()
    {
        if (!_path.empty() && !_finished) {
            if (_ofs.is_open()) _ofs.close();
            std::error_code fec;
            std::filesystem::remove(_path, fec);
        }
    }

private:
    static constexpr int Error_Json = 1001;   // 与 GateServer/const.h 一致
    bool _finished = false;

    tcp::socket _socket;
    beast::flat_buffer _buffer{ 8192 };
    http::request_parser<http::buffer_body> _parser;
    http::response<http::dynamic_body> _response;
    net::steady_timer _deadline;
    std::vector<char> _scratch;

    std::string _body_str;
    std::string _target;
    std::string _content_type;
    std::string _file_name;
    std::string _ext;
    std::string _path;
    std::string _stamp;
    std::ofstream _ofs;
    bool _header_done = false;
    bool _is_download = false;
    std::string _down_name;
    bool _is_upload = false;
    bool _drain = false;
    bool _has_len = false;
    std::uint64_t _content_len = 0;
    std::uint64_t _written = 0;
    std::uint64_t _bytes_since_patrol = 0;
    int           _slow_patrols = 0;
    std::chrono::steady_clock::time_point _upload_start{};
    unsigned long long _hash = 1469598103934665603ULL;
    std::chrono::steady_clock::time_point _last_activity{};
};

int main(int argc, char** argv)
{
    unsigned short port = 18080;
    if (argc > 1) port = static_cast<unsigned short>(std::atoi(argv[1]));
    if (argc > 2) g_out_dir = argv[2];
    if (argc > 3) g_max_upload = std::strtoull(argv[3], nullptr, 10);
    if (argc > 4) g_min_bps    = std::strtoull(argv[4], nullptr, 10);
    if (argc > 5) g_max_dur    = std::atoi(argv[5]);
    std::error_code fec;
    std::filesystem::create_directories(g_out_dir, fec);
    std::cout << "[srv] mock gate listening on " << port
              << ", upload dir " << g_out_dir << ", max upload " << g_max_upload
              << ", min bps " << g_min_bps << ", max dur " << g_max_dur << "s" << std::endl;

    net::io_context ioc{ 2 };
    tcp::acceptor acceptor{ ioc, tcp::endpoint(net::ip::make_address("127.0.0.1"), port) };
    std::function<void()> do_accept;
    do_accept = [&]() {
        acceptor.async_accept([&](beast::error_code ec, tcp::socket sock) {
            if (!ec) std::make_shared<Session>(std::move(sock))->run();
            do_accept();
        });
    };
    do_accept();
    ioc.run();
    report_mem("server-exit");
    return 0;
}
