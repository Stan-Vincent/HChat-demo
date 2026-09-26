#pragma once
#include "const.h"
class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
public:
    friend class LogicSystem;
	HttpConnection(tcp::socket socket);
	void Start();
private:
	void CheckDeadline();
	void WriteResponse();
	void HandleReq();
	tcp::socket _socket;
    //_buffer 用来接受数据
    beast::flat_buffer  _buffer{ 8192 };

    // _request 用来解析请求
    http::request<http::dynamic_body> _request;

    // _response 用来回应客户端
    http::response<http::dynamic_body> _response;

    //_deadline 用来做定时器判断请求是否超时
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(60) 
    };
};

