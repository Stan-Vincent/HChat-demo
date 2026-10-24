# -*- coding: utf-8 -*-
"""审计 spdlog 日志的 fmt 占位符 {} 数量与参数数量是否一致。
spdlog 参数不匹配是运行期抛异常，编译期查不出，必须静态审计。"""
import glob
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')


def read(p):
    b = open(p, 'rb').read()
    if b[:3] == b'\xef\xbb\xbf':
        return b.decode('utf-8-sig')
    try:
        return b.decode('utf-8')
    except UnicodeDecodeError:
        return b.decode('gbk', errors='replace')


def split_args(s):
    parts = []
    cur = ''
    depth = 0
    instr = None
    i = 0
    while i < len(s):
        c = s[i]
        if instr:
            cur += c
            if c == '\\':
                cur += s[i + 1]
                i += 2
                continue
            if c == instr:
                instr = None
        else:
            if c in '"\'':
                instr = c
                cur += c
            elif c in '([{':
                depth += 1
                cur += c
            elif c in ')]}':
                depth -= 1
                cur += c
            elif c == ',' and depth == 0:
                parts.append(cur.strip())
                cur = ''
            else:
                cur += c
        i += 1
    if cur.strip():
        parts.append(cur.strip())
    return parts


bad = 0
total = 0
pat = re.compile(r'LOG_\w+\(("[^"]*")\s*(?:,\s*(.*))?\)\s*;')
for d in ['GateServer', 'ChatServer', 'ChatServer2', 'StatusServer']:
    for p in sorted(glob.glob(d + '/*.cpp')):
        txt = read(p)
        for ln, line in enumerate(txt.split('\n'), 1):
            s = line.strip()
            if not s.startswith('LOG_'):
                continue
            m = pat.match(s)
            if not m:
                continue
            fmt = m.group(1)
            args = m.group(2) or ''
            nbrace = fmt.count('{}')
            nargs = 0 if not args else len(split_args(args))
            total += 1
            if nbrace != nargs:
                bad += 1
                print('MISMATCH %s:%d  braces=%d args=%d :: %s' % (p, ln, nbrace, nargs, s[:70]))
print('total LOG = %d, mismatched = %d' % (total, bad))
