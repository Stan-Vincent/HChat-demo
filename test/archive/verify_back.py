# -*- coding: utf-8 -*-
"""One-shot verification: start ChatServer1, log in as uid 5, dump friend_list back fields."""
import json
import socket
import struct
import subprocess
import sys
import time

CHAT_HOST, CHAT_PORT = "127.0.0.1", int(sys.argv[1]) if len(sys.argv) > 1 else 8990
REDIS_HOST, REDIS_PORT, REDIS_PWD = "127.0.0.1", 6380, "123456"
CHAT_DIR = r"D:\QTcode\HChat\ChatServer"
CHAT_EXE = r"D:\QTcode\HChat\ChatServer\x64\Debug\ChatServer.exe"


def redis_get(key: str) -> str | None:
    """Minimal RESP client: AUTH + GET."""
    s = socket.create_connection((REDIS_HOST, REDIS_PORT), timeout=5)
    f = s.makefile("rb")
    s.sendall(f"AUTH {REDIS_PWD}\r\n".encode())
    f.readline()  # +OK
    s.sendall(f"GET {key}\r\n".encode())
    line = f.readline().decode(errors="replace")
    if line.startswith("$"):
        n = int(line[1:].strip())
        if n == -1:
            s.close()
            return None
        data = f.read(n)
        f.read(2)  # trailing CRLF
        s.close()
        return data.decode(errors="replace")
    s.close()
    return None


def start_chatserver() -> None:
    # NOTE: must redirect stdio, otherwise the started ChatServer inherits the
    # parent's stdout pipe and subprocess.communicate() blocks until it exits.
    subprocess.Popen(
        ["cmd", "/c", "start", "VerifyChat", "/D", CHAT_DIR, CHAT_EXE],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        stdin=subprocess.DEVNULL,
    )
    # wait for port
    for _ in range(30):
        try:
            socket.create_connection((CHAT_HOST, CHAT_PORT), timeout=0.5).close()
            return
        except OSError:
            time.sleep(0.5)
    raise RuntimeError("ChatServer did not listen on 8990")


def recv_exact(s: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("connection closed")
        buf += chunk
    return buf


def main() -> None:
    # ChatServer is started separately (background bash task); this script only
    # reads the token from Redis and drives the login request/response.
    uid = 5
    token = redis_get(f"utoken_{uid}")
    print(f"[redis] utoken_{uid} = {token!r}")
    if not token:
        print("[FATAL] no token in redis; user must log in via client first")
        return

    # Protocol (see MsgNode.h/CSession.cpp): 2-byte msg_id (network order)
    # + 2-byte body length (network order, uint16) + JSON body.
    body = json.dumps({"uid": uid, "token": token}).encode()
    pkt = struct.pack(">h", 1005) + struct.pack(">H", len(body)) + body

    s = socket.create_connection((CHAT_HOST, CHAT_PORT), timeout=5)
    s.sendall(pkt)
    print(f"[send] MSG_CHAT_LOGIN uid={uid}, body_len={len(body)}, port={CHAT_PORT}")

    msg_id = struct.unpack(">h", recv_exact(s, 2))[0]
    length = struct.unpack(">H", recv_exact(s, 2))[0]
    data = recv_exact(s, length)
    print(f"[recv] msg_id={msg_id} len={length}")
    root = json.loads(data.decode(errors="replace"))
    print(f"[rsp ] error={root.get('error')}")
    fl = root.get("friend_list", [])
    print(f"[rsp ] friend_list size={len(fl)}")
    print("-" * 46)
    print(f"{'uid':>4}  {'name':<12} {'nick':<12} back")
    print("-" * 46)
    for ele in fl:
        print(f"{ele.get('uid'):>4}  {ele.get('name',''):<12} {ele.get('nick',''):<12} {ele.get('back','')}")
    print("-" * 46)
    s.close()


if __name__ == "__main__":
    main()
