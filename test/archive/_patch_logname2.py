# -*- coding: utf-8 -*-
"""ChatServer.cpp：退出日志里的服务器编号参数化（两版原本一个是"聊天服务"、一个是"聊天服务2"）。"""
import sys

PATH = 'ChatServer/ChatServer.cpp'
text = open(PATH, 'rb').read().decode('gbk')

NEW_LOG = 'LOG_INFO("\u804a\u5929\u670d\u52a1{} \u6b63\u5e38\u9000\u51fa", server_name);'
OLD_LOGS = ['LOG_INFO("\u804a\u5929\u670d\u52a1\u6b63\u5e38\u9000\u51fa");',       # 聊天服务正常退出
            'LOG_INFO("\u804a\u5929\u670d\u52a12\u6b63\u5e38\u9000\u51fa");']      # 聊天服务2正常退出

if NEW_LOG in text:
    print('SKIP: 已参数化'); sys.exit(0)

hit = None
for o in OLD_LOGS:
    if text.count(o) == 1:
        hit = o
        break
if hit is None:
    print('FAIL: 未找到唯一匹配的退出日志行'); sys.exit(1)

new = text.replace(hit, NEW_LOG)
assert new.count('LOG_INFO') == text.count('LOG_INFO')
open(PATH, 'wb').write(new.encode('gbk'))
print('OK:', hit, '->', NEW_LOG)
