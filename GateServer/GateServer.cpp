#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "CServer.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "logger.h"

void TestRedisMgr() {
    
    assert(RedisMgr::GetInstance()->Set("blogwebsite", "hcat.club"));
    std::string value = "";
    assert(RedisMgr::GetInstance()->Get("blogwebsite", value));
    assert(RedisMgr::GetInstance()->Get("nonekey", value) == false);
    assert(RedisMgr::GetInstance()->HSet("bloginfo", "blogwebsite", "hcat.club"));
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
    hcat::log::init("gate");

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
            LOG_INFO("收到信号 {}，正在关闭...", signal_number);
            if (error) {
                return;
            }
            ioc.stop();
        });

        //上下文和端口号构造CServer，并调用Start();
        std::make_shared<CServer>(ioc, port)->Start();
        LOG_INFO("网关服务已启动，监听端口 {}", port);
        ioc.run();
        LOG_INFO("网关服务正常退出");
        hcat::log::shutdown();
    }
    catch (std::exception const& e)
    {
        LOG_ERROR("网关服务致命错误: {}", e.what());
        return EXIT_FAILURE;
    }
}