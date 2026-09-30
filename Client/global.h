#ifndef GLOBAL_H
#define GLOBAL_H
#include <QWidget>
#include <functional>
#include <QRegularExpression>//正则表达式
#include "QStyle"
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <memory>
#include <iostream>
#include <mutex>
#include <QDir>
#include <QSettings>

//声明 用于刷新qss样式表的函数(接收类型:QWidget*，返回类型:void)
extern std::function<void(QWidget*)> repolish;

//枚举请求Id类型
enum ReqId{
    ID_GET_VARIFY_CODE = 1001, //获取验证码
    ID_REG_USER = 1002, //注册用户
};

//枚举是哪个模块发出的请求mod
enum Modules{
    REGISTERMOD = 0,
};

//错误类型枚举
enum ErrorCodes{
    SUCCESS = 0,    //无错误
    ERR_JSON =1,    //json解析失败
    ERR_NETWORK = 2,    //网络错误

};

//网关(GateServer)的url前缀
extern QString gate_url_prefix;

#endif // GLOBAL_H
