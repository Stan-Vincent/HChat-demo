# -*- coding: utf-8 -*-
"""让 ChatServer 的 LoadChatMsg 按字节预算分页，避免响应超过协议上限。

【背景】协议头长度字段只有 2 字节 -> 单个响应体上限 65535（MAX_MSG_BODY_LEN）。
       历史消息一页 10 条、每条 content 最长 2048 字。中文在
       Json::Value::toStyledString() 里是 \\uXXXX（6 字节/字），
       2000 字的消息一条就 12KB，10 条轻松破 100KB —— 装不下。

【为什么不能硬截】硬截会把 JSON 剪断，客户端 json 解析必然失败。
       实测：响应断在 "\\u4e2d\\u4e2d\\u4e2d" 半路，Python/Qt 都解析不了。
       正确做法：装不下就少放几条，并把 load_more 置 true，让客户端继续拉。

【做法】循环追加 chat_data 之后，检查序列化大小：
       超过 MSG_BODY_SAFE 就从尾部丢一条，并置 load_more=true、
       last_message_id 回退到剩下最后一条的 msg_id。
       如果连一条都装不下（单条本身就超限），保留原游标避免死循环。

LogicSystem.cpp 是 GBK，换行 CRLF；LoadChatMsg 函数体缩进是 1 个 tab。
"""
EOL = "\r\n"
T = "\t"


def blk(*lines):
    return "".join(l + EOL for l in lines)


OLD = blk(
    T + T + 'rtvalue["chat_datas"].append(chat_data);',
    T + "}",
    "",
    "}",
)

NEW = blk(
    T + T + 'rtvalue["chat_datas"].append(chat_data);',
    T + "}",
    "",
    T + "// ★ 按字节预算裁剪：装不下就少放几条，并置 load_more 让客户端继续拉。",
    T + "//   硬截会把 JSON 剪断（实测断在 \\u4e2d 半路），客户端解析必失败。",
    T + "while (rtvalue[\"chat_datas\"].size() > 0) {",
    T + T + "std::string tmp = rtvalue.toStyledString();",
    T + T + "if ((int)tmp.size() <= MSG_BODY_SAFE) {",
    T + T + T + "break;",
    T + T + "}",
    T + T + "Json::Value arr = rtvalue[\"chat_datas\"];",
    T + T + "int lastIdx = arr.size() - 1;",
    T + T + "int dropped_id = arr[lastIdx][\"msg_id\"].asInt();",
    T + T + "rtvalue[\"chat_datas\"].removeIndex(lastIdx, nullptr);",
    T + T + "rtvalue[\"load_more\"] = true;",
    T + T + "// 游标回退到「现在最后一条」，下次从它之后开始",
    T + T + "if (rtvalue[\"chat_datas\"].size() > 0) {",
    T + T + T + "int n = rtvalue[\"chat_datas\"].size() - 1;",
    T + T + T + 'rtvalue["last_message_id"] = rtvalue["chat_datas"][n]["msg_id"].asInt();',
    T + T + "} else {",
    T + T + T + "// 一条都装不下（单条本身就超限）—— 保留原游标，避免死循环",
    T + T + T + 'rtvalue["last_message_id"] = res->next_cursor;',
    T + T + "}",
    T + T + 'std::cerr << "LoadChatMsg: 响应超过 " << MSG_BODY_SAFE',
    T + T + T + '<< " 字节，丢弃 msg_id=" << dropped_id << std::endl;',
    T + "}",
    "}",
)

for srv in ("ChatServer", "ChatServer2"):
    p = r"D:\QTcode\HChat\%s\LogicSystem.cpp" % srv
    raw = open(p, "rb").read().decode("gbk", errors="replace")
    if "MSG_BODY_SAFE" in raw:
        print("[SKIP] %s: already patched" % srv)
        continue
    cnt = raw.count(OLD)
    if cnt != 1:
        print("[FAIL] %s: anchor count = %d" % (srv, cnt))
        raise SystemExit(1)
    raw = raw.replace(OLD, NEW, 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] %s: byte-budget paging added" % srv)