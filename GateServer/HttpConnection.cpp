#include "HttpConnection.h"
#include "LogicSystem.h"

HttpConnection::HttpConnection(tcp::socket socket)
    : _socket(std::move(socket)) {
}

void HttpConnection::Start()
{
    //当前对象的shared_ptr 捕获进 lambda，保证异步读完成前，当前对象不会被销毁
    auto self = shared_from_this();
    http::async_read(_socket, _buffer, _request, 
        [self](beast::error_code ec,std::size_t bytes_transferred) 
        {
            try {
                std::cout << "HttpConnection Async_read...\n";
                if (ec) {
                    std::cout << "Http read err is " << ec.what() << std::endl;
                    return;
                }

                //bytes_transferred：本次读到的字节数。这里用不到，就用 boost::ignore_unused 忽略掉
                boost::ignore_unused(bytes_transferred);
                //处理 _request 并填充_response
                self->HandleReq();
                //每收到一个请求，就重新调一次 CheckDeadline() 刷新倒计时
                self->CheckDeadline();
            }
            catch (std::exception& exp) {
                std::cout << "HttpConnection Exception is " << exp.what() << std::endl;
            }
        }
    );
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

        std::cout << "HttpConnection::HandleReq() success...\n\n";
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

        std::cout << "HttpConnection::HandlePost() success...\n\n";
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }
}

void HttpConnection::WriteResponse() {
    auto self = shared_from_this();
    _response.content_length(_response.body().size());
    http::async_write(
        _socket,
        _response,
        [self](beast::error_code ec, std::size_t)
        {
            std::cout << "Async_write...(HttpConnection:Start())\n\n";
            self->_socket.shutdown(tcp::socket::shutdown_send, ec);
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




