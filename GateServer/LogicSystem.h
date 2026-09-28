#pragma once
#include "const.h"

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

class LogicSystem :public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    bool HandleGet(std::string, std::shared_ptr<HttpConnection>);
    void RegGet(std::string url, HttpHandler handler);
    void RegPost(std::string url, HttpHandler handler);
private:
    LogicSystem();

    //_post_handlers和_get_handlers分别是post请求和get请求的回调函数map，key为路由，value为回调函数
    std::map<std::string, HttpHandler> _post_handlers;
    std::map<std::string, HttpHandler> _get_handlers;
};

