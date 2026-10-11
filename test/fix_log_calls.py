# -*- coding: utf-8 -*-
"""
Validate & repair LOG_* calls produced by the batch converter.

Problems found by inspection:
  P1  placeholders count != argument count  -> spdlog throws at RUNTIME
  P2  variable not in scope (uid_str in MysqlDao password check)
  P3  redundant duplicated arg  ("Result:  {}", result, result)
"""
import re, os

ROOT = r"D:\QTcode\HChat"
SERVICES = ["GateServer", "ChatServer", "ChatServer2", "StatusServer"]

BAD_VARS = {"uid_str"}   # not in scope at those sites


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


def fix_file(path):
    txt, enc = read(path)
    if txt is None or "LOG_" not in txt:
        return 0
    orig = txt
    nl = "\r\n" if "\r\n" in txt else "\n"
    lines = txt.split(nl)
    out = []
    n = 0
    for ln in lines:
        m = re.match(r'^(\s*)LOG_(INFO|WARN|ERROR|DEBUG)\((.*)\);\s*$', ln)
        if not m:
            out.append(ln)
            continue
        indent, lvl, inner = m.group(1), m.group(2), m.group(3)

        # split top-level args (respect quotes)
        args, buf, depth, inq = [], "", 0, False
        i = 0
        while i < len(inner):
            c = inner[i]
            if inq:
                buf += c
                if c == '\\':
                    if i + 1 < len(inner):
                        buf += inner[i + 1]
                        i += 1
                elif c == '"':
                    inq = False
            else:
                if c == '"':
                    inq = True
                    buf += c
                elif c in "([{<":
                    depth += 1
                    buf += c
                elif c in ")]}>":
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
            out.append(ln)
            continue

        fmtstr = args[0]
        rest = args[1:]
        changed = False

        # P3: exact duplicate args -> collapse to one per placeholder
        if rest:
            uniq = []
            for a in rest:
                if uniq and uniq[-1] == a:
                    changed = True
                    continue
                uniq.append(a)
            rest = uniq

        # P1: reconcile placeholder count
        if fmtstr.startswith('"') and fmtstr.endswith('"'):
            body = fmtstr[1:-1]
            slots = body.count("{}")
            if slots < len(rest):
                body = body + " {}" * (len(rest) - slots)
                changed = True
            elif slots > len(rest):
                # drop extra placeholders from the tail (safe-ish)
                while body.count("{}") > len(rest):
                    idx = body.rfind("{}")
                    body = body[:idx] + body[idx + 2:]
                changed = True
                # if a trailing {} was removed, clean the separator before it
            fmtstr = '"%s"' % body

        # P2: replace out-of-scope vars with a literal placeholder arg
        new_rest = []
        for a in rest:
            if a in BAD_VARS:
                new_rest.append('"<redacted>"')
                changed = True
            else:
                new_rest.append(a)
        rest = new_rest

        if changed or (fmtstr, rest) != (args[0], args[1:]):
            n += 1
        out.append('%sLOG_%s(%s);' % (indent, lvl, ', '.join([fmtstr] + rest)))
    txt2 = nl.join(out)
    if txt2 != orig:
        write(path, txt2, enc)
    return n


total = 0
for svc in SERVICES:
    d = os.path.join(ROOT, svc)
    if not os.path.isdir(d):
        continue
    sn = 0
    for fn in sorted(os.listdir(d)):
        if fn.endswith('.cpp'):
            sn += fix_file(os.path.join(d, fn))
    print("  %-14s fixed=%d" % (svc, sn))
    total += sn
print("TOTAL fixed=%d" % total)