#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mock_gate.exe 的冒烟测试。

用第 3 个 argv 把上传上限临时压到 1MB，这样"超限拒绝"这条路径
不用真传 300MB 也能验证；默认上限仍是真实服务端的 300MB。

用例：
  1 小文件上传成功 + 内容逐字节比对
  2 可执行扩展名被中和成 .bin
  3 缺 file_name
  4 Content-Type 是 json
  5 空文件
  6 超限（声明长度超限 -> 走 drain 路径，客户端仍能读到 JSON 错误）
  7 文件名带中文/空格  -> URL 编码不被破坏
  8 两路并发上传

用法：python3 mock_smoke.py
"""
import http.client
import json
import os
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "mock_gate.exe")
OUT = os.path.join(HERE, "mock_upload")
PORT = 18081
CAP = 1024 * 1024          # 1MB，加速超限用例
SLEEP_UP = 0.8

results = []


def record(name, ok, detail):
    results.append((name, ok, detail))
    print(("  [PASS] " if ok else "  [FAIL] ") + name + "  " + detail, flush=True)


def post_raw(path, body, ctype="application/octet-stream", host="127.0.0.1"):
    """发一条原始 body 的 POST，返回 (status, parsed_json_or_None, raw_text)"""
    conn = http.client.HTTPConnection(host, PORT, timeout=120)
    try:
        conn.putrequest("POST", path, skip_accept_encoding=True)
        conn.putheader("Content-Type", ctype)
        conn.putheader("Content-Length", str(len(body)))
        conn.endheaders()
        # 分块发送，模拟真客户端边读边发
        for i in range(0, len(body), 65536):
            conn.send(body[i:i + 65536])
        resp = conn.getresponse()
        raw = resp.read().decode("utf-8", "replace")
        try:
            return resp.status, json.loads(raw), raw
        except Exception:
            return resp.status, None, raw
    finally:
        conn.close()


def fnv1a(data: bytes) -> str:
    h = 1469598103934665603
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return "%016x" % h


def wait_server():
    for _ in range(60):
        try:
            c = http.client.HTTPConnection("127.0.0.1", PORT, timeout=2)
            c.request("GET", "/__ping")
            c.getresponse().read()
            c.close()
            return True
        except Exception:
            time.sleep(0.2)
    return False


def main():
    if not os.path.exists(EXE):
        print("! 找不到 mock_gate.exe，先编译")
        return 1

    # 造数据
    small = bytes(range(256)) * 40                 # 10240 B
    big = os.urandom(2 * 1024 * 1024)              # 2MB，用于超限用例
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        try:
            os.remove(os.path.join(OUT, f))
        except OSError:
            pass

    print("[srv] starting mock_gate on %d, cap=%d" % (PORT, CAP))
    srv = subprocess.Popen([EXE, str(PORT), OUT, str(CAP)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_server():
            print("! 服务没起来")
            return 1
        print("[srv] up\n")

        # 1 小文件上传成功 + 内容比对
        st, js, raw = post_raw("/upload_file?uid=2&file_name=report.pdf", small)
        ok = (st == 200 and js and js.get("error") == 0
              and js.get("size") == len(small))
        uname = js.get("unique_name") if js else None
        detail = "size=%d name=%s" % (js.get("size", -1) if js else -1, uname)
        if ok and uname:
            p = os.path.join(OUT, uname)
            ok = os.path.exists(p) and open(p, "rb").read() == small
            ok = ok and uname.endswith(".pdf") and js.get("md5") == fnv1a(small)
        record("1 小文件上传+内容+哈希+扩展名", ok, detail)

        # 2 可执行扩展名 -> .bin
        st, js, raw = post_raw("/upload_file?uid=2&file_name=setup.exe", small)
        ok = (st == 200 and js and js.get("error") == 0)
        uname = js.get("unique_name") if js else None
        ok = ok and bool(uname) and uname.endswith(".bin")
        record("2 .exe 被中和为 .bin", ok, str(uname))

        # 3 缺 file_name
        st, js, raw = post_raw("/upload_file?uid=2", small)
        ok = (st == 200 and js and js.get("error") == 1001
              and js.get("msg") == "missing file_name")
        record("3 缺 file_name 返回 JSON 错误", ok, raw.strip()[:80])

        # 4 Content-Type json
        st, js, raw = post_raw("/upload_file?uid=2&file_name=a.txt", small,
                               ctype="application/json")
        ok = (st == 200 and js and js.get("error") == 1001
              and "not application/json" in (js.get("msg") or ""))
        record("4 json content-type 被拒", ok, raw.strip()[:80])

        # 5 空文件
        st, js, raw = post_raw("/upload_file?uid=2&file_name=empty.txt", b"")
        ok = (st == 200 and js and js.get("error") == 1001
              and js.get("msg") == "empty file")
        record("5 空文件被拒", ok, raw.strip()[:80])

        # 6 超限（声明长度超限 -> drain 后仍读到 JSON）
        t0 = time.time()
        st, js, raw = post_raw("/upload_file?uid=2&file_name=huge.zip", big)
        cost = time.time() - t0
        ok = (st == 200 and js and js.get("error") == 1001
              and "too large" in (js.get("msg") or ""))
        leftover = [f for f in os.listdir(OUT) if f.endswith(".part")]
        ok = ok and not leftover
        record("6 超限：drain 后返回 JSON 错误，无 .part 残留",
               ok, "%.2fs %s parts=%d" % (cost, raw.strip()[:60], len(leftover)))

        # 7 中文 + 空格文件名
        name = "我的 报告 2026.pdf"
        from urllib.parse import quote
        st, js, raw = post_raw("/upload_file?uid=2&file_name=" + quote(name), small)
        ok = (st == 200 and js and js.get("error") == 0
              and js.get("size") == len(small))
        uname = js.get("unique_name") if js else None
        ok = ok and bool(uname) and uname.endswith(".pdf")
        record("7 中文/空格文件名", ok, str(uname))

        # 8 并发两路
        def one(i):
            d = os.urandom(300 * 1024) * (i + 1)
            return post_raw("/upload_file?uid=2&file_name=c%d.dat" % i, d), d

        with ThreadPoolExecutor(max_workers=2) as ex:
            futs = [ex.submit(one, i) for i in range(2)]
            outs = [f.result() for f in futs]
        ok = True
        for i, ((st, js, raw), d) in enumerate(outs):
            if not (js and js.get("error") == 0 and js.get("size") == len(d)):
                ok = False
                continue
            p = os.path.join(OUT, js["unique_name"])
            if not (os.path.exists(p) and open(p, "rb").read() == d):
                ok = False
        record("8 两路并发上传", ok, "")

    finally:
        srv.terminate()
        try:
            srv.wait(timeout=5)
        except subprocess.TimeoutExpired:
            srv.kill()

    bad = [n for n, ok, _ in results if not ok]
    print("\n==== %d/%d passed ====" % (len(results) - len(bad), len(results)))
    if bad:
        for n in bad:
            print("  FAILED: " + n)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
