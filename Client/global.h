/******************************************************************************
 *
 * @file       global.h
 * @brief      全局头文件
 *
 * @author     CEACI_XXL
 * @date       2026/09/23
 * @history
 *****************************************************************************/
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

//声明 用于刷新 qss样式表 的函数  (接收 QWidget*，返回 void)
extern std::function<void(QWidget*)> repolish;

//请求枚举
enum ReqId{
    ID_GET_VARIFY_CODE = 1001, //获取验证码
    ID_REG_USER = 1002, //注册用户
};

//枚举是哪个模块发出的请求mod
enum Modules{
    REGISTERMOD = 0,
};

//错误枚举
enum ErrorCodes{
    SUCCESS = 0,
    ERR_JSON =1,    //json解析失败
    ERR_NETWORK = 2,    //网络错误

};

extern QString gate_url_prefix;

#endif // GLOBAL_H
