# -*- coding: utf-8 -*-
"""StatusServer entry: init logger + convert its logs (UTF-8 w/ BOM file)."""
import re

p = r"D:\QTcode\HChat\StatusServer\StatusServer.cpp"
txt = open(p, "rb").read().decode("utf-8-sig")
nl = "\r\n" if "\r\n" in txt else "\n"

if '#include "logger.h"' not in txt:
    lines = txt.split(nl)
    last = max(i for i, ln in enumerate(lines[:80]) if ln.startswith('#include'))
    lines.insert(last + 1, '#include "logger.h"')
    txt = nl.join(lines)
    print("  [inc] logger.h")

if 'hchat::log::init' not in txt:
    txt = re.sub(r'(int\s+main\s*\([^)]*\)\s*\{\s*\r?\n)',
                 r'\1\thchat::log::init("status");' + nl + nl,
                 txt, count=1)
    print("  [init] status")

# convert remaining cout/cerr
txt = re.sub(r'std::cerr << "Error: " << e\.what\(\) << std::endl;',
             'LOG_ERROR("StatusServer fatal error: {}", e.what());', txt)
txt = re.sub(r'std::cout << "(\w[^"]*)"\s*<<\s*std::endl;',
             lambda m: 'LOG_INFO("%s");' % m.group(1), txt)

txt = txt.replace('return EXIT_FAILURE;',
                  'hchat::log::shutdown();\r\n\t\treturn EXIT_FAILURE;', 1)
txt = re.sub(r'(\r?\n)(\t*return EXIT_SUCCESS;)',
             r'\1\2LOG_INFO("StatusServer exited gracefully");'
             r'\1\2hchat::log::shutdown();'
             r'\1\2return EXIT_SUCCESS;', txt, count=1)

open(p, "wb").write(b'\xef\xbb\xbf' + txt.encode("utf-8"))
print("  [ok] saved StatusServer.cpp")
print(txt[txt.find("int main"):txt.find("int main") + 620])