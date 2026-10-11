# -*- coding: utf-8 -*-
"""安全回归测试：用畸形包头打 ChatServer，验证它既不被打崩也不崩内存。

【背景漏洞】协议头 = 2 字节 id + 2 字节 len。
    原代码用 `short msg_len`（有符号 16 位）接收：
      - 声明 65535 -> short = -1   -> `-1 > MAX_LENGTH(2048)` 为假 -> 绕过校验
      - RecvNode(-1) -> new char[0] 成功（不抛异常）
      - AsyncReadBody(-1) -> asyncReadFull(size_t(-1)) -> 巨大长度
      - async_read_some 往 char[2048] 里写 -> **堆缓冲区溢出**，实测把服务打死
    修复：改用 uint16_t 接收 + 转 int 比较 + 显式拒绝 <= 0。

【本测试】发一堆畸形包头，然后确认：
    1) 服务端进程还活着（端口仍在监听）
    2) 正常请求仍能正确响应（没有因为恶意连接而卡死/状态错乱）
"""
import socket
import struct
import time
import json
import subprocess
import sys

HOST, PORT = "127.0.0.1", 8990

# 各种畸形/边界长度：负数解读、超长、0、1、正好等于缓冲区
BAD_LENS = [65535, 40000, 32768, 32767, 30000, 65534, 0, 2049, 2048, 1]


def port_alive():
    out = subprocess.run(["netstat", "-an"], capture_output=True, timeout=15)
    txt = out.stdout.decode("utf-8", "replace")
    return (":%d " % PORT) in txt and "LISTENING" in txt


def main():
    print("  ChatServer 畸形包头安全测试 (%s:%d)" % (HOST, PORT))
    if not port_alive():
        print("  !! 端口没在监听，请先启动 ChatServer")
        return 2

    sent = 0
    for L in BAD_LENS:
        for body_len in (0, 100, 8000):        # 空 / 小 / 远超缓冲区
            try:
                s = socket.create_connection((HOST, PORT), timeout=3)
                s.sendall(struct.pack(">HH", 1005, L))
                if body_len:
                    time.sleep(0.05)
                    s.sendall(b"X" * body_len)
                time.sleep(0.05)
                s.close()
                sent += 1
            except Exception:
                pass      # 被拒连也算合理防御

    print("  已发送 %d 个畸形包（含 %d 种长度 × 3 种载荷）" % (sent, len(BAD_LENS)))
    time.sleep(2)

    ok = True
    if port_alive():
        print("  [PASS] 服务端存活（没有被畸形包打死）")
    else:
        print("  [FAIL] 服务端已崩溃 —— 缓冲区溢出漏洞仍在！")
        ok = False

    # 再发一个正常请求，确认服务没被搞成不可用状态
    try:
        out = subprocess.run(
            [r"D:\Program Files\Redis\redis-cli.exe", "-p", "6380",
             "-a", "123456", "GET", "utoken_5"],
            capture_output=True, timeout=10)
        tok = out.stdout.decode().strip()
        if not tok:
            print("  [SKIP] 拿不到 token，跳过正常请求验证")
            return 0 if ok else 1

        s = socket.create_connection((HOST, PORT), timeout=8)
        s.settimeout(8)
        payload = json.dumps({"uid": 5, "token": tok}).encode()
        s.sendall(struct.pack(">HH", 1005, len(payload)) + payload)
        head = s.recv(4)
        mid, ln = struct.unpack(">HH", head)
        body = b""
        while len(body) < ln:
            body += s.recv(ln - len(body))
        o = json.loads(body)
        if o.get("error") == 0:
            print("  [PASS] 畸形包之后正常请求仍能正确响应")
        else:
            print("  [FAIL] 正常请求被影响: error=%s" % o.get("error"))
            ok = False
        s.close()
    except Exception as e:
        print("  [FAIL] 正常请求异常: %s" % e)
        ok = False

    print("\n  安全测试: %s" % ("通过" if ok else "失败"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())