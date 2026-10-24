# -*- coding: utf-8 -*-
"""全项目编码体检：每个源文件到底是 UTF-8 还是 GBK，有没有被腐蚀（U+FFFD）。

判定：
  - utf8_ok   : 能按 UTF-8 解码
  - has_fffd  : UTF-8 解码后出现替换字符 -> 曾被按错误编码写过（注释已烂）
  - gbk_ok    : 能按 GBK 解码
分类：
  UTF8        utf8_ok 且无 ffd
  GBK         utf8 失败、gbk 成功
  腐蚀        utf8 成功但含 ffd（原本是 GBK，被当 UTF-8 改过）
  纯ASCII     无中文，两种编码一样
  混合/异常   两种都失败
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIR = {'.git', '.vs', 'x64', 'Debug', 'Release', 'build', 'node_modules',
            'archive', '_db_backup', '_res_backup', '__pycache__', '_logs',
            'res', 'third_party', 'logs'}
EXTS = {'.h', '.cpp', '.cc', '.ui', '.pro', '.pri', '.py', '.md', '.bat',
        '.js', '.json', '.ini', '.sh', '.ps1', '.qrc'}
SKIP_FILE = {'message.pb.cc', 'message.pb.h', 'message.grpc.pb.cc', 'message.grpc.pb.h'}


def has_cn(b):
    return any(c > 127 for c in b)


rows = []
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIR]
    for fn in filenames:
        if fn in SKIP_FILE:
            continue
        ext = os.path.splitext(fn)[1].lower()
        if ext not in EXTS:
            continue
        p = os.path.join(dirpath, fn)
        rel = os.path.relpath(p, ROOT)
        try:
            b = open(p, 'rb').read()
        except Exception:
            continue
        if not has_cn(b):
            rows.append(('ASCII', rel, ''))
            continue
        u8 = gbk = False
        ffd = 0
        try:
            s = b.decode('utf-8')
            u8 = True
            ffd = s.count('\ufffd')
        except Exception:
            pass
        try:
            b.decode('gbk')
            gbk = True
        except Exception:
            pass
        if u8 and ffd == 0:
            rows.append(('UTF8', rel, ''))
        elif u8 and ffd > 0:
            rows.append(('腐蚀', rel, 'U+FFFD x%d' % ffd))
        elif gbk:
            rows.append(('GBK', rel, ''))
        else:
            rows.append(('异常', rel, '两种编码都解不开'))

order = {'腐蚀': 0, '异常': 1, 'GBK': 2, 'UTF8': 3, 'ASCII': 4}
rows.sort(key=lambda r: (order[r[0]], r[1]))

print('%-6s %s' % ('类别', '文件'))
print('-' * 70)
for k, rel, note in rows:
    if k in ('腐蚀', '异常'):
        print('%-6s %s   %s' % (k, rel, note))

from collections import Counter
c = Counter(r[0] for r in rows)
print('-' * 70)
print('总计 %d 个文件：' % len(rows),
      ' '.join('%s=%d' % (k, c[k]) for k in ('UTF8', 'GBK', 'ASCII', '腐蚀', '异常')))

print('\n--- GBK 文件按模块统计 ---')
mod = Counter(r[1].split(os.sep)[0] for r in rows if r[0] == 'GBK')
for k, v in sorted(mod.items()):
    print('  %-16s %d' % (k, v))
