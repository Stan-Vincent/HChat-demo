# -*- coding: utf-8 -*-
"""修复 filters 里的双反斜杠：`..\\ChatServer\\\\xxx` -> `..\\ChatServer\\xxx`，
并把 config.ini 还原为裸文件名（每个工程显示自己的 config.ini，而不是 #1 的）。

背景：filters 的 <Filter> 归组靠【Include 字符串与工程评估出的条目值完全一致】匹配。
之前脚本用 raw string r'..\\ChatServer\\\\' 加前缀，lambda 替换不做转义，
结果写入的是双反斜杠，与 props 评估出的单反斜杠路径永不相等 -> 全部文件不进组。
"""
FILES = ['ChatServer/ChatServer.vcxproj.filters',
         'ChatServer2/ChatServer.vcxproj.filters']

DBL = '..\\ChatServer\\\\'   # ..\ChatServer\\   （错误：双反斜杠）
SGL = '..\\ChatServer\\'     # ..\ChatServer\    （正确）

for f in FILES:
    t = open(f, encoding='utf-8-sig').read()
    n = t.count(DBL)
    t = t.replace(DBL, SGL)
    # config.ini 各工程各自持有，必须裸文件名
    n2 = t.count('Include="' + SGL + 'config.ini"')
    t = t.replace('Include="' + SGL + 'config.ini"', 'Include="config.ini"')
    open(f, 'w', encoding='utf-8-sig', newline='').write(t)
    print('%s: 修双反斜杠 %d 处, config.ini 去前缀 %d 处' % (f, n, n2))

# 自检
for f in FILES:
    t = open(f, encoding='utf-8-sig').read()
    assert DBL not in t, f + ' 仍有双反斜杠'
    assert 'Include="' + SGL + 'config.ini"' not in t, f + ' config.ini 仍带前缀'
    # 40 个文件条目 + 9 个 <Filter Include="分组"> 定义 = 49
    assert t.count('Include=') == 49, f + ' 条目数异常: %d' % t.count('Include=')
print('自检通过（每份 filters 40 个条目 = 39 共享 + config.ini）')
