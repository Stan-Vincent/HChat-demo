# -*- coding: utf-8 -*-
"""Accurate check: slots == args for every LOG_* call (incl. multi-line)."""
import re, os

ROOT = r"D:\QTcode\HChat"
SERVICES = ["GateServer", "ChatServer", "ChatServer2", "StatusServer"]
START = re.compile(r'LOG_(TRACE|DEBUG|INFO|WARN|ERROR)\s*\(')


def read(p):
    raw = open(p, "rb").read()
    for enc in ("gbk", "utf-8-sig", "utf-8"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return None


def analyse(inner):
    """return (slots, args, fmt_ok)"""
    args, buf, depth, inq = [], "", 0, False
    i = 0
    while i < len(inner):
        c = inner[i]
        if inq:
            buf += c
            if c == '\\' and i + 1 < len(inner):
                buf += inner[i + 1]
                i += 1
            elif c == '"':
                inq = False
        else:
            if c == '"':
                inq = True
                buf += c
            elif c in "([{":
                depth += 1
                buf += c
            elif c in ")]}":
                depth -= 1
                buf += c
            elif c == ',' and depth == 0:
                args.append(buf.strip())
                buf = ""
            else:
                buf += c
        i += 1
    if buf.strip():
        args.append(buf.strip())

    if not args:
        return 0, 0, False
    fmt = args[0]
    if not (fmt.startswith('"') and fmt.endswith('"')):
        return 0, len(args), False
    return fmt[1:-1].count("{}"), len(args) - 1, True


problems = []
total_calls = 0
for svc in SERVICES:
    d = os.path.join(ROOT, svc)
    for fn in sorted(os.listdir(d)):
        if not fn.endswith('.cpp'):
            continue
        p = os.path.join(d, fn)
        t = read(p)
        if t is None or 'LOG_' not in t:
            continue
        nl = '\r\n' if '\r\n' in t else '\n'
        lines = t.split(nl)
        i = 0
        while i < len(lines):
            m = START.search(lines[i])
            if not m:
                i += 1
                continue
            # collect from the opening paren to the matching close
            depth = 0
            blk = ""
            j = i
            started = False
            while j < len(lines):
                ln = lines[j]
                if j > i:
                    blk += nl + ln
                else:
                    blk = ln[lines[i].index('LOG_'):]
                for ch in blk:
                    if ch == '(':
                        depth += 1
                        started = True
                    elif ch == ')':
                        depth -= 1
                if started and depth <= 0:
                    break
                j += 1
            total_calls += 1
            body = blk[blk.index('(') + 1: blk.rindex(')')]
            slots, nargs, ok = analyse(body)
            if not ok:
                problems.append((svc, fn, i + 1, 'NON_LITERAL_FMT',
                                 blk.strip().replace(nl, ' ')[:100]))
            elif slots != nargs:
                problems.append((svc, fn, i + 1, 'MISMATCH %d!=%d' % (slots, nargs),
                                 blk.strip().replace(nl, ' ')[:100]))
            i = j + 1

print("total LOG_* calls : %d" % total_calls)
print("problems          : %d" % len(problems))
for p in problems[:25]:
    print("  %-12s %-20s L%-5d %-16s %s" % (p[0], p[1], p[2], p[3], p[4]))