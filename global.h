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
#include <QNetworkReply>
#include <memory>
#include <iostream>
#include <mutex>

//声明 用于刷新qss 的函数  (接收 QWidget*，返回 void)
extern std::function<void(QWidget*)> repolish;

enum ReqId{
    ID_GET_VARIFY_CODE = 1001, //获取验证码
    ID_REG_USER = 1002, //注册用户
};

enum Modules{
    REGISTERMOD = 0,
};

enum ErrorCodes{
    SUCCESS = 0,
    ERR_JSON =1,    //json解析失败
    ERR_NETWORK = 2,    //网络错误

};

#endif // GLOBAL_H
