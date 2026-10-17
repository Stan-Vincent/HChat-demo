# -*- coding: utf-8 -*-
"""测试脚本共用的 MySQL 连接信息：直接从 GateServer/config.ini 的 [Mysql] 段读。

★ 为什么要有这个文件
    gate_e2e_test.py 里原本把口令写死成 `-p123456`，而真实口令是 Hcat@2026Secure。
    结果不是报错，而是**静默降级**：pick_uid() 连不上库 -> 退回 uid=2，
    "备份原资料"也失败 -> 测完不还原，把用户资料改成 e2e_* 就跑了。
    这种"测试通过了但其实啥也没验"比直接失败更坑。所以统一来这里问 config.ini 要。

★ configparser 会把 option 名小写化，所以下面一律用小写 key 取。
"""
import configparser
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 没在 PATH 里就用绝对路径；两台机器装的位置不一样时改这一行。
MYSQL_EXE = r"D:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe"


def cfg():
    """返回 dict(host, port, user, passwd, schema)"""
    cp = configparser.ConfigParser()
    path = os.path.join(ROOT, "GateServer", "config.ini")
    try:
        cp.read(path, encoding="utf-8")
    except Exception:
        pass
    s = cp["Mysql"] if cp.has_section("Mysql") else {}
    return {
        "host":   s.get("host", "127.0.0.1").strip(),
        "port":   s.get("port", "3308").strip(),
        "user":   s.get("user", "root").strip(),
        "passwd": s.get("passwd", "").strip(),
        "schema": s.get("schema", "Hcat").strip(),
    }


def query(sql, timeout=15):
    """执行一条 SQL，返回 stdout 文本；失败返回 ''。"""
    c = cfg()
    cmd = [MYSQL_EXE,
           "-h%s" % c["host"], "-P%s" % c["port"],
           "-u%s" % c["user"], "-p%s" % c["passwd"],
           c["schema"], "-N", "-B", "-e", sql]
    try:
        out = subprocess.run(cmd, capture_output=True, timeout=timeout)
        return out.stdout.decode("utf-8", "replace").strip()
    except Exception:
        return ""


def pick_uid(default=2):
    """取一个库里真实存在的 uid。写死 uid=1 会让 UPDATE 匹配 0 行 -> 假失败。"""
    v = query("SELECT uid FROM user ORDER BY id LIMIT 1;")
    if v.splitlines() and v.splitlines()[0].strip().isdigit():
        return int(v.splitlines()[0].strip())
    return default


def pick_email(uid=None, default="0@1.com"):
    """取一个库里真实存在的邮箱，供 HTTP 登录用。

    ★ protocol_test.py 之前写死 test@test.com，而库里 uid=2 的实际邮箱是
      0@1.com —— 28 个用例会以「登录失败」全军覆没，第一眼看起来像服务端崩了。
      测试账号换过几次邮箱，所以别再写死，问库要。
    """
    if uid is None:
        uid = pick_uid()
    v = query("SELECT email FROM user WHERE uid=%d;" % uid)
    if v.splitlines() and v.splitlines()[0].strip():
        return v.splitlines()[0].strip()
    return default
