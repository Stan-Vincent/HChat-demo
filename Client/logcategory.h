//==============================================================
// HChat 客户端日志分类（Qt 原生 QLoggingCategory）
//
//   客户端是 Qt 程序，Qt 自带一套日志分类体系，与 qDebug/qWarning 完全兼容，
//   可以用 QT_LOGGING_RULES 环境变量在运行时控制级别，Qt Creator 里也能
//   按分类过滤。
//
// 分类：
//   lcNetwork  —— HTTP / TCP 收发、连接状态、协议解析
//   lcChat     —— 会话列表、消息气泡、发送流程
//   lcUi       —— 界面交互、窗口状态、样式
//   lcUser     —— 登录注册、验证码、资料页
//   lcAvatar   —— 头像下载与缓存
//
// 运行时控制（PowerShell）：
//   $env:QT_LOGGING_RULES="hchat.network.debug=true;hchat.ui.debug=false"
//==============================================================
#pragma once

#include <QLoggingCategory>

// 分类名统一带 "hchat." 前缀，便于用 QT_LOGGING_RULES 通配控制
Q_DECLARE_LOGGING_CATEGORY(lcNetwork)
Q_DECLARE_LOGGING_CATEGORY(lcChat)
Q_DECLARE_LOGGING_CATEGORY(lcUi)
Q_DECLARE_LOGGING_CATEGORY(lcUser)
Q_DECLARE_LOGGING_CATEGORY(lcAvatar)

// 在 QApplication 构造之后调用一次：设置各分类的默认级别
void init_log_categories();