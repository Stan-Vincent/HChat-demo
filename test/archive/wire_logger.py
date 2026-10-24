# -*- coding: utf-8 -*-
"""1) ensure every .cpp using LOG_* includes logger.h
   2) ensure every service entry calls hchat::log::init(<module>)"""
import re, os

ROOT = r"D:\QTcode\HChat"
ENTRY = {
    "ChatServer":  ("ChatServer.cpp",  "chat1"),
    "ChatServer2": ("ChatServer.cpp",  "chat2"),
    "StatusServer": ("StatusServer.cpp", "status"),
    "GateServer":  ("GateServer.cpp",  "gate"),
}


def read(p):
    raw = open(p, "rb").read()
    for enc in ("gbk", "utf-8"):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return None, None


def write(p, txt, enc):
    open(p, "wb").write(txt.encode(enc))


# ---- 1) includes ----
for svc in ENTRY:
    d = os.path.join(ROOT, svc)
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".cpp"):
            continue
        p = os.path.join(d, fn)
        txt, enc = read(p)
        if txt is None:
            continue
        uses = re.search(r'\bLOG_(TRACE|DEBUG|INFO|WARN|ERROR)\s*\(', txt)
        if not uses:
            continue
        if '#include "logger.h"' in txt:
            continue
        nl = "\r\n" if "\r\n" in txt else "\n"
        lines = txt.split(nl)
        last = 0
        for i, ln in enumerate(lines[:80]):
            if ln.startswith('#include'):
                last = i
        if last:
            lines.insert(last + 1, '#include "logger.h"')
            write(p, nl.join(lines), enc)
            print("  [inc] %s/%s" % (svc, fn))

# ---- 2) init in entry points ----
for svc, (entry, module) in ENTRY.items():
    p = os.path.join(ROOT, svc, entry)
    if not os.path.exists(p):
        print("  [MISS] entry", svc, entry)
        continue
    txt, enc = read(p)
    if 'hchat::log::init' in txt:
        print("  [--] init already present:", svc)
        continue
    if '#include "logger.h"' not in txt:
        nl0 = "\r\n" if "\r\n" in txt else "\n"
        lines = txt.split(nl0)
        last = 0
        for i, ln in enumerate(lines[:80]):
            if ln.startswith('#include'):
                last = i
        lines.insert(last + 1, '#include "logger.h"')
        txt = nl0.join(lines)
    nl = "\r\n" if "\r\n" in txt else "\n"
    # insert right after main() { opening brace
    new = re.sub(r'(int\s+main\s*\([^)]*\)\s*\r?\n\{\r?\n)',
                 r'\1    hchat::log::init("%s");' % module + nl + nl,
                 txt, count=1)
    if new == txt:
        print("  [FAIL] cannot locate main() in", svc)
        continue
    # add graceful-exit logging right after io_context.run()
    new = re.sub(r'(\r?\n)(\s*)io_context\.run\(\);',
                 r'\1\2io_context.run();' + nl + r'\2LOG_INFO("%s exited gracefully");'
                 % svc + nl + r'\2hchat::log::shutdown();',
                 new, count=1)
    write(p, new, enc)
    print("  [init] %s -> %s" % (svc, module))
print("done")