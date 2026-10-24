# -*- coding: utf-8 -*-
"""修 ChatServer 的远程堆缓冲区溢出（一个 4 字节包头就能打死服务）。

【实测】
  header = 2 字节 id + 2 字节 len，声明 len = 65535
  服务端：`short msg_len` 解读成 **-1**
  -> `if (msg_len > MAX_LENGTH)` 即 `-1 > 2048` 为假 -> **检查被绕过**
  -> `RecvNode(-1)` -> `new char[0]` 成功（不抛异常）
  -> `AsyncReadBody(-1)` -> asyncReadFull(size_t(-1)) -> 巨大长度
  -> `async_read_some(buffer(_data, 巨大))` 往 `char _data[2048]` 里写
  -> **堆缓冲区溢出**，实测直接把 ChatServer 打死（端口不再监听）

  声明 len = 40000 时 short = -25536，同样绕过，表现为 "bad allocation"。

【根因】长度/消息号用了 **有符号 short**：
    short msg_id = 0;  memcpy(&msg_id, ...);
    msg_id = network_to_host_short(msg_id);
    if (msg_id > MAX_LENGTH) {...}     // 负数恒不大于 2048 -> 绕过
    short msg_len = 0; 同理
  协议里这两个字段本来就是 2 字节【无符号】，应该用 uint16_t 来接。

【修法】改成 uint16_t 接收 + 转成 int 再比较，并且显式拒绝 <= 0 的长度。

CSession.cpp 是 GBK，换行 CRLF，缩进 2 个 tab（在 lambda 里）。
"""
import sys

EOL = "\r\n"
T = "\t"


def blk(*lines):
    return "".join(l + EOL for l in lines)


for srv in ("ChatServer", "ChatServer2"):
    p = r"D:\QTcode\HChat\%s\CSession.cpp" % srv
    raw = open(p, "rb").read().decode("gbk", errors="replace")
    orig = raw

    # ---- msg_id ----
    old_id = blk(
        T + T + T + "short msg_id = 0;",
        T + T + T + "memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);",
        T + T + T + "//\u7f51\u7edc\u5b57\u8282\u5e8f\u8f6c\u5316\u4e3a\u672c\u5730\u5b57\u8282\u5e8f",
        T + T + T + "msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);",
    )
    new_id = blk(
        T + T + T + "// \u2605 \u5fc5\u987b\u7528 uint16_t\uff1a\u534f\u8bae\u91cc\u8fd9\u4e2a\u5b57\u6bb5\u662f 2 \u5b57\u8282\u65e0\u7b26\u53f7\u3002",
        T + T + T + "//   \u7528 short \u63a5\u6536\u7684\u8bdd\uff0c0xFFFF \u4f1a\u53d8\u6210 -1\uff0c",
        T + T + T + "//   \u800c -1 > MAX_LENGTH \u4e3a\u5047 -> \u6821\u9a8c\u88ab\u7ed5\u8fc7 -> \u5806\u7f13\u51b2\u533a\u6ea2\u51fa\u3002",
        T + T + T + "uint16_t msg_id = 0;",
        T + T + T + "memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);",
        T + T + T + "//\u7f51\u7edc\u5b57\u8282\u5e8f\u8f6c\u5316\u4e3a\u672c\u5730\u5b57\u8282\u5e8f",
        T + T + T + "msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);",
    )
    if old_id in raw:
        raw = raw.replace(old_id, new_id, 1)
    else:
        print("  [%s] msg_id anchor missing" % srv)

    # ---- msg_len ----
    old_len = blk(
        T + T + T + "short msg_len = 0;",
        T + T + T + "memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);",
        T + T + T + "//\u7f51\u7edc\u5b57\u8282\u5e8f\u8f6c\u5316\u4e3a\u672c\u5730\u5b57\u8282\u5e8f",
        T + T + T + "msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);",
    )
    new_len = blk(
        T + T + T + "// \u2605 \u540c\u7406\uff1a\u7528 uint16_t \u63a5\uff0c\u518d\u8f6c int \u6bd4\u8f83\u3002",
        T + T + T + "uint16_t msg_len_raw = 0;",
        T + T + T + "memcpy(&msg_len_raw, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);",
        T + T + T + "//\u7f51\u7edc\u5b57\u8282\u5e8f\u8f6c\u5316\u4e3a\u672c\u5730\u5b57\u8282\u5e8f",
        T + T + T + "int msg_len = (int)boost::asio::detail::socket_ops::network_to_host_short(msg_len_raw);",
    )
    if old_len in raw:
        raw = raw.replace(old_len, new_len, 1)
    else:
        print("  [%s] msg_len anchor missing" % srv)

    # ---- 长度校验加 <= 0 ----
    old_chk = blk(
        T + T + T + "if (msg_len > MAX_LENGTH) {",
        T + T + T + T + 'std::cout << "invalid data length is " << msg_len << endl;',
        T + T + T + T + "_server->ClearSession(_session_id);",
        T + T + T + T + "return;",
        T + T + T + "}",
    )
    new_chk = blk(
        T + T + T + "// \u2605 \u5fc5\u987b\u540c\u65f6\u62d2\u7edd <= 0 \u548c\u8d85\u8fc7\u7f13\u51b2\u533a\u7684\u957f\u5ea6",
        T + T + T + "if (msg_len <= 0 || msg_len > MAX_LENGTH) {",
        T + T + T + T + 'std::cout << "invalid data length is " << msg_len << endl;',
        T + T + T + T + "_server->ClearSession(_session_id);",
        T + T + T + T + "return;",
        T + T + T + "}",
    )
    if old_chk in raw:
        raw = raw.replace(old_chk, new_chk, 1)
    else:
        print("  [%s] len-check anchor missing" % srv)

    if raw != orig:
        open(p, "wb").write(raw.encode("gbk"))
        print("[OK] %s: 包头长度校验已加固" % srv)
    else:
        print("[NOCHANGE] %s" % srv)