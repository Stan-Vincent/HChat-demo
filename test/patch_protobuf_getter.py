# -*- coding: utf-8 -*-
"""修AuthFriend 回包里对 protobuf 生成类的字段访问。

protobuf 3.x 生成的是 public 字段（chat_data->md5），但 6.x 生成的是
inline getter（chat_data->md5()）。之前按 3.x 的写法写，cl.exe 报 C3867/C2679/C2440。

只改 AuthFriend 里那两处（变量名是 chat_data，不是 chat_data->md5() 之外的东西）。
源文件是 GBK，按 gbk 读写，保留 CRLF。
"""
import os

BASE = r"D:\QTcode\HChat"

PATCHES = [
    # 4 tab缩进的那段（AuthFriend 通知对方）
    (
        '\t\t\t\tchat["msg_type"] = chat_data->msg_type;\r\n'
        '\t\t\t\tchat["total_size"] = (Json::Int64)chat_data->total_size;\r\n'
        '\t\t\t\tchat["md5"] = chat_data->md5;\r\n',
        '\t\t\t\tchat["msg_type"] = chat_data->msg_type();\r\n'
        '\t\t\t\tchat["total_size"] = (Json::Int64)chat_data->total_size();\r\n'
        '\t\t\t\tchat["md5"] = chat_data->md5();\r\n',
    ),
    # 2 tab 缩进的那段（AuthFriend 主循环）
    (
        '\t\tchat["msg_type"] = chat_data->msg_type;\r\n'
        '\t\tchat["total_size"] = (Json::Int64)chat_data->total_size;\r\n'
        '\t\tchat["md5"] = chat_data->md5;\r\n',
        '\t\tchat["msg_type"] = chat_data->msg_type();\r\n'
        '\t\tchat["total_size"] = (Json::Int64)chat_data->total_size();\r\n'
        '\t\tchat["md5"] = chat_data->md5();\r\n',
    ),
]

for srv in ("ChatServer", "ChatServer2"):
    p = os.path.join(BASE, srv, "LogicSystem.cpp")
    raw = open(p, "rb").read().decode("gbk")
    orig = raw
    for old, new in PATCHES:
        n = raw.count(old)
        if n == 0:
            print("[SKIP] %s: not found" % srv)
            continue
        raw = raw.replace(old, new)
        print("[OK] %s: %d replaced" % (srv, n))
    if raw != orig:
        open(p, "wb").write(raw.encode("gbk"))
        print("[WRITE] %s" % srv)