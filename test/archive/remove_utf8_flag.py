# -*- coding: utf-8 -*-
"""移除 vcxproj 里的 /utf-8 —— GBK 源码加它会导致中文注释乱码警告。
spdlog 的 unicode 需求已由 common/logger.h 里的 FMT_UNICODE=0 解决。"""
import os

PROJ = [
    r"D:\QTcode\HChat\GateServer\GateServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer\ChatServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer2\ChatServer.vcxproj",
    r"D:\QTcode\HChat\StatusServer\StatusServer.vcxproj",
]

for path in PROJ:
    raw = open(path, "rb").read()
    try:
        txt = raw.decode("utf-8")
    except UnicodeDecodeError:
        txt = raw.decode("gbk")
    target = "\r\n      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>"
    target_lf = "\n      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>"
    n = txt.count(target) + txt.count(target_lf)
    if n == 0:
        print("  [skip]", os.path.basename(os.path.dirname(path)))
        continue
    txt = txt.replace(target, "").replace(target_lf, "")
    try:
        out = txt.encode("utf-8")
    except Exception:
        out = txt.encode("gbk")
    open(path, "wb").write(out)
    print("  [ok] removed /utf-8 x%d in %s" % (n, os.path.basename(os.path.dirname(path))))
print("done")