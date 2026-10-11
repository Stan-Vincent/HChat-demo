# -*- coding: utf-8 -*-
"""Fix the ConfigMgr config-dump logs: literal-less format + silly literal args."""
import os

ROOT = r"D:\QTcode\HChat"
FILES = ["GateServer/ConfigMgr.cpp", "ChatServer/ConfigMgr.cpp",
         "ChatServer2/ConfigMgr.cpp", "StatusServer/ConfigMgr.cpp"]

OLD_SEC = 'LOG_INFO("[ {} {} {} {}", section_name, "]", section_name, "]");'
NEW_SEC = 'LOG_INFO("[section {}]", section_name);'

OLD_KV = 'LOG_INFO("key_value_pair.first", "=", key_value_pair.second);'
NEW_KV = 'LOG_INFO("{} = {}", key_value_pair.first, key_value_pair.second);'


def read(p):
    raw = open(p, "rb").read()
    for enc in ("gbk", "utf-8-sig", "utf-8"):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return None, None


for rel in FILES:
    p = os.path.join(ROOT, rel.replace("/", os.sep))
    txt, enc = read(p)
    if txt is None:
        print("  [MISS]", rel)
        continue
    bom = open(p, "rb").read(3) == b'\xef\xbb\xbf'
    o = txt
    txt = txt.replace(OLD_SEC, NEW_SEC).replace(OLD_KV, NEW_KV)
    if txt != o:
        data = txt.encode(enc)
        if bom:
            data = b"\xef\xbb\xbf" + data
        open(p, "wb").write(data)
        print("  [ok]", rel)
    else:
        print("  [--]", rel, "unchanged")
print("done")