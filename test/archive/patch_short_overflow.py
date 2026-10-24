# -*- coding: utf-8 -*-
"""修 ChatServer 的「大响应被截断」bug —— short 长度溢出。

【实测复现】往 chat_message 插 10 条 2000 字的中文消息，然后拉历史消息：
    声明的长度 ln = 9396
    实际收到    = 9396 字节
    JSON 断在半路：...\\u4e2d\\u4e2d\\u4e2d  <- 不是合法 JSON，客户端解析直接失败

【根因】
    MsgNode(short max_len) / SendNode(const char* msg, short max_len, short msg_id)
    长度字段是有符号 16 位 short，而调用处传的是 msg.length()（size_t）：
        _send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
    协议头里的长度字段本来就是 2 字节，所以单个响应体上限是 65535。
    但 short 是【有符号】的，超过 32767 就溢出成负数/错值：
        - (short)L 溢出 -> MsgNode 里 new char[_total_len + 1] 拿到错的大小
        - memcpy 用错的长度 -> 只拷了一部分
        - async_write 按 _total_len 发 -> 客户端收到截断的 JSON
    中文在 Json::Value::toStyledString() 里是 \\uXXXX（6 字节/字），
    2000 字的消息一条就 12KB，10 条轻松破 100KB —— 必现。

【修法】
    1. MsgNode / SendNode / RecvNode 的长度字段 short -> int（32 位），
       彻底消除截断和负数。
    2. CSession::Send 加长度上限检查：超过协议能表示的 65535 就打错误日志并且
       【不发】—— 发半截 JSON 比不发更糟（客户端解析必崩）。
    3. （另一个补丁里做）LogicSystem::LoadChatMsg 按字节预算分页，
       保证响应永远装得下。

MsgNode.h / MsgNode.cpp / CSession.h / CSession.cpp 都是 GBK，换行 \\r\\n。
"""
import sys

EOL = "\r\n"
T = "\t"


def blk(*lines):
    """把多行拼成一块，每行末尾补 CRLF。"""
    return "".join(l + EOL for l in lines)


def patch(path, pairs, enc="gbk"):
    raw = open(path, "rb").read().decode(enc, errors="replace")
    orig = raw
    for old, new in pairs:
        if old not in raw:
            print("  [SKIP] anchor missing in %s: %r" % (path.split("\\")[-1], old[:70]))
            continue
        raw = raw.replace(old, new, 1)
    if raw != orig:
        open(path, "wb").write(raw.encode(enc))
        print("[OK] %s" % path.split("\\")[-1])


INT_NOTE = blk(
    T + "// ★ 长度字段必须用 int，不能用 short：",
    T + "//   协议头的长度是 2 字节，单个响应体上限 65535，",
    T + "//   而 short 是有符号 16 位，超过 32767 就溢出成负数，",
    T + "//   导致 new char[负数] / memcpy(负长度) / 发半截数据。",
)

for srv in ("ChatServer", "ChatServer2"):
    print("=== %s" % srv)

    patch(r"D://QTcode//HChat//%s//MsgNode.h" % srv, [
        (
            blk(T + "MsgNode(short max_len) :_total_len(max_len), _cur_len(0) {"),
            INT_NOTE + blk(T + "MsgNode(int max_len) :_total_len(max_len), _cur_len(0) {"),
        ),
        (
            blk(T + "short _cur_len;", T + "short _total_len;"),
            blk(T + "int _cur_len;", T + "int _total_len;"),
        ),
        (
            blk(T + "RecvNode(short max_len, short msg_id);"),
            blk(T + "RecvNode(int max_len, short msg_id);"),
        ),
        (
            blk(T + "SendNode(const char* msg,short max_len, short msg_id);"),
            blk(T + "SendNode(const char* msg, int max_len, short msg_id);"),
        ),
    ], enc="gbk")

    patch(r"D://QTcode//HChat//%s//MsgNode.cpp" % srv, [
        (
            blk("RecvNode::RecvNode(short max_len, short msg_id):MsgNode(max_len),"),
            blk("RecvNode::RecvNode(int max_len, short msg_id):MsgNode(max_len),"),
        ),
        (
            blk("SendNode::SendNode(const char* msg, short max_len, short msg_id):MsgNode(max_len + HEAD_TOTAL_LEN)"),
            blk("SendNode::SendNode(const char* msg, int max_len, short msg_id):MsgNode(max_len + HEAD_TOTAL_LEN)"),
        ),
    ], enc="gbk")

    patch(r"D://QTcode//HChat//%s//CSession.h" % srv, [
        (
            blk(T + "void Send(char* msg,  short max_length, short msgid);"),
            blk(T + "void Send(char* msg, int max_length, short msgid);"),
        ),
    ], enc="gbk")

    patch(r"D://QTcode//HChat//%s//CSession.cpp" % srv, [
        (
            blk("void CSession::Send(char* msg, short max_length, short msgid) {"),
            blk("void CSession::Send(char* msg, int max_length, short msgid) {"),
        ),
        (
            blk(T + "_send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));"),
            blk(
                T + "// ★ 协议头的长度字段只有 2 字节，单个响应体上限 65535。",
                T + "//   超过就装不下了：原来会被 (short) 截断，",
                T + "//   发一个被剪掉的 JSON 给客户端（实测：断在 \\u4e2d... 半路）。",
                T + "//   发半截数据比不发更糟 —— 客户端解析必然失败。",
                T + "if (msg.length() > MAX_MSG_BODY_LEN) {",
                T + T + 'std::cerr << "ERROR: 响应体过大 (" << msg.length() << " > " << MAX_MSG_BODY_LEN',
                T + T + T + '<< "), msgid=" << msgid << ", 已丢弃（不发半截数据）" << std::endl;',
                T + T + "return;",
                T + "}",
                T + "_send_que.push(make_shared<SendNode>(msg.c_str(), (int)msg.length(), msgid));",
            ),
        ),
    ], enc="gbk")