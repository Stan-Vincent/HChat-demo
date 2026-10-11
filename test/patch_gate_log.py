# -*- coding: utf-8 -*-
"""GateServer.cpp 接入统一日志（GBK 安全的正则替换）"""
import re, os

def read_gbk(p):
    raw = open(p, "rb").read()
    try:
        return raw.decode("gbk")
    except UnicodeDecodeError:
        return raw.decode("utf-8")

def write_gbk(p, txt):
    try:
        open(p, "wb").write(txt.encode("gbk"))
    except UnicodeEncodeError:
        open(p, "wb").write(txt.encode("utf-8"))

p = r"D:\QTcode\HChat\GateServer\GateServer.cpp"
txt = read_gbk(p)
orig = txt
nl = "\r\n" if "\r\n" in txt else "\n"

# 1) 加 include
if '#include "logger.h"' not in txt:
    txt = txt.replace('#include "RedisMgr.h"',
                      '#include "RedisMgr.h"' + nl + '#include "logger.h"', 1)
    print("  [ok] include")

# 2) main 开头初始化日志
txt = re.sub(r'(int main\(\)\s*\r?\n\{\r?\n)',
             r'\1    hchat::log::init("gate");' + nl + nl,
             txt, count=1)
print("  [ok] init")

# 3) signal 日志（替换整条 std::cout，注释是乱码不能用字面量匹配）
txt = re.sub(r'std::cout <<[^\n]*signal_number[^\n]*std::endl;',
             'LOG_INFO("received signal {}, shutting down...", signal_number);',
             txt, count=1)
print("  [ok] signal")

# 4) 启动日志
txt = re.sub(r'std::cout << "Gate Server listen on port:"<< port <<"\\n";',
             'LOG_INFO("GateServer started, listening on port {}", port);',
             txt, count=1)
print("  [ok] listen")

# 5) 致命错误
txt = re.sub(r'std::cerr << "Gate Server Error: " << e\.what\(\) << std::endl;',
             'LOG_ERROR("GateServer fatal error: {}", e.what());',
             txt, count=1)
print("  [ok] fatal")

# 6) 退出日志 + flush
txt = re.sub(r'(\r?\n)(\s*)ioc\.run\(\);',
             r'\1\2ioc.run();' + nl + r'\2LOG_INFO("GateServer exited gracefully");'
             + nl + r'\2hchat::log::shutdown();',
             txt, count=1)
print("  [ok] shutdown")

if txt != orig:
    write_gbk(p, txt)
    print("saved:", p)
else:
    print("no change")