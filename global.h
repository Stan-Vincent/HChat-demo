#ifndef GLOBAL_H
#define GLOBAL_H


/*
 *      global.h
 *
 *      全局头文件
 */

#include <QWidget>
#include <functional>
#include <QRegularExpression>//正则表达式
#include "QStyle"

//声明 用于刷新qss 的函数
//接收 QWidget*，返回 void
extern std::function<void(QWidget*)> repolish;

#endif // GLOBAL_H
