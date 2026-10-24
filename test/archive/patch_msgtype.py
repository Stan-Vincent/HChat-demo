# -*- coding: utf-8 -*-
"""给 ChatServer / ChatServer2 的 LogicSystem.cpp 补上 msg_type 回包字段。
源文件是 GBK，按 gbk 读写。注意保留 CRLF。"""
import io, sys, os

BASE = r"D:\QTcode\HChat"

# (old, new) 精确替换对
PATCHES = [
    # 1) AuthFriend: 通知对方的那段循环（4 个 tab 缩进）
    (
        '\t\t\t\tchat["msg_content"] = chat_data->msgcontent();\r\n'
        '\t\t\t\tchat["chat_time"] = chat_time;\r\n'
        '\t\t\t\tchat["status"] = chat_data->status();\r\n',
        '\t\t\t\tchat["msg_content"] = chat_data->msgcontent();\r\n'
        '\t\t\t\tchat["msg_type"] = chat_data->msg_type;\r\n'
        '\t\t\t\tchat["total_size"] = (Json::Int64)chat_data->total_size;\r\n'
        '\t\t\t\tchat["md5"] = chat_data->md5;\r\n'
        '\t\t\t\tchat["chat_time"] = chat_time;\r\n'
        '\t\t\t\tchat["status"] = chat_data->status();\r\n',
    ),
    # 2) AuthFriend: 主循环
    (
        '\t\tchat["msg_content"] = chat_data->msgcontent();\r\n'
        '\t\tchat["chat_time"] = chat_time;\r\n'
        '\t\tchat["status"] = chat_data->status();\r\n',
        '\t\tchat["msg_content"] = chat_data->msgcontent();\r\n'
        '\t\tchat["msg_type"] = chat_data->msg_type;\r\n'
        '\t\tchat["total_size"] = (Json::Int64)chat_data->total_size;\r\n'
        '\t\tchat["md5"] = chat_data->md5;\r\n'
        '\t\tchat["chat_time"] = chat_time;\r\n'
        '\t\tchat["status"] = chat_data->status();\r\n',
    ),
    # 3) LoadChatMsg 历史消息
    (
        '\t\tchat_data["msg_content"] = chat.content;\r\n'
        '\t\tchat_data["chat_time"] = chat.chat_time;\r\n',
        '\t\tchat_data["msg_content"] = chat.content;\r\n'
        '\t\tchat_data["msg_type"] = chat.msg_type;\r\n'
        '\t\tchat_data["total_size"] = (Json::Int64)chat.total_size;\r\n'
        '\t\tchat_data["md5"] = chat.md5;\r\n'
        '\t\tchat_data["chat_time"] = chat.chat_time;\r\n',
    ),
]

for srv in ("ChatServer", "ChatServer2"):
    p = os.path.join(BASE, srv, "LogicSystem.cpp")
    raw = open(p, "rb").read().decode("gbk")
    orig = raw
    for old, new in PATCHES:
        n = raw.count(old)
        if n == 0:
            print("[SKIP] %s: pattern not found:\n%s" % (srv, old.replace("\r\n", "\\n")))
            continue
        raw = raw.replace(old, new)
        print("[OK] %s: replaced %d occurrence(s)" % (srv, n))
    if raw != orig:
        open(p, "wb").write(raw.encode("gbk"))
        print("[WRITE] %s" % p)
    else:
        print("[NOCHANGE] %s" % p)