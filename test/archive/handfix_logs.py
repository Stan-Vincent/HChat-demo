# -*- coding: utf-8 -*-
"""Final hand-fixes on known-bad LOG_ calls found during review."""
import re, os

ROOT = r"D:\QTcode\HChat"

# (relative path, exact old fragment, new fragment)
FIXES = [
    # 1) duplicate identical args
    ("ChatServer/MysqlDao.cpp",
     'LOG_INFO("Check Email:  {}", res->getString("email"), res->getString("email"));',
     'LOG_INFO("check email exists: {}", res->getString("email"));'),
    ("ChatServer2/MysqlDao.cpp",
     'LOG_INFO("Check Email:  {}", res->getString("email"), res->getString("email"));',
     'LOG_INFO("check email exists: {}", res->getString("email"));'),
    ("StatusServer/MysqlDao.cpp",
     'LOG_INFO("Check Email:  {}", res->getString("email"), res->getString("email"));',
     'LOG_INFO("check email exists: {}", res->getString("email"));'),

    # 2) password check: no uid in scope -> log nothing extra, keep it a warning
    ("GateServer/MysqlDao.cpp",
     'LOG_WARN("password mismatch check for user {} (origin pwd hidden)", "<redacted>");',
     'LOG_WARN("verifying password for email={} (origin pwd hidden)", email);'),
    ("ChatServer/MysqlDao.cpp",
     'LOG_WARN("password mismatch check for user {} (origin pwd hidden)", "<redacted>");',
     'LOG_WARN("verifying password (origin pwd hidden)");'),
    ("ChatServer2/MysqlDao.cpp",
     'LOG_WARN("password mismatch check for user {} (origin pwd hidden)", "<redacted>");',
     'LOG_WARN("verifying password (origin pwd hidden)");'),
    ("StatusServer/MysqlDao.cpp",
     'LOG_WARN("password mismatch check for user {} (origin pwd hidden)", "<redacted>");',
     'LOG_WARN("verifying password (origin pwd hidden)");'),
]

# Generic cleanups: quoted-literal args passed as format arguments
GENERIC = [
    (r', "<redacted>"\)', r')'),
]


def read(p):
    raw = open(p, "rb").read()
    for enc in ("gbk", "utf-8"):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return None, None


total = 0
for rel, old, new in FIXES:
    p = os.path.join(ROOT, rel.replace("/", os.sep))
    txt, enc = read(p)
    if txt is None:
        print("  [MISS] unreadable", rel)
        continue
    if old in txt:
        txt = txt.replace(old, new)
        open(p, "wb").write(txt.encode(enc))
        print("  [ok]", rel, "::", old[:52])
        total += 1
    else:
        print("  [--] not found in", rel, "::", old[:52])

# generic sweep for leftover quoted-literal args
for svc in ["GateServer", "ChatServer", "ChatServer2", "StatusServer"]:
    d = os.path.join(ROOT, svc)
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".cpp"):
            continue
        p = os.path.join(d, fn)
        txt, enc = read(p)
        if txt is None:
            continue
        o = txt
        for pat, rep in GENERIC:
            txt = re.sub(pat, rep, txt)
        if txt != o:
            open(p, "wb").write(txt.encode(enc))
            print("  [sweep]", svc + "/" + fn)
            total += 1
print("total fixes:", total)