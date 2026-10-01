#pragma once
#include "const.h"
class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
public:
    friend class LogicSystem;
	HttpConnection(boost::asio::io_context& ioc);
	void Start();
    tcp::socket& GetSocket();

private:
    //启动60秒倒计时如果期间没有新请求，就关闭连接
	void CheckDeadline();

    //把处理好的响应异步发回客户端，发完后关闭连接的发送端，并取消超时定时器
	void WriteResponse();

    //处理 _request 并填充_response
	void HandleReq();

    //从 HTTP 请求的 URI 里，切分出路径和查询参数，存到成员变量 _get_url 和 _get_params
    void PreParseGetParam();

    //套接字
	tcp::socket _socket;

    //_buffer 用来接受数据
    beast::flat_buffer  _buffer{ 8192 };

    // _request 用来解析请求 在async_read时填充内容
    http::request<http::dynamic_body> _request;

    // _response 用来回应客户端 在HandleReq()里填充内容
    http::response<http::dynamic_body> _response;

    //_deadline 用来做定时器判断请求是否超时
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(60) 
    };

    //get请求的url
    std::string _get_url;

    //get请求的url的参数 结构:key->value
    std::unordered_map<std::string, std::string> _get_params;
};

