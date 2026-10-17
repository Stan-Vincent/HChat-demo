#include "HttpConnection.h"
#include "LogicSystem.h"
#include "ConfigMgr.h"
#include <string>
#include "logger.h"

// ===================== 上传上限的运行时配置 =====================
// 默认值 = HttpConnection.h 里那组 constexpr；config.ini 里有 [Upload] 段就覆盖。
//
// 用"文件内静态 + 显式 Init()"而不是全局对象：ConfigMgr::Inst() 自己也是函数内
// 静态（见 ConfigMgr.cpp），全局对象的初始化顺序跨编译单元是没有保证的；而
// InitUploadLimits() 是在 main 里显式调用的，那时 ConfigMgr 一定已经构造好了。
static UploadLimits g_upload_limits;

const UploadLimits& UploadCfg() { return g_upload_limits; }

void InitUploadLimits() {
    // ★ ConfigMgr::operator[] 是【按值】返回 SectionInfo（不是引用），
    //   这里必须按值接住；写成 auto& 会报 C2440 无法把临时对象绑到非 const 引用。
    //   section 不存在时它返回空的 SectionInfo，下面的 cfg[key] 会拿到 ""，
    //   于是全部落到默认值 —— 没配 [Upload] 段也能正常启动。
    auto cfg = ConfigMgr::Inst()["Upload"];
    auto get_u64 = [&](const char* key, std::uint64_t def) -> std::uint64_t {
        auto v = cfg[key];
        if (v.empty()) return def;
        try { return std::stoull(v); }
        catch (const std::exception&) {
            LOG_ERROR("[Upload] {} 不是合法数字，用默认值 {}", key, (long long)def);
            return def;
        }
    };
    auto get_int = [&](const char* key, int def) -> int {
        auto v = cfg[key];
        if (v.empty()) return def;
        try { return std::stoi(v); }
        catch (const std::exception&) {
            LOG_ERROR("[Upload] {} 不是合法数字，用默认值 {}", key, def);
            return def;
        }
    };
    g_upload_limits.max_bytes         = get_u64("MaxBytes", kMaxUploadBytes);
    g_upload_limits.min_bytes_per_sec = get_u64("MinBytesPerSec", kMinUploadBytesPerSec);
    g_upload_limits.max_duration_sec  = get_int("MaxDurationSec", kMaxUploadDurationSec);

    // ★ 硬约束：上传上限不能超过 Beast 的解析器上限 kMaxHttpBodyBytes。
    //   Beast 在解析请求头时拿声明的 Content-Length 跟 body_limit 比，一旦超限
    //   直接报 http::error::body_limit —— body 根本不会被排空，客户端就只能看到
    //   连接重置，而不是那句可读的 JSON 错误（这正是流式改造要修掉的老毛病）。
    //   所以这里宁可悄悄夹到解析器上限，也不能让它反过来。
    if (g_upload_limits.max_bytes > kMaxHttpBodyBytes) {
        LOG_ERROR("[Upload] MaxBytes 超过解析器上限 {}，已夹到 {}",
                  (long long)g_upload_limits.max_bytes, (long long)kMaxHttpBodyBytes);
        g_upload_limits.max_bytes = kMaxHttpBodyBytes;
    }
    LOG_INFO("上传上限: {} 字节, 最低速率 {} B/s, 最长 {} 秒",
             (long long)g_upload_limits.max_bytes,
             (long long)g_upload_limits.min_bytes_per_sec,
             g_upload_limits.max_duration_sec);
}

HttpConnection::HttpConnection(boost::asio::io_context& ioc)
    : _socket(ioc), _scratch(kBodyChunkSize) {
}

HttpConnection::~HttpConnection()
{
    // 中断的流式上传不能留下 .part 文件：否则每掉一个连接，
    // upload/ 目录就会慢慢堆满垃圾。
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
    // ★ 显式设置 body_limit。
    //   Beast 在解析请求头时就把 body_limit 和声明的 Content-Length 做比较
    //   （boost/beast/http/impl/basic_parser.ipp），而且它是递减的预算而不是
    //   累计值，所以必须在第一次 async_read 之前装好，且要能覆盖我们接受的
    //   最大请求体。Beast 对请求的默认值只有 1MB，比它大的都会被悄悄拒掉。
    _parser.body_limit(kMaxHttpBodyBytes);

    _last_activity = std::chrono::steady_clock::now();
    ArmDeadline();
    DoReadBody();
}

void HttpConnection::ArmDeadline()
{
    auto self = shared_from_this();
    // 按间隔巡检，只有连续 kIdleTimeoutSec 没有任何活动才关闭连接。
    // 光在读回调里刷新是不够的：卡住的对端根本不会产生读完成事件，
    // 所以 accept 时一次性 armed 的定时器依然会误杀“慢但在推进”的上传。
    deadline_.expires_after(std::chrono::seconds(kPatrolIntervalSec));
    deadline_.async_wait([self](beast::error_code ec) {
        if (ec) {
            return;   // cancelled
        }
        auto idle = std::chrono::steady_clock::now() - self->_last_activity;
        if (idle >= std::chrono::seconds(kIdleTimeoutSec)) {
            LOG_INFO("连接空闲 {} 秒，关闭", kIdleTimeoutSec);
            beast::error_code ig;
            self->_socket.close(ig);
            return;
        }

        // ② 慢速：这一段巡检周期里的平均速率。
        //    ★ kMaxUploadBytes 只挡住了"文件多大"，挡不住"传多慢"：
        //      一个稳定以 20KB/s 传的对端每 5 秒都有数据，永远不会 idle，
        //      300MB 能拖 4 小时，连接、文件句柄、磁盘上的 .part 全程占着。
        if (self->_is_upload && !self->_drain_mode) {
            auto rate = self->_bytes_since_patrol / kPatrolIntervalSec;
            if (rate < UploadCfg().min_bytes_per_sec) {
                if (++self->_slow_patrols >= kSlowPatrolLimit) {
                    LOG_ERROR("上传速率 {} B/s 低于下限 {} B/s（连续 {} 次），断开",
                              (long long)rate,
                              (long long)UploadCfg().min_bytes_per_sec,
                              kSlowPatrolLimit);
                    beast::error_code ig;
                    self->_socket.close(ig);
                    return;
                }
            }
            else {
                self->_slow_patrols = 0;
            }
        }
        self->_bytes_since_patrol = 0;

        // ③ 总时长硬上限：抓"总量大 + 中等慢"——速率没低到被 ② 掐掉，
        //   但一个上传拖了十几分钟，同样是在白占连接。
        if (self->_is_upload && self->_upload_start.time_since_epoch().count() != 0) {
            auto elapsed = std::chrono::steady_clock::now() - self->_upload_start;
            if (elapsed >= std::chrono::seconds(UploadCfg().max_duration_sec)) {
                LOG_ERROR("单次上传耗时超过 {} 秒，断开", UploadCfg().max_duration_sec);
                beast::error_code ig;
                self->_socket.close(ig);
                return;
            }
        }

        self->ArmDeadline();
    });
}

void HttpConnection::DoReadBody()
{
    auto self = shared_from_this();
    // buffer_body 把请求体字节拷入调用方给的缓冲区，
    // 所以目标缓冲区必须在每次读之前重新装填。
    _parser.get().body().data = _scratch.data();
    _parser.get().body().size = _scratch.size();

    http::async_read(_socket, _buffer, _parser,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            self->OnReadBody(ec, bytes_transferred);
        });
}

// 每条失败路径都用 HTTP 200 + JSON 里的错误码来回复。这是客户端一直在解析的
// 形状，所以被拒的上传会走正常的 ErrorCodes 分支上报，而不是依赖客户端去检查
// HTTP 状态码。
void HttpConnection::WriteJsonError(const char* msg) {
    _response.version(11);
    _response.keep_alive(false);
    _response.result(http::status::ok);
    _response.set(http::field::content_type, "text/json");
    Json::Value root;
    root["error"] = ErrorCodes::Error_Json;
    root["msg"] = msg;
    beast::ostream(_response.body()) << root.toStyledString();
    WriteResponse();
}

void HttpConnection::OnReadBody(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    _last_activity = std::chrono::steady_clock::now();

    if (ec == http::error::body_limit) {
        // 声明的 Content-Length 超过了解析器兜底上限。此时解析器已进入失败
        // 状态，没法把剩下的 body 排空；只能立即回复，并接受一个现实：还在发
        // 数据的对端可能看到的是 reset 而不是这个响应。
        LOG_ERROR("请求体超过解析器兜底上限，已拒绝");
        WriteJsonError("request body too large");
        return;
    }

    if (ec && ec != http::error::need_buffer) {
        LOG_ERROR("HTTP 读取错误: {}", ec.what());
        return;
    }

    // 请求头总是在第一个 body 字节交付之前就解析完了，所以这个分支只会执行
    // 一次：在这里选定路由，如果是上传，就在任何数据块到达之前把输出文件打开。
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
            auto ct = _parser.get().base().find(http::field::content_type);
            if (ct != _parser.get().base().end()) {
                _content_type = std::string(ct->value().data(), ct->value().size());
            }
            if (!LogicSystem::GetInstance()->PrepareUpload(shared_from_this())) {
                // _response 里已经装好了 JSON 错误。继续读并扔掉，
                // 等对端把数据发完，这样它才读得到错误，
                // 而不是和一个 reset 赛跑。
                _is_upload = false;
                _drain_mode = true;
            }
            else {
                // 上传真正开始的时刻，用来算总时长硬上限
                _upload_start = std::chrono::steady_clock::now();
            }
        }
    }

    // 本轮有多少 body 字节落进了 scratch 缓冲区。解析器一边拷贝，
    // 一边推进 body().data 并递减 body().size。
    std::size_t written = _scratch.size() - _parser.get().body().size;
    if (written > 0) {
        if (_drain_mode) {
            // 请求已经被拒：这些字节直接扔掉。
        }
        else if (_is_upload) {
            // 兜底：不带 Content-Length 的 chunked 上传，PrepareUpload() 里
            // 那个声明式检查看不到，所以等实际写入量一超上限就立刻掐断。
            if (_upload_written + written > UploadCfg().max_bytes) {
                LOG_ERROR("流式上传中途超过 {} 字节，中止",
                          (long long)UploadCfg().max_bytes);
                if (_upload_ofs.is_open()) {
                    _upload_ofs.close();
                }
                boost::system::error_code fec;
                boost::filesystem::remove(_upload_path, fec);
                _upload_path.clear();
                WriteJsonError(("file too large (max " +
                                std::to_string(UploadCfg().max_bytes / (1024 * 1024)) +
                                "MB)").c_str());
                return;
            }
            _upload_ofs.write(_scratch.data(), static_cast<std::streamsize>(written));
            if (!_upload_ofs) {
                LOG_ERROR("上传写盘失败");
                if (_upload_ofs.is_open()) {
                    _upload_ofs.close();
                }
                boost::system::error_code fec;
                boost::filesystem::remove(_upload_path, fec);
                _upload_path.clear();
                WriteJsonError("disk write failed");
                return;
            }
            _upload_written += written;
            // 巡检算速率用：这段时间内实际落盘的字节数
            _bytes_since_patrol += written;
            // FNV-1a 增量更新，这样不用把整个文件留在内存里，
            // 到最后也能拿到内容哈希。
            for (std::size_t i = 0; i < written; ++i) {
                _upload_hash ^= static_cast<unsigned char>(_scratch[i]);
                _upload_hash *= 1099511628211ULL;
            }
        }
        else {
            _body_str.append(_scratch.data(), written);
            // 非上传路由只承载很小的 JSON 负载。解析器上限现在是故意放宽的，
            // 所以紧的闸门改在这里。
            if (_body_str.size() > kMaxJsonBodyBytes) {
                LOG_ERROR("非上传路由请求体超过 {} 字节，已拒绝",
                          kMaxJsonBodyBytes);
                WriteJsonError("request body too large");
                return;
            }
        }
    }

    if (_drain_mode) {
        // 等对端把整个 body 发完再回复。
        if (!_parser.is_done()) {
            DoReadBody();
            return;
        }
        _response.keep_alive(false);
        WriteResponse();
        return;
    }

    if (ec == http::error::need_buffer) {
        // scratch 缓冲区满了，而且这一块已经在上面落盘/累积过了：
        // 重新装填，读下一块。
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
    // 上传路由不走 HandleReq()：请求体已经被流式处理完了，
    // 这里只负责收尾（定名、写响应）。
    if (_is_upload) {
        LogicSystem::GetInstance()->FinishUpload(shared_from_this());
        return;
    }
    HandleReq();
}

void HttpConnection::HandleReq() {
    //设置版本
    _response.version(_parser.get().version());
    //设置为短链接
    _response.keep_alive(false);
    //切分请求的url和参数对
    PreParseGetParam();

    //处理get请求
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

        LOG_INFO("处理 GET 请求成功");
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");

        WriteResponse();
        return;
    }

    //处理post请求
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

        LOG_INFO("处理 POST 请求成功");
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }
}

//把处理好的响应异步发回客户端，发完后关闭连接的发送端，并取消超时定时器
void HttpConnection::WriteResponse() {
    auto self = shared_from_this();
    //在 HTTP 响应头里设置 Content-Length
    _response.content_length(_response.body().size());
    //async write
    http::async_write(
        _socket,
        _response,
        [self](beast::error_code ec, std::size_t)
        {
            LOG_INFO("异步写 HTTP 响应完成");
            //只关闭发送端,接收方向还开着
            self->_socket.shutdown(tcp::socket::shutdown_send, ec);
            //取消超时定时器
            self->deadline_.cancel();
        });
}

tcp::socket& HttpConnection::GetSocket()
{
    return _socket;
}

//十进制-->十六进制
unsigned char ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

//十六进制-->十进制
unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 10 + 'A';
    else if (x >= 'a' && x <= 'z') y = x - 10 + 'a';
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}

//url编码
std::string UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        //判断是否仅有数字和字母构成
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ') //为空字符
            strTemp += "+";
        else
        {
            //其他字符需要提前加%并且高四位和低四位分别转为16进制
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] & 0x0F);
        }
    }
    return strTemp;
}

//url解码
std::string UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        //遇到%将后面的两个字符从16进制转为char再拼接
        if (str[i] == '%')
        {
            assert(i + 2 < str.length());
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[i+1]);
            strTemp += high * 16 + low;
        }
        else if (str[i] == '+') //还原+为空
            strTemp += ' ';
        else
            strTemp += str[i];
    }
    return strTemp;
}

//从 HTTP 请求的 URI 里，切分出路径和查询参数，存到成员变量 _get_url 和 _get_params
void HttpConnection::PreParseGetParam() {
    // 提取 URI
    auto uri = _parser.get().target();
    // 查找查询字符串的开始位置（即 '?' 的位置）
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
            key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }

    // 处理最后一个参数对（如果没有 & 分隔符）
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
