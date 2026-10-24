# -*- coding: utf-8 -*-
"""修复编码体检发现的 4 处 U+FFFD：

1) Client/ui/chatpage.cpp:924 —— 真·源码腐蚀：注释里“根本发不出��”
   按上下文（图片只在本地显示，上传代码执行不到）还原为“根本发不出去”。
2) .workbuddy/memory/*.md 与 docs/工程备忘.md —— 这几处 U+FFFD 是**写文档时引用的示例字符**
   （描述“腐蚀成 X”时直接把那个字符写进去了），不是真腐蚀。改成文字 "U+FFFD"，
   免得体检脚本每次都报。
"""
import io

FIX_SRC = {
    'Client/ui/chatpage.cpp': [('\u53d1\u4e0d\u51fa\ufffd\ufffd\u3002',
                                '\u53d1\u4e0d\u51fa\u53bb\u3002')],  # 发不出XX。 -> 发不出去。
}

DOCS = ['.workbuddy/memory/MEMORY.md',
        '.workbuddy/memory/2026-10-18.md',
        'docs/\u5de5\u7a0b\u5907\u5fd8.md']  # docs/工程备忘.md

for p, pairs in FIX_SRC.items():
    s = open(p, encoding='utf-8').read()
    n = 0
    for a, b in pairs:
        if a in s:
            s = s.replace(a, b); n += 1
    assert n, p + ' 未命中'
    open(p, 'w', encoding='utf-8', newline='').write(s)
    print('源码还原: %s  (%d 处)' % (p, n))

for p in DOCS:
    try:
        s = open(p, encoding='utf-8').read()
    except FileNotFoundError:
        print('跳过(不存在)', p); continue
    if '\ufffd' not in s:
        continue
    n = s.count('\ufffd')
    s = s.replace('\ufffd', 'U+FFFD')
    open(p, 'w', encoding='utf-8', newline='').write(s)
    print('文档去示例字符: %s  (%d 处)' % (p, n))
