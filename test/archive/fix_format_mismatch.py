# -*- coding: utf-8 -*-
"""
Repair spdlog format-string/argument mismatches.

spdlog THROWS at runtime when placeholder count != argument count, which would
crash the service on a code path we did not exercise at build time.
So: reconcile every LOG_* call to slots == args.
"""
import re, os

ROOT = r"D:\QTcode\HChat"
SERVICES = ["GateServer", "ChatServer", "ChatServer2", "StatusServer"]
LOG_RE = re.compile(r'^(\s*)LOG_(TRACE|DEBUG|INFO|WARN|ERROR)\((.*)\);\s*$')


def read(p):
    raw = open(p, "rb").read()
    for enc in ("gbk", "utf-8-sig", "utf-8"):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return None, None


def write(p, txt, enc, bom=False):
    data = txt.encode(enc)
    if bom:
        data = b"\xef\xbb\xbf" + data
    open(p, "wb").write(data)


def split_top(inner):
    """split macro args on top-level commas, respecting quotes/brackets"""
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
    return args


def fix_line(line):
    m = LOG_RE.match(line)
    if not m:
        return None
    indent, lvl, inner = m.group(1), m.group(2), m.group(3)
    args = split_top(inner)
    if not args:
        return None

    fmt = args[0]
    rest = args[1:]

    # Non-literal first arg (e.g. a variable holding text) -> wrap it.
    if not (fmt.startswith('"') and fmt.endswith('"')):
        return '%sLOG_%s("%s", %s);' % (indent, lvl, fmt, ', '.join(rest)) if rest \
               else '%sLOG_%s("{}", %s);' % (indent, lvl, fmt)

    body = fmt[1:-1]
    slots = body.count("{}")

    # case A: literal already contains {} and args repeat it -> collapse dupes
    dedup = []
    for a in rest:
        if dedup and dedup[-1] == a:
            continue
        dedup.append(a)
    rest = dedup

    if slots < len(rest):
        body = body + " {}" * (len(rest) - slots)
    elif slots > len(rest):
        # drop excess placeholders from the end (keep code valid & non-throwing)
        while body.count("{}") > len(rest):
            i = body.rfind("{}")
            body = body[:i] + body[i + 2:]
        body = body.rstrip()

    return '%sLOG_%s("%s"%s);' % (indent, lvl, body,
                                  (', ' + ', '.join(rest)) if rest else "")


total = 0
for svc in SERVICES:
    d = os.path.join(ROOT, svc)
    for fn in sorted(os.listdir(d)):
        if not fn.endswith('.cpp'):
            continue
        p = os.path.join(d, fn)
        txt, enc = read(p)
        if txt is None or 'LOG_' not in txt:
            continue
        bom = open(p, 'rb').read(3) == b'\xef\xbb\xbf'
        nl = '\r\n' if '\r\n' in txt else '\n'
        lines = txt.split(nl)
        changed = 0
        for i, ln in enumerate(lines):
            new = fix_line(ln)
            if new is not None and new != ln:
                lines[i] = new
                changed += 1
        if changed:
            write(p, nl.join(lines), enc, bom)
            print("  %-14s %-22s fixed=%d" % (svc, fn, changed))
            total += changed
print("TOTAL fixed =", total)