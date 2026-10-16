#include "HttpConnection.h"
#include "LogicSystem.h"
#include "logger.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc)
    : _socket(ioc) {
}

void HttpConnection::Start()
{
    //当前对象的shared_ptr 捕获进 lambda，保证异步读完成前，当前对象不会被销毁
    auto self = shared_from_this();

    // ★ 显式设置 body_limit。
    //   默认 1MB 太小：图片 base64 会膨胀 4/3，
    //   原图超过 ~750KB 就被拒，而且是直接断连接、
    //   客户端没有任何错误提示（只会收到 10053）。
    //   客户端上限 5MB -> base64 后约 6.7MB，这里给 8MB 留足量。
    // NOTE: limit raised from 8MB to 32MB -- a 20MB file becomes ~27MB after
    //       base64, so it must fit, otherwise the parser rejects the request.
    self->_parser.body_limit(32 * 1024 * 1024);

    http::async_read(_socket, _buffer, self->_parser,
        [self](beast::error_code ec,std::size_t bytes_transferred) 
        {
            try {
                LOG_INFO("开始异步读取 HTTP 请求...");
                if (ec) {
                    // ★ 超限时给一个干净的 413，
                    //   而不是默默断开连接
                    if (ec == http::error::body_limit) {
LOG_ERROR("request body >= 32MB, rejected");
                        // ★ 必须显式设置 version / keep_alive：HandleReq() 没被调用，
                        //   这两个还是默认值，不设就发不出去。
                        self->_response.version(11);
                        self->_response.keep_alive(false);
                        self->_response.result(http::status::payload_too_large);
                        self->_response.set(http::field::content_type, "text/plain");
                        beast::ostream(self->_response.body()) << "request body too large";
                        self->_response.content_length(self->_response.body().size());
                        self->WriteResponse();
                        return;
                    }
                    LOG_ERROR("HTTP 读取错误: {}", ec.what());
                    return;
                }

                // 解析完成，把消息取出来存到 _request
                self->_request = self->_parser.release();

                //bytes_transferred：本次读到的字节数。这里用不到，就用 boost::ignore_unused 忽略掉
                boost::ignore_unused(bytes_transferred);
                //处理 _request 并填充_response
                self->HandleReq();
                //每收到一个请求，就重新调一次 CheckDeadline() 刷新倒计时
                self->CheckDeadline();
            }
            catch (std::exception& exp) {
                LOG_ERROR("HTTP 连接异常: {}", exp.what());
            }
        }
    );
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
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
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
        //还原+为空
        if (str[i] == '+') strTemp += ' ';
        //遇到%将后面的两个字符从16进制转为char再拼接
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}

//从 HTTP 请求的 URI 里，切分出路径和查询参数，存到成员变量 _get_url 和 _get_params
void HttpConnection::PreParseGetParam() {
    // 提取 URI  
    auto uri = _request.target();
    // 查找查询字符串的开始位置（即 '?' 的位置）  
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos) {
        _get_url = uri;
        return;
    }

    _get_url = uri.substr(0, query_pos);
    std::string query_string = uri.substr(query_pos + 1);
    std::string key;
    std::string value;
    size_t pos = 0;
    while ((pos = query_string.find('&')) != std::string::npos) {
        auto pair = query_string.substr(0, pos);
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码  
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }
    // 处理最后一个参数对（如果没有 & 分隔符）  
    if (!query_string.empty()) {
        size_t eq_pos = query_string.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(query_string.substr(0, eq_pos));
            value = UrlDecode(query_string.substr(eq_pos + 1));
            _get_params[key] = value;
        }
    }
}

//处理 _request 并填充_response
void HttpConnection::HandleReq() {
    //设置版本
    _response.version(_request.version());
    //设置为短链接
    _response.keep_alive(false);
    //切分请求的url和参数对
    PreParseGetParam();

    //处理get请求
    if (_request.method() == http::verb::get) {
        //调用LogicSystem 的函数HandleGet
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
    if (_request.method() == http::verb::post) {
        //调用LogicSystem 的函数HandleGet
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
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

    http::async_write(
        _socket,
        _response,
        [self](beast::error_code ec, std::size_t)
        {
            LOG_INFO("异步写（连接启动）");
            //只关闭发送端,接收方向还开着
            self->_socket.shutdown(tcp::socket::shutdown_send, ec);
            //取消超时定时器
            self->deadline_.cancel();
        });
}

//启动 60 秒倒计时。如果期间没有新请求，就关闭连接；每收到一个请求，就重新刷新倒计时
void HttpConnection::CheckDeadline() {
    auto self = shared_from_this();
    deadline_.async_wait(
        [self](beast::error_code ec)
        {
            if (!ec)
            {
                //超时则关闭socket 
                self->_socket.close(ec);
            }
        });
}




