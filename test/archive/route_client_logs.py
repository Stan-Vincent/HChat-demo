# -*- coding: utf-8 -*-
"""Client: route qDebug/qWarning/qCritical to per-module QLoggingCategory."""
import re, os

CLIENT = r"D:\QTcode\HChat\Client"

# map source file -> category variable
CATEGORY = {
    'tcpmgr.cpp':       'lcNetwork',
    'httpmgr.cpp':      'lcNetwork',
    'uploadmanager.cpp':'lcNetwork',
    'chatdialog.cpp':   'lcChat',
    'chatpage.cpp':     'lcChat',
    'chatuserwid.cpp':  'lcChat',
    'chatuserlist.cpp': 'lcChat',
    'mainwindow.cpp':   'lcChat',
    'imagemanager.cpp': 'lcAvatar',
    'userinfopage.cpp': 'lcAvatar',
    'conuseritem.cpp':  'lcAvatar',
    'applyfrienditem.cpp': 'lcAvatar',
    'friendinfopage.cpp': 'lcAvatar',
    'logindialog.cpp':  'lcUser',
    'registerdialog.cpp': 'lcUser',
    'resetdialog.cpp':  'lcUser',
    'applyfriend.cpp':  'lcUser',
    'authenfriend.cpp': 'lcUser',
    'usermgr.cpp':      'lcUser',
    'contactuserlist.cpp': 'lcUser',
    'searchlist.cpp':   'lcUser',
}

LEVEL_MAP = {
    'qDebug':   'qCDebug',
    'qWarning': 'qCWarning',
    'qCritical':'qCCritical',
    'qInfo':    'qCInfo',
}


def read(p):
    raw = open(p, "rb").read()
    for enc in ("utf-8", "gbk"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return None


def write(p, txt):
    open(p, "w", encoding="utf-8", newline="").write(txt)


total = 0
for fn, cat in CATEGORY.items():
    p = os.path.join(CLIENT, fn)
    if not os.path.exists(p):
        continue
    txt = read(p)
    if txt is None or 'qDebug' not in txt:
        continue
    orig = txt

    # ensure include (idempotent)
    if '#include "logcategory.h"' not in txt:
        nl = "\r\n" if "\r\n" in txt else "\n"
        lines = txt.split(nl)
        last = max((i for i, ln in enumerate(lines[:60]) if ln.startswith('#include')),
                   default=0)
        lines.insert(last + 1, '#include "logcategory.h"')
        txt = nl.join(lines)

    nl = "\r\n" if "\r\n" in txt else "\n"
    for q, repl in LEVEL_MAP.items():
        txt = re.sub(r'\b%s\s*(?:\(\)|\(nullptr\))?\s*<<' % q,
                     '%s(%s) <<' % (repl, cat), txt)
    # bare qDebug("literal") form (no <<)
    for q, repl in LEVEL_MAP.items():
        txt = re.sub(r'\b%s\((?!<)' % q, '%s(%s) << ' % (repl, cat), txt)

    if txt != orig:
        write(p, txt)
        n = len(re.findall(r'qC(?:Debug|Warning|Critical|Info)\(%s\)' % cat, txt))
        print("  %-24s -> %-10s (%d)" % (fn, cat, n))
        total += n

print("total routed:", total)