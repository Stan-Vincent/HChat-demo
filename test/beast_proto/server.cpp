// 原型：验证 http::buffer_body 流式读取语义 + deadline 刷新行为
// 目的：在改 GateServer 之前，用 MinGW 可编译的独立程序把 Beast 的坑踩完
// 编译：g++ -std=c++17 -I/d/software/boost_1_92_0 server.cpp -o server.exe -lws2_32 -lpsapi
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <psapi.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp = net::ip::tcp;

static const std::size_t CHUNK = 64 * 1024;   // 流式读写块
// ★ 实测结论：body_limit 在【解析请求头时】就与 Content-Length 比较
//   （basic_parser.ipp:460），超限当场失败，且它是递减预算。
//   必须在 async_read 之前设好，且 >= 允许的最大 body。
static const std::uint64_t MAX_BODY = 300ull * 1024 * 1024;
static bool g_refresh_deadline = false;        // REFRESH=1 时每块刷新 deadline
static std::size_t g_chunk_count = 0;
static std::size_t g_total_bytes = 0;

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

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket sock)
        : _socket(std::move(sock))
        , _deadline(_socket.get_executor(), std::chrono::seconds(60))
        , _scratch(CHUNK)
    {
    }

    void run()
    {
        std::cout << "[srv] connection accepted" << std::endl;
        _parser.body_limit(MAX_BODY);
        _last_activity = std::chrono::steady_clock::now();
        if (g_refresh_deadline) {
            // 修复版：周期性巡检"距上次活动的空闲时长"，而不是一次性 60s 硬截断。
            // ★ 实测结论：只在 read 完成回调里刷新是【不够的】——客户端停滞时
            //   根本没有 read 完成事件，一次性定时器照样到点杀连接。
            arm_deadline();
        } else {
            // 现状复现：accept 时构造即定 60s，且从不刷新
            _deadline.expires_after(std::chrono::seconds(60));
            _deadline.async_wait([self = shared_from_this()](beast::error_code ec) {
                if (!ec) {
                    std::cout << "[srv] *** deadline fired -> closing socket ***"
                              << std::endl;
                    beast::error_code ig;
                    self->_socket.close(ig);
                }
            });
        }
        do_read();
    }

    // 每 5s 巡检一次：空闲超过 60s 才杀连接
    void arm_deadline()
    {
        auto self = shared_from_this();
        _deadline.expires_after(std::chrono::seconds(5));
        _deadline.async_wait([self](beast::error_code ec) {
            if (ec) return;   // 已 cancel
            auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - self->_last_activity).count();
            if (idle >= 60) {
                std::cout << "[srv] *** idle " << idle
                          << "s -> closing socket ***" << std::endl;
                beast::error_code ig;
                self->_socket.close(ig);
                return;
            }
            self->arm_deadline();
        });
    }

private:
    // 把 body 指针指向 scratch，然后读一轮
    void do_read()
    {
        auto self = shared_from_this();
        _parser.get().body().data = _scratch.data();
        _parser.get().body().size = _scratch.size();
        http::async_read(_socket, _buffer, _parser,
            [self](beast::error_code ec, std::size_t n) {
                self->on_read(ec, n);
            });
    }

    void on_read(beast::error_code ec, std::size_t /*n*/)
    {
        if (ec && ec != http::error::need_buffer) {
            std::cout << "[srv] read error: " << ec.message() << std::endl;
            return;
        }

        // 头刚解析完：定路由
        if (!_header_done && _parser.is_header_done()) {
            _header_done = true;
            _target = std::string(_parser.get().target());
            std::cout << "[srv] header done: " << _parser.get().method_string()
                      << " " << _target
                      << " content_length="
                      << (_parser.content_length()
                              ? std::to_string(*_parser.content_length())
                              : std::string("none"))
                      << std::endl;
            _is_upload = (_target.rfind("/upload_file", 0) == 0);
            if (_is_upload && !_ofs.is_open()) {
                _ofs.open("proto_out.bin", std::ios::binary | std::ios::trunc);
                if (!_ofs) {
                    std::cout << "[srv] cannot open output file" << std::endl;
                    return;
                }
                std::cout << "[srv] streaming body -> proto_out.bin" << std::endl;
            }
        }

        // need_buffer 说明 scratch 被 put() 填满了：先落盘/累积，再读下一轮
        const bool need_more = (ec == http::error::need_buffer);
        std::size_t written = _scratch.size() - _parser.get().body().size;

        if (written > 0) {
            if (_is_upload) {
                _ofs.write(_scratch.data(), static_cast<std::streamsize>(written));
            } else {
                _body_str.append(_scratch.data(), written);
            }
            g_chunk_count += 1;
            g_total_bytes += written;
            if (g_chunk_count % 32 == 1) {
                std::cout << "[srv] chunk #" << g_chunk_count
                          << " bytes=" << written
                          << " total=" << g_total_bytes << std::endl;
            }
        }

        _last_activity = std::chrono::steady_clock::now();

        if (need_more) {
            do_read();
            return;
        }

        if (!_parser.is_done()) {
            // 没报错也没填满：可能只是这轮没数据，继续读
            do_read();
            return;
        }

        finish();
    }

    void finish()
    {
        if (_is_upload) {
            _ofs.flush();
            if (!_ofs) { std::cout << "[srv] write failed" << std::endl; return; }
            _ofs.close();
            std::cout << "[srv] upload complete: " << g_total_bytes
                      << " bytes in " << g_chunk_count << " chunks" << std::endl;
        } else {
            std::cout << "[srv] small body received: " << _body_str.size()
                      << " bytes -> " << _body_str << std::endl;
        }
        report_mem("after-upload");

        _response.result(http::status::ok);
        _response.set(http::field::content_type, "text/json");
        beast::ostream(_response.body())
            << "{\"error\":0,\"size\":" << g_total_bytes << "}";
        write_response();
    }

    void write_response()
    {
        auto self = shared_from_this();
        _response.content_length(_response.body().size());
        http::async_write(_socket, _response,
            [self](beast::error_code ec, std::size_t) {
                if (ec) {
                    std::cout << "[srv] write error: " << ec.message() << std::endl;
                    return;
                }
                std::cout << "[srv] response sent" << std::endl;
                beast::error_code ig;
                self->_socket.shutdown(tcp::socket::shutdown_send, ig);
                self->_deadline.cancel();
            });
    }

    tcp::socket _socket;
    beast::flat_buffer _buffer{ 8192 };
    http::request_parser<http::buffer_body> _parser;
    http::request<http::buffer_body> _request;
    http::response<http::dynamic_body> _response;
    net::steady_timer _deadline;
    std::vector<char> _scratch;
    std::ofstream _ofs;
    std::string _body_str;
    std::string _target;
    bool _header_done = false;
    bool _is_upload = false;
    std::chrono::steady_clock::time_point _last_activity{};
};

int main(int argc, char** argv)
{
    unsigned short port = 18080;
    if (argc > 1) port = static_cast<unsigned short>(std::atoi(argv[1]));
    if (std::getenv("REFRESH")) g_refresh_deadline = true;
    std::cout << "[srv] listening on " << port
              << (g_refresh_deadline ? " (deadline refresh ON)" : " (deadline refresh OFF)")
              << std::endl;

    net::io_context ioc{ 1 };
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
