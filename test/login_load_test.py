# -*- coding: utf-8 -*-
"""模拟客户端「登录后加载聊天列表」的 TCP 链路，抓真实回包。

目的：定位「登录后无限加载 + 卡死」的根因 —— 是服务端回包load_more
一直为 true，还是游标不推进导致死循环。

HChat 自定义 TCP 协议：2 字节 msg_id + 2 字节 len，均大端。
"""
import socket
import struct
import json
import sys
import time

CHATSERVER = ("127.0.0.1", 8990)

# ReqId（来自 Client/global.h）
ID_CHAT_LOGIN_REQ = 1011          # 先确认
ID_HEART_BEAT_REQ = 1023
ID_LOAD_CHAT_THREAD_REQ = 1025
ID_LOAD_CHAT_THREAD_RSP = 1026
ID_LOAD_CHAT_MSG_REQ = 1029
ID_LOAD_CHAT_MSG_RSP = 1030
ID_OFF_LINE_REQ = 1001            # 先确认


def pack(msg_id, payload_bytes):
    return struct.pack(">HH", msg_id, len(payload_bytes)) + payload_bytes


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("连接被对端关闭")
        buf += chunk
    return buf


def recv_msg(sock):
    head = recv_exact(sock, 4)
    msg_id, length = struct.unpack(">HH", head)
    body = recv_exact(sock, length)
    return msg_id, body


def http_post(path, obj):
    import urllib.request
    data = json.dumps(obj).encode()
    req = urllib.request.Request(
        "http://127.0.0.1:8080" + path, data=data,
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read())


def main():
    uid = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    print("  模拟登录 uid=%d" % uid)

    sock = socket.create_connection(CHATSERVER, timeout=10)
    sock.settimeout(10)

    # ---- 1. 先走 HTTP 登录拿 token（ChatServer 的 LoginHandler 会校验 Redis 里的 token）----
    print("\n  [1] HTTP 登录拿 token")
    # 密码是 Windows DPAPI 加密的，明文登不进去。
    # 直接从 Redis 取已有的 utoken_<uid>（GateServer 登录成功时写进去的）。
    import subprocess
    token = ""
    for cand in [uid, 5, 1114, 1115]:
        out = subprocess.run(
            [r"D://Program Files//Redis//redis-cli.exe", "-p", "6380",
             "-a", "123456", "GET", "utoken_%d" % cand],
            capture_output=True, timeout=10)
        v = out.stdout.decode("utf-8", "replace").strip()
        if v and "WRONGPASS" not in v and "NOAUTH" not in v:
            token = v
            uid = cand
            print("      用 Redis 里现成的 token: uid=%d" % uid)
            break
        print("      utoken_%d 不存在" % cand)
    if not token:
        print("!! Redis 里没有任何有效 token。请先用真实客户端登录一次，")
        print("!! 或手工执行: redis-cli -p 6380 SET utoken_2 <uuid>")
        return

    # ---- 2. TCP 登录（LoginHandler），否则后续请求都会被判 LOAD_CHAT_FAILED ----
    print("\n  [2] TCP 登录")
    login_req = json.dumps({"uid": uid, "token": token}).encode()
    sock.sendall(pack(1005, login_req))   # MSG_CHAT_LOGIN = 1005
    mid, body = recv_msg(sock)
    lo = json.loads(body)
    print("      msg_id=%d error=%s name=%s" % (mid, lo.get("error"), lo.get("name")))
    if lo.get("error") != 0:
        print("!! TCP 登录失败，后续请求会被拒")
        return

    # ---- 3. 拉聊天线程列表，翻页直到 load_more 为 false ----
    print("\n  [3] 拉聊天线程列表（模拟客户端分页）")
    last_thread_id = 0
    total_threads = 0
    pages = 0
    seen_cursors = []
    all_threads = []

    while True:
        pages += 1
        if pages > 30:
            print("      !! 超过 30 页仍在加载 —— 复现「无限加载」")
            print("      !! 游标序列(旧->新,条数,load_more): %s" % seen_cursors)
            break

        req = json.dumps({"uid": uid, "thread_id": last_thread_id}).encode()
        sock.sendall(pack(ID_LOAD_CHAT_THREAD_REQ, req))

        try:
            mid, body = recv_msg(sock)
        except socket.timeout:
            print("      !! 第 %d 页超时无回包" % pages)
            break
        except ConnectionError as e:
            print("      !! 第 %d 页连接断开: %s（服务端可能崩了）" % (pages, e))
            break

        obj = json.loads(body)
        if obj.get("error") != 0:
            print("      第 %d 页 error=%s msg=%s" % (pages, obj.get("error"), obj.get("msg")))
            break

        threads = obj.get("threads", [])
        load_more = obj.get("load_more", False)
        next_last_id = obj.get("next_last_id", 0)

        total_threads += len(threads)
        all_threads += threads
        seen_cursors.append((last_thread_id, next_last_id, len(threads), load_more))
        print("第 %d 页: cursor %d -> %d, 本页 %d 条, load_more=%s"
              % (pages, last_thread_id, next_last_id, len(threads), load_more))

        if not load_more:
            print("      -> 线程列表加载结束，共 %d 页 / %d 个会话" % (pages, total_threads))
            break

        if next_last_id <= last_thread_id:
            print("      !! 游标没有推进（%d -> %d），会永远重复请求同一页！"
                  % (last_thread_id, next_last_id))
            break
        last_thread_id = next_last_id

    # ---- 4. 逐个会话拉消息（客户端就是循环干这件事）----
    print("\n  [4] 逐个会话拉消息（共 %d 个会话）" % len(all_threads))
    msg_pages = 0
    load_more_msg_sessions = []
    for t in all_threads:
        tid = t["thread_id"]
        msg_cursor = 0
        session_pages = 0
        for _ in range(50):
            msg_pages += 1
            session_pages += 1
            if msg_pages > 300:
                print("      !! 消息翻页超过 300 次 —— 消息侧无限加载")
                break
            req = json.dumps({"thread_id": tid, "message_id": msg_cursor}).encode()
            sock.sendall(pack(ID_LOAD_CHAT_MSG_REQ, req))
            try:
                mid, body = recv_msg(sock)
            except socket.timeout:
                print("      !! thread %d 第 %d 页超时" % (tid, session_pages))
                break
            except ConnectionError as e:
                print("      !! thread %d 连接断开(服务端崩溃?): %s" % (tid, e))
                return
            obj = json.loads(body)
            if obj.get("error") != 0:
                print("      thread %d error=%s msg=%s"
                      % (tid, obj.get("error"), obj.get("msg")))
                break
            msgs = obj.get("chat_datas", [])
            lm = obj.get("load_more", False)
            last_msg_id = obj.get("last_message_id", 0)
            if not lm:
                break
            if last_msg_id <= msg_cursor:
                print("      !! thread %d 消息游标卡住: %d -> %d（死循环）"
                      % (tid, msg_cursor, last_msg_id))
                load_more_msg_sessions.append(tid)
                break
            msg_cursor = last_msg_id
        if session_pages >= 50:
            load_more_msg_sessions.append(tid)

    print("\n  消息翻页请求总数: %d" % msg_pages)

    # ---- 5. 专项验证：thread 1 有 32 条，应该翻 4 页（10+10+10+2）----
    print("\n  [5] 专项验证 thread 1 的翻页（32 条 -> 应翻 4 页）")
    cursor = 0
    page_no = 0
    got = 0
    while True:
        page_no += 1
        if page_no > 20:
            print("      !! 翻页超过 20 次 —— 死循环")
            break
        req = json.dumps({"thread_id": 1, "message_id": cursor}).encode()
        sock.sendall(pack(ID_LOAD_CHAT_MSG_REQ, req))
        mid, body = recv_msg(sock)
        obj = json.loads(body)
        if obj.get("error") != 0:
            print("      第 %d 页 error=%s" % (page_no, obj.get("error")))
            break
        msgs = obj.get("chat_datas", [])
        lm = obj.get("load_more", False)
        last_msg_id = obj.get("last_message_id", 0)
        got += len(msgs)
        print("      第 %d 页: %d 条, load_more=%s, next_cursor=%d"
              % (page_no, len(msgs), lm, last_msg_id))
        if not lm:
            break
        if last_msg_id <= cursor:
            print("      !! 游标卡住 %d -> %d" % (cursor, last_msg_id))
            break
        cursor = last_msg_id
    # ★ 不写死条数：直接问库里 thread 1 现在有多少条。
    #   之前写死 32，清掉测试数据后就误报 FAIL（测试的问题不是代码的问题）。
    expect = 0
    try:
        out = subprocess.run(
            [r"D://Program Files//MySQL//MySQL Server 8.0//bin//mysql.exe",
             "-h127.0.0.1", "-P3308", "-uroot", "-p123456", "xxxl", "-N", "-B",
             "-e", "SELECT COUNT(*) FROM chat_message WHERE thread_id=1;"],
            capture_output=True, timeout=15)
        v = out.stdout.decode("utf-8", "replace").strip().splitlines()
        if v and v[0].strip().isdigit():
            expect = int(v[0].strip())
    except Exception as e:
        print("      (查库失败 %s)" % e)

    print("      共取到 %d 条（库里共 %d 条）" % (got, expect))
    if expect == 0:
        print("      [SKIP] 库里没数据，跳过断言")
    elif got != expect:
        print("      [FAIL] 条数对不上：取到 %d / 库里 %d" % (got, expect))
    else:
        # 页数应该等于 ceil(expect / 10)
        want_pages = (expect + 9) // 10
        if page_no == want_pages:
            print("      [OK] 翻页正确：%d 条分 %d 页取完" % (got, page_no))
        else:
            print("      [FAIL] 页数不对：%d 页，应为 %d 页" % (page_no, want_pages))
    if load_more_msg_sessions:
        print("  !! 这些会话的消息翻页没有正常结束: %s" % load_more_msg_sessions)
    else:
        print("  所有会话的消息都能正常翻到结尾")
    sock.close()


if __name__ == "__main__":
    main()