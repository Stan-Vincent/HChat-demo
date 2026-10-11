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
    // ★ 改成 request_parser，才能设置 body_limit。
    //   原来用 http::request 直接 async_read，beast 默认 body_limit 只有 1MB：
    //   图片经 base64 会膨胀 4/3，所以原图超过 ~750KB 就被拒（且是直接断连接，
    //   客户端收不到任何错误提示）。手机照片 3-5MB 根本传不了。
    http::request_parser<http::dynamic_body> _parser;
    // 解析完成后把消息取出来（原 _request 的用法保持不变）
    http::request<http::dynamic_body> _request;

    // _response 用来回应客户端 在HandleReq()里填充内容
    http::response<http::dynamic_body> _response;

    //_deadline 用来做定时器判断请求是否超时
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(60) 
    };

    //get请求的url
    std::string _get_url;
    // ★ 存放静态资源（upload 下的图片）的二进制内容。
    //   必须放在成员里：beast::buffers_body 持有的是指针，
    //   如果指向 handler 里的局部变量，返回后数据就没了，客户端会收到乱码。
    std::string _file_body;

    //get请求的url的参数 结构:key->value
    std::unordered_map<std::string, std::string> _get_params;
};

