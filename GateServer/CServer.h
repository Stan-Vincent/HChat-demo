#pragma once
#include "const.h"

class CServer :public std::enable_shared_from_this<CServer>
{
public:
    //CServer类构造函数接受一个端口号，创建acceptor接受新到来的链接
    CServer(boost::asio::io_context& ioc, unsigned short& port);
    //Start函数内创建HttpConnection类型智能指针,将_socket内部数据转移给HttpConnection管理
    void Start();
private:
    tcp::acceptor  _acceptor;   //接收器
    net::io_context& _ioc;      //上下文
    tcp::socket   _socket; 
};

