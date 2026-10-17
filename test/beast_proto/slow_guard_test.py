#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证"防慢慢传"的两道闸门真的会触发。

背景：kMaxUploadBytes（上传上限）只挡住了"文件多大"，挡不住"传多慢"。
一个稳定以 20KB/s 传的对端每 5 秒都有数据进来，永远不会触发 idle 超时，
300MB 能拖 4 小时，连接/文件句柄/磁盘 .part 全程占着。所以补了两道闸门：
    ② 最低速率：连续 kSlowPatrolLimit(3) 次巡检平均速率 < MinBytesPerSec 就断开
    ③ 总时长：单次上传超过 MaxDurationSec 就断开（抓"量大 + 中等慢"）

用例：
    1 慢速（10KB/s，下限 64KB/s）-> 约 15s 内被断开，且服务端日志出现 "too slow"
    2 总时长（上限 3s，速率 1MB/s 不算慢）-> 约 3s 被断开，日志 "upload exceeded"
    3 对照组：正常速率上传不受影响（不能被误杀）
    4 被掐断的上传不能留下 .part 垃圾

用法：python3 slow_guard_test.py
"""
import os
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "mock_gate.exe")
OUT = os.path.join(HERE, "mock_upload")
PORT = 18085

results = []


def record(name, ok, detail):
    results.append((name, ok))
    print(("  [PASS] " if ok else "  [FAIL] ") + name + "  " + detail, flush=True)


def paced_post(port, path, total, bytes_per_sec):
    """按指定速率发一条原始 body 的 POST。
    返回 (kind, payload, cost)：kind = 'resp' 收到响应 / 'err' 连接被断"""
    s = socket.create_connection(("127.0.0.1", port), timeout=90)
    t0 = time.time()
    try:
        s.sendall(("POST %s HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Content-Type: application/octet-stream\r\n"
                   "Content-Length: %d\r\nConnection: close\r\n\r\n"
                   % (path, total)).encode())
        tick = 0.1
        per = max(1, int(bytes_per_sec * tick))
        blob = b"x" * per
        sent = 0
        while sent < total:
            n = min(per, total - sent)
            s.sendall(blob[:n])
            sent += n
            time.sleep(tick)
        # 发完了，等着收响应
        buf = b""
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
        return "resp", buf.decode("utf-8", "replace"), time.time() - t0
    except Exception as e:
        return "err", repr(e), time.time() - t0
    finally:
        s.close()


def wait_up():
    for _ in range(60):
        try:
            c = socket.create_connection(("127.0.0.1", PORT), timeout=1)
            c.close()
            return True
        except OSError:
            time.sleep(0.2)
    return False


def run_case(label, cap, min_bps, max_dur, total, rate, expect_killed, expect_log, budget):
    """起一次 mock，跑一条用例，返回是否通过"""
    for f in os.listdir(OUT) if os.path.isdir(OUT) else []:
        try:
            os.remove(os.path.join(OUT, f))
        except OSError:
            pass
    log = os.path.join(HERE, "slow_case.log")
    f = open(log, "wb")
    srv = subprocess.Popen([EXE, str(PORT), OUT, str(cap), str(min_bps), str(max_dur)],
                           stdout=f, stderr=subprocess.STDOUT)
    try:
        if not wait_up():
            return False, "服务没起来"
        kind, payload, cost = paced_post(
            PORT, "/upload_file?uid=2&file_name=slow.bin", total, rate)
        time.sleep(0.3)
    finally:
        srv.terminate()
        try:
            srv.wait(timeout=5)
        except subprocess.TimeoutExpired:
            srv.kill()
        f.close()

    srvlog = open(log, "rb").read().decode("utf-8", "replace")
    killed = (kind == "err")
    parts = [x for x in (os.listdir(OUT) if os.path.isdir(OUT) else []) if x.endswith(".part")]

    ok = True
    if expect_killed:
        ok = killed and cost < budget
    else:
        ok = (not killed) and ("error\":0" in payload or '"error": 0' in payload)
    if expect_log and expect_log not in srvlog:
        ok = False
    if parts:
        ok = False

    detail = "%.1fs killed=%s log_hit=%s parts=%d" % (
        cost, killed, (expect_log in srvlog) if expect_log else "-", len(parts))
    record(label, ok, detail)
    if not ok:
        print("        payload=%s" % payload[:160])
        print("        srvlog=%s" % srvlog.strip().replace("\n", " | ")[:400])
    return ok


def main():
    if not os.path.exists(EXE):
        print("! 先编译 mock_gate.exe")
        return 1
    os.makedirs(OUT, exist_ok=True)

    print("=== 1 慢速闸门：10KB/s 发送，下限 64KB/s ===")
    # 巡检 5s 一次，连续 3 次 -> 约 15s 被掐；给 30s 预算
    run_case("1 慢速上传被掐断", 300 * 1024 * 1024, 64 * 1024, 3600,
             2 * 1024 * 1024, 10 * 1024, True, "too slow", 30)

    print("=== 2 总时长闸门：上限 3s，速率 1MB/s（不算慢） ===")
    run_case("2 超过总时长被掐断", 300 * 1024 * 1024, 64 * 1024, 3,
             20 * 1024 * 1024, 1024 * 1024, True, "upload exceeded", 12)

    print("=== 3 对照组：正常速率不能被误杀 ===")
    run_case("3 正常速率上传成功", 300 * 1024 * 1024, 64 * 1024, 3600,
             2 * 1024 * 1024, 8 * 1024 * 1024, False, None, 30)

    bad = [n for n, ok in results if not ok]
    print("\n==== %d/%d passed ====" % (len(results) - len(bad), len(results)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
