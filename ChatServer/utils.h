#pragma once
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>
#include <string>

//★ 安全的字符串转整数：解析失败返回 default_val，绝不抛 std::invalid_argument。
//  std::stoXxx 遇到空串/非数字会抛异常，而 LogicSystem::DealMsg 的工作线程
//  没有 try/catch 时，一个异常就会 std::terminate 让整个 ChatServer 进程崩掉。
int safeToInt(const std::string& s, int default_val = 0);


std::string getCurrentTimestamp();