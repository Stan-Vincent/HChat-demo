# -*- coding: utf-8 -*-
"""
Batch convert std::cout logging -> LOG_* macros across backend services.

Strategy (value-based triage, per user decision):
  - Sensitive data (password / token)  -> redact, keep the event
  - Errors / failures                   -> LOG_ERROR / LOG_WARN
  - Full request/response dumps         -> delete (noise + perf)
  - Success paths                       -> LOG_INFO
  - Pure debug tracing                  -> LOG_DEBUG

GBK files are read/written as GBK to preserve Chinese comments.
"""
import re, os, sys

SERVICES = ["GateServer", "ChatServer", "ChatServer2", "StatusServer"]
ROOT = r"D:\QTcode\HChat"

# ---------------------------------------------------------------- patterns
# 1) Sensitive: never log the secret itself
SENSITIVE = [
    (r'std::cout << "Password: " << (\w+) << std::endl;',
     r'LOG_WARN("password mismatch check for user {} (origin pwd hidden)", uid_str);'),
    (r'std::cout << "user login uid is\s*" << (\w+) << " user token\s+is\s*"\s*\n?\s*<< (\w+) << endl;',
     r'LOG_INFO("chat login request, uid={}, token=[redacted]", uid);'),
]

# 2) Full body dumps: pure noise + perf killer + leaks pwd in POST body
BODY_DUMP = [
    r'std::cout << "receive body is " << (\w+) << std::endl;',
    r'std::cout << "recv body is " << (\w+) << std::endl;',
]

# 3) Generic fallbacks applied to whatever is left
ERROR_HINTS = [
    r'fail', r'error', r'err\b', r'errno', r'not found', r'invalid',
    r'timeout', r'time out', r'expire', r'refuse', r'close', r'disconnect',
    r'exception', r'crash', r'wrong', r'miss', r'empty\b', r'reject',
]
WARN_HINTS = [
    r'deprecated', r'skip', r'ignore', r'retry', r'slow', r'full',
    r'expire', r'already',
]


def classify(text):
    low = text.lower()
    for h in ERROR_HINTS:
        if re.search(h, low):
            return 'error'
    for h in WARN_HINTS:
        if re.search(h, low):
            return 'warn'
    return 'info'


def convert_cout_line(line, fname):
    """Convert a single `std::cout << ... << std::endl;` line (possibly
    spanning multiple physical lines) into a LOG_XXX call."""
    # Normalize: capture everything between std::cout and the trailing ;
    m = re.match(r'^(\s*)std::cout << (.*?);\s*$', line, re.S)
    if not m:
        return None
    indent, expr = m.group(1), m.group(2)

    # Split the << chain into operands
    parts = re.split(r'\s*<<\s*', expr)
    parts = [p.strip() for p in parts if p.strip()]
    if not parts:
        return None

    # Detect endl / "\n" terminators to drop
    cleaned = []
    for p in parts:
        if p in ('std::endl', 'endl'):
            continue
        if re.fullmatch(r'"\\n"', p):
            continue
        cleaned.append(p)

    # Drop a leading literal string chunk that is pure whitespace/newlines
    if cleaned and re.fullmatch(r'"\s*"', cleaned[0]):
        cleaned = cleaned[1:]
    if not cleaned:
        return None

    first = cleaned[0]
    if not (first.startswith('"') and first.endswith('"')):
        # Not a leading literal -> wrap everything in one string
        literal = '"{}"'
        args = cleaned
    else:
        # Convert literal "{}" placeholders into spdlog "{}"
        lit = first[1:-1]
        n_placeholder = lit.count('{}')
        args = [first]
        rest = cleaned[1:]
        # spdlog consumes one arg per {}; if counts mismatch, keep it simple:
        # put remaining operands as args only when placeholders exist.
        if n_placeholder == 0 and rest:
            # no slots: append as extra arg(s) -> fmt will error.
            # safest: turn rest into additional "{}" slots.
            lit = lit + " {}" * len(rest)
            args = ['"%s"' % lit] + rest
        elif n_placeholder < len(rest):
            lit = lit + " {}" * (len(rest) - n_placeholder)
            args = ['"%s"' % lit] + rest
        args = args + rest

    lvl = classify(first)
    macro = {'info': 'LOG_INFO', 'warn': 'LOG_WARN', 'error': 'LOG_ERROR'}[lvl]
    return '%s%s(%s);' % (indent, macro, ', '.join(args))


def process(path):
    raw = open(path, "rb").read()
    try:
        txt = raw.decode('gbk')
    except UnicodeDecodeError:
        try:
            txt = raw.decode('utf-8')
        except UnicodeDecodeError:
            return 0, 0, 0
    orig = txt
    fname = os.path.basename(path)

    if 'std::cout' not in txt and 'std::cerr' not in txt:
        return 0, 0, 0

    # include
    if '#include "logger.h"' not in txt:
        lines = txt.split('\n')
        # insert after the last #include at top
        last = 0
        for i, ln in enumerate(lines[:80]):
            if ln.startswith('#include'):
                last = i
        if last:
            lines.insert(last + 1, '#include "logger.h"')
            txt = '\n'.join(lines)

    nl = '\r\n' if '\r\n' in txt else '\n'
    out_lines = txt.split(nl)
    res = []
    i = 0
    n_conv = n_del = 0
    while i < len(out_lines):
        line = out_lines[i]
        if 'std::cout' in line or 'std::cerr' in line:
            # gather continuation lines until we see a ';'
            block = line
            j = i
            while ';' not in block and j + 1 < len(out_lines):
                j += 1
                block += nl + out_lines[j]

            # sensitive first
            done = False
            for pat, rep in SENSITIVE:
                if re.search(pat, block, re.S):
                    res.append(re.sub(pat, rep, block, flags=re.S))
                    n_conv += 1
                    done = True
                    break
            if done:
                i = j + 1
                continue

            # body dumps -> delete
            if any(re.search(p, block, re.S) for p in BODY_DUMP):
                n_del += 1
                i = j + 1
                continue

            # cerr is always error-ish
            conv = convert_cout_line(block, fname)
            if conv:
                if 'std::cerr' in block:
                    conv = conv.replace('LOG_INFO', 'LOG_ERROR').replace('LOG_WARN', 'LOG_ERROR')
                res.append(conv)
                n_conv += 1
            else:
                res.append(block)
            i = j + 1
            continue
        res.append(line)
        i += 1

    txt2 = nl.join(res)
    if txt2 != orig:
        try:
            open(path, "wb").write(txt2.encode('gbk'))
        except UnicodeEncodeError:
            open(path, "wb").write(txt2.encode('utf-8'))
    return n_conv, n_del, 0


total_c = total_d = 0
for svc in SERVICES:
    d = os.path.join(ROOT, svc)
    if not os.path.isdir(d):
        continue
    sc = sd = 0
    for fn in sorted(os.listdir(d)):
        if not fn.endswith('.cpp'):
            continue
        c, dl, _ = process(os.path.join(d, fn))
        if c or dl:
            print("  %-14s %-22s converted=%-3d deleted=%d" % (svc, fn, c, dl))
        sc += c
        sd += dl
    print("  %-14s TOTAL converted=%d deleted=%d" % (svc, sc, sd))
    total_c += sc
    total_d += sd

print("TOTAL converted=%d deleted=%d" % (total_c, total_d))