#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "CServer.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"

void TestRedisMgr() {
    
    assert(RedisMgr::GetInstance()->Set("blogwebsite", "xxxl.club"));
    std::string value = "";
    assert(RedisMgr::GetInstance()->Get("blogwebsite", value));
    assert(RedisMgr::GetInstance()->Get("nonekey", value) == false);
    assert(RedisMgr::GetInstance()->HSet("bloginfo", "blogwebsite", "xxxl.club"));
    assert(RedisMgr::GetInstance()->HGet("bloginfo", "blogwebsite") != "");
    assert(RedisMgr::GetInstance()->ExistsKey("bloginfo"));
    assert(RedisMgr::GetInstance()->Del("bloginfo"));
    assert(RedisMgr::GetInstance()->Del("bloginfo"));
    assert(RedisMgr::GetInstance()->ExistsKey("bloginfo") == false);
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue1"));
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue2"));
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue3"));
    assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->LPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->LPop("lpushkey2", value) == false);
}

int main()
{
    //TestRedisMgr();

    //ConfigMgr:从config,ini文件里读取GateServer的端口号
    auto& gCfgMgr = ConfigMgr::Inst();
    std::string gate_post_str = gCfgMgr["GateServer"]["Port"];
    //std::cout <<"gate_post_str:"<< gate_post_str << std::endl;
    unsigned short gate_port = atoi(gate_post_str.c_str());
    
    try
    {
        unsigned short port = static_cast<unsigned short>(gate_port);
        net::io_context ioc{ 1 };

        ///创建一个信号集合对象，把 SIGINT 和 SIGTERM 两个信号注册进去，以后可以异步等待这两个退出信号
        //SIGINT	Ctrl+C 发出的中断信号
        //SIGTERM	系统终止进程的信号
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& error, int signal_number) {
            std::cout << "收到 "<<signal_number<<" 退出信号，正在关闭服务器..." << std::endl;
            if (error) {
                return;
            }
            ioc.stop();
        });

        //上下文和端口号构造CServer，并调用Start();
        std::make_shared<CServer>(ioc, port)->Start();
        std::cout << "Gate Server listen on port:"<< port <<"\n";
        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Gate Server Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}