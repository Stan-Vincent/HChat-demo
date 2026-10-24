# -*- coding: utf-8 -*-
"""修 start.bat：`backend` 分支不起 ChatServer2 的 bug。

原逻辑第 115-116 行：
    if /i "%~1"=="chat2"   goto :s_chat2
    if /i "%~1"=="backend" goto :done      <- backend 直接收尾，漏了 ChatServer2

而 test/run_all.sh 的提示文案写的是 "Start\\start.bat backend"（它需要 8991 在线），
两边对不上。改为 backend 也走 :s_chat2（s_chat2 结尾本身就是 goto :done）。
文件是 GBK（chcp 936），字节补丁。
"""
p = 'Start/start.bat'
raw = open(p, 'rb').read()
try:
    text = raw.decode('gbk')
except Exception as e:
    print('FAIL 非 GBK:', e); raise SystemExit(1)

OLD = 'if /i "%~1"=="backend" goto :done'
NEW = 'if /i "%~1"=="backend" goto :s_chat2'

if NEW in text:
    print('SKIP 已修')
    raise SystemExit(0)
if text.count(OLD) != 1:
    print('FAIL 期望 1 处，实际', text.count(OLD)); raise SystemExit(1)

new = text.replace(OLD, NEW)
assert new.count('goto :s_chat2') >= 2
open(p, 'wb').write(new.encode('gbk'))
print('OK: start.bat backend 现在会连带启动 ChatServer2')
