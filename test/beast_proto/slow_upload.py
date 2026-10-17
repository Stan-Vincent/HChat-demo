#!/usr/bin/env python3
"""限速上传客户端：以固定速率持续发送 body（模拟慢网络）。
用于验证 GateServer 的 60s deadline 是"从 accept 起算"（旧，会杀死慢上传）
还是"按空闲时长"（新，慢但持续的上传能活）。

用法: slow_upload.py <port> <file> <rate_kb_per_sec>
"""
import socket
import sys
import time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 18080
path = sys.argv[2] if len(sys.argv) > 2 else "big300.bin"
rate_kb = float(sys.argv[3]) if len(sys.argv) > 3 else 3000.0   # KB/s

data = open(path, "rb").read()
host = "127.0.0.1"
slice_kb = 512

print(f"[cli] {host}:{port} body={len(data)} bytes rate={rate_kb} KB/s "
      f"-> 预计 {len(data)/1024/rate_kb:.0f}s")

s = socket.create_connection((host, port), timeout=300)
hdr = (
    f"POST /upload_file?uid=2&file_name={path} HTTP/1.1\r\n"
    f"Host: {host}:{port}\r\n"
    f"Content-Type: application/octet-stream\r\n"
    f"Content-Length: {len(data)}\r\n"
    f"\r\n"
).encode()
s.sendall(hdr)
print("[cli] headers sent, streaming body slowly ...")

t0 = time.time()
sent = 0
interval = slice_kb / rate_kb
try:
    while sent < len(data):
        chunk = data[sent:sent + slice_kb * 1024]
        s.sendall(chunk)
        sent += len(chunk)
        # 按速率节流：每发一片睡一片的时间
        target = t0 + sent / (rate_kb * 1024)
        delay = target - time.time()
        if delay > 0:
            time.sleep(delay)
        if sent % (16 * 1024 * 1024) < slice_kb * 1024:
            print(f"[cli] sent {sent/1024/1024:.0f}MB / {len(data)/1024/1024:.0f}MB "
                  f"({time.time()-t0:.0f}s)")
    print(f"[cli] body fully sent in {time.time()-t0:.0f}s, awaiting response ...")
except ConnectionResetError:
    print(f"[cli] !! KILLED by server after sending {sent/1024/1024:.0f}MB "
          f"({time.time()-t0:.0f}s)")
    sys.exit(1)

s.settimeout(300)
buf = b""
try:
    while b"\r\n\r\n" not in buf:
        c = s.recv(65536)
        if not c:
            print(f"[cli] !! peer closed after {len(buf)} bytes")
            sys.exit(1)
        buf += c
    head, _, rest = buf.partition(b"\r\n\r\n")
    print("[cli] response head:", head.decode(errors="replace").splitlines()[0])
    while len(rest) < 200:
        c = s.recv(65536)
        if not c:
            break
        rest += c
    print("[cli] response body:", rest[:200].decode(errors="replace"))
    print("[cli] === SLOW UPLOAD OK ===")
except socket.timeout:
    print("[cli] !! timed out")
    sys.exit(1)
finally:
    s.close()
