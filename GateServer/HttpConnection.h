#pragma once
#include "const.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// ---- 流式上传相关参数（设计原理见 HttpConnection.cpp） ----
// 请求体每次拷入的块大小，落盘也按这个粒度写。
constexpr std::size_t kBodyChunkSize = 64 * 1024;
// 解析器级别的兜底上限，仅此而已。Beast 在解析请求头时就把 body_limit 和声明
// 的 Content-Length 做比较（basic_parser.ipp），而且它是递减的预算而不是累计
// 值，所以必须在 async_read 之前装好。这里故意放宽：真正决定收不收的是下面两
// 个业务上限，而放宽解析器上限有一个具体好处——被拒绝的上传还能把 body 排空，
// 客户端才读得到错误响应。Beast 对请求的默认值只有 1MB，原来连正常请求都会被
// 悄悄拒掉。
constexpr std::uint64_t kMaxHttpBodyBytes = 1024ull * 1024 * 1024;
// 非上传路由的真实上限（JSON 负载）。解析器上限放宽之后，原来 Beast 默认
// 1MB 的紧箍咒挪到这里来戴。
constexpr std::size_t kMaxJsonBodyBytes = 1024 * 1024;
// 流式上传的真实上限，两处共同把关：PrepareUpload() 里校验声明的
// Content-Length，OnReadBody() 里校验实际写出的字节数，这样不带长度的
// chunked 上传也绕不过去。
constexpr std::uint64_t kMaxUploadBytes = 300ull * 1024 * 1024;
// 连接多久没有任何活动就判定为死亡。
constexpr int kIdleTimeoutSec = 60;
// 空闲巡检每隔多久复查一次。
constexpr int kPatrolIntervalSec = 5;

// 单次上传的【最低速率】（字节/秒）。低于它连续 kSlowPatrolLimit 次巡检就判定为
// 慢速上传并断开。
// ★ 为什么光有 kMaxUploadBytes 不够：那个只挡住了"文件多大"，挡不住"传多慢"。
//   300MB 用 20KB/s 要拖 4 小时，而它每 5 秒都有数据进来，永远不会触发
//   kIdleTimeoutSec —— 连接、文件句柄、磁盘上的 .part 全程占着。这就是慢速攻击面。
constexpr std::uint64_t kMinUploadBytesPerSec = 64 * 1024;
// 单次上传的【总时长硬上限】（秒）。与最低速率互为补充：
//   最低速率抓"稳定慢"，总时长抓"总量大 + 中等慢"
//   （比如 300MB 用 600KB/s 传 8 分钟，速率没低到被掐，但拖得太久）。
constexpr int kMaxUploadDurationSec = 600;
// 连续多少次巡检都低于最低速率才断开。判一次就杀太脆，网络抖一下就误杀了。
constexpr int kSlowPatrolLimit = 3;

// 上传相关的运行时上限。默认值就是上面这组常量；进程启动后由 InitUploadLimits()
// 从 config.ini 的 [Upload] 段覆盖（调用点在 GateServer.cpp 的 main 里）。
// 为什么要做成运行时可配：改上限不该重新编译；而且客户端和服务端各有一份
// config.ini，两边必须配成同一个值，写死在代码里特别容易忘记同步。
struct UploadLimits {
    std::uint64_t max_bytes         = kMaxUploadBytes;
    std::uint64_t min_bytes_per_sec = kMinUploadBytesPerSec;
    int           max_duration_sec  = kMaxUploadDurationSec;
};
const UploadLimits& UploadCfg();
void InitUploadLimits();

class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
public:
    friend class LogicSystem;
	HttpConnection(boost::asio::io_context& ioc);
	~HttpConnection();
	//启动连接：装 body_limit、启动空闲巡检、开始读请求
	//启动连接：装 body_limit、启动空闲巡检、开始读请求
	//启动连接：装 body_limit、启动空闲巡检、开始读请求
	void Start();
    tcp::socket& GetSocket();

    // ---- 流式上传状态，与 LogicSystem 共享 ----
    // 由 HttpConnection 在请求头解析完成后填充，LogicSystem::PrepareUpload()
    // 读取、LogicSystem::FinishUpload() 消费。
    bool          _is_upload = false;   // POST /upload_image|/upload_file 为真
    bool          _upload_finished = false;   // 上传是否已成功收尾
    bool          _has_content_length = false;
    std::uint64_t _content_length = 0;
    std::string   _content_type;
    std::string   _upload_file_name;    // 查询串里的原始文件名
    std::string   _upload_ext;          // 消毒后的扩展名
    std::string   _upload_unique_name;  // 落盘后的最终文件名
    std::string   _upload_path;         // 磁盘上的当前路径（.part 或最终名）
    std::ofstream _upload_ofs;          // 流式输出文件
    std::uint64_t _upload_written = 0;  // 已写入的字节数
    // FNV-1a 滚动哈希状态，每块更新一次，这样不用把整个文件留在内存里，
    // 也能在最后算出内容哈希。
    unsigned long long _upload_hash = 1469598103934665603ULL;

    // 非上传路由累积的请求体（很小的 JSON 负载）。
    // 取代原来的 buffers_to_string(_request.body().data())。
    std::string _body_str;

private:
    // 周期性的空闲巡检。accept 时一次性 armed 的定时器会误杀“慢但在推进”的
    // 上传，所以改成按间隔反复复查。
    void ArmDeadline();

    //把处理好的响应异步发回客户端，发完后关闭连接的发送端，并取消超时定时器
    void WriteResponse();

    // 用 JSON 错误体填充 _response 并发送。流式读循环里的每条失败路径都走这里。
    void WriteJsonError(const char* msg);

    //从 HTTP 请求的 URI 里，切分出路径和查询参数，存到成员变量 _get_url 和 _get_params
    void PreParseGetParam();

    //把完整收到的请求交给 LogicSystem 分发处理
    void HandleReq();

    // 流式读循环
    void DoReadBody();
    void OnReadBody(beast::error_code ec, std::size_t bytes_transferred);
    void FinishMessage();

    //套接字
    //套接字
    //套接字
    tcp::socket _socket;

    //_buffer 用来接受数据
    beast::flat_buffer  _buffer{ 8192 };

    // buffer_body 让请求体可以一小块一小块地消费，而不是整个塞进内存：
    // body().data / body().size 必须在每次 async_read 之前重新装填，
    // 目标缓冲区满了的时候解析器会报 need_buffer。
    http::request_parser<http::buffer_body> _parser;

    // _response 用来回应客户端 在HandleReq()里填充内容
    http::response<http::dynamic_body> _response;

    //_deadline 用来做定时器判断请求是否超时
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(kPatrolIntervalSec)
    };
    std::chrono::steady_clock::time_point _last_activity{};

    // 巡检算速率用：自上次巡检以来收到的字节数，以及连续低速的次数
    std::uint64_t _bytes_since_patrol = 0;
    int           _slow_patrols = 0;
    // 上传真正开始的时刻（PrepareUpload 成功时打点），用于总时长硬上限
    std::chrono::steady_clock::time_point _upload_start{};


    // 解析器把请求体字节拷入的目标缓冲区。
    std::vector<char> _scratch;
    bool _header_done = false;
    // 上传路由被拒绝（PrepareUpload 返回 false）、但请求体还没收完时置位。
    // 这时继续把 body 读出来扔掉，等解析器跑完再写错误响应：立刻回复会和
    // 客户端赛跑——它还在忙着发文件，大概率只会看到一个 reset 而不是 JSON。
    bool _drain_mode = false;

    //get请求的url
	std::string _get_url;

    // ★ 存放静态资源（upload 下的文件）的二进制内容。
    //   必须放在成员里：beast::buffers_body 持有的是指针，
    //   如果指向 handler 里的局部变量，返回后数据就没了，客户端会收到乱码。
    std::string _file_body;

    //get请求的url的参数 结构:key->value
    std::unordered_map<std::string, std::string> _get_params;
};
