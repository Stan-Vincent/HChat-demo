# -*- coding: utf-8 -*-
"""把 ChatServer.cpp 里写死的日志实例名参数化，使两台服务器可共用一份源码。

原：hcat::log::init("chat1");            <- ChatServer2 里是 "chat2"
     LOG_INFO("聊天服务器1正常退出");     <- ChatServer2 里是 2

改：从 config.ini 的 [SelfServer]Name 读（chatserver1 / chatserver2），
    两台共用同一份 ChatServer.cpp，靠配置区分。
铁律：GBK 源文件，按字节补丁；自动适配 CRLF/LF；幂等 + 行级自检 + 括号自检。
"""
import sys

PATH = 'ChatServer/ChatServer.cpp'

raw = open(PATH, 'rb').read()
try:
    text = raw.decode('gbk')
except Exception as e:
    print('FAIL: 不是 GBK:', e); sys.exit(1)
print('GBK 解码 OK, U+FFFD =', text.count('\ufffd'))

if 'hcat::log::init(server_name)' in text:
    print('SKIP: 已参数化'); sys.exit(0)

lines = text.splitlines(keepends=True)

def eol_of(line):
    return '\r\n' if line.endswith('\r\n') else '\n'

out = []
done_init = False
done_log = False
init_line = None
for line in lines:
    body = line.rstrip('\r\n')

    # 1) 抽掉写死的 init 行
    if 'hcat::log::init("chat1")' in body:
        init_line = line
        continue

    out.append(line)

    # 2) 在 server_name 读出来之后插入参数化的 init
    if (not done_init) and 'auto server_name' in body and 'SelfServer' in body:
        eol = eol_of(line)
        indent = body[:len(body) - len(body.lstrip())]
        out.append(indent + '// 日志实例名跟随 config.ini 的 [SelfServer]Name（chatserver1 / chatserver2），' + eol)
        out.append(indent + '// 两台 ChatServer 共用同一份源码，靠配置区分，不再各存一份只差一个字符的副本' + eol)
        out.append(indent + 'hcat::log::init(server_name);' + eol)
        done_init = True

# 3) 启动/退出日志里的硬编码编号改成参数
OLD_LOG = '("\u804a\u5929\u670d\u52a1\u56681\u6b63\u5e38\u9000\u51fa")'          # ("聊天服务器1正常退出")
NEW_LOG = '("\u804a\u5929\u670d\u52a1\u5668{}\u6b63\u5e38\u9000\u51fa", server_name)'  # ("聊天服务器{}正常退出", server_name)
for i, line in enumerate(out):
    if OLD_LOG in line:
        out[i] = line.replace(OLD_LOG, NEW_LOG)
        done_log = True

if not done_init:
    print('FAIL: 没找到 server_name 行，无法插入 init'); sys.exit(1)
if not done_log:
    print('WARN: 没找到硬编码的退出日志（可能已经改过）')

new = ''.join(out)

# 行级吞行自检：原文件每个非空行（去掉被删的那行）必须还在新文件里
old_bodies = [l.rstrip('\r\n') for l in lines if l.rstrip('\r\n').strip()]
new_text_stripped = new
missing = [b for b in old_bodies
           if b.strip() and 'hcat::log::init("chat1")' not in b and b not in new_text_stripped]
if missing:
    print('FAIL: 行丢失:', missing[:3]); sys.exit(1)

# 括号深度自检（只看 (), {}，忽略字符串内的粗略误差可接受；这里要求净变化为 0）
def depth(s, a, b):
    d = 0
    for ch in s:
        if ch == a: d += 1
        elif ch == b: d -= 1
    return d
for a, b in (('(', ')'), ('{', '}')):
    if depth(text, a, b) != depth(new, a, b):
        print('FAIL: %s%s 深度变化' % (a, b)); sys.exit(1)

try:
    data = new.encode('gbk')
except Exception as e:
    print('FAIL: GBK 编码失败:', e); sys.exit(1)
open(PATH, 'wb').write(data)
print('OK: 已参数化（init 用 server_name，退出日志用 {} 占位）')
