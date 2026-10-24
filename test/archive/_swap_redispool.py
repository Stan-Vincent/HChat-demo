# -*- coding: utf-8 -*-
"""把 ChatServer / GateServer 的本地 RedisConPool 换成共享的 common/RedisConPool.h。

ChatServer/RedisMgr.h：删掉整个本地 RedisConPool 类（306 行里的一大块），
                      改为 #include "RedisConPool.h"（common 在 include 路径上）。
GateServer/RedisMgr.h：同样删掉本地声明。
GateServer/RedisMgr.cpp：删掉 RedisConPool 的实现（构造/析构/getConnection/
                      returnConnection/Close），那些都在共享头里内联实现。

铁律：GBK+CRLF 字节补丁；幂等；括号深度自检。
"""
import re
import sys

CRLF = '\r\n'


def detect(path):
    b = open(path, 'rb').read()
    for enc in ('utf-8', 'gbk'):
        try:
            s = b.decode(enc)
            if '\ufffd' not in s:
                return s, enc, (CRLF in s)
        except Exception:
            pass
    print('FAIL 编码识别失败:', path)
    sys.exit(1)


def save(path, text, enc):
    open(path, 'wb').write(text.encode(enc))
    print('OK', path, '(%s)' % enc)


def brace_balance_ok(removed, tag):
    """要求被删片段：① 以 class RedisConPool { 开头、以 }; 结尾（范围没切歪）；
    ② 大括号配平（结构关键）。
    不检查小括号配平——类内部中文注释里可能有单个 ASCII 括号，原文整体就不配平。"""
    if not removed.startswith('class RedisConPool {'):
        print('FAIL %s: 删除片段开头不是 class RedisConPool {' % tag)
        sys.exit(1)
    if not removed.rstrip().endswith('};'):
        print('FAIL %s: 删除片段结尾不是 };' % tag)
        sys.exit(1)
    if removed.count('{') != removed.count('}'):
        print('FAIL %s: 被删片段 {} 不配平 (%d vs %d)'
              % (tag, removed.count('{'), removed.count('}')))
        sys.exit(1)


# ---------------- ChatServer/RedisMgr.h ----------------
p = 'ChatServer/RedisMgr.h'
text, enc, _ = detect(p)
if 'common/RedisConPool.h' in text or text.count('class RedisConPool') == 0:
    print('SKIP（已改或没有本地类）', p)
else:
    # 删除从 "class RedisConPool {" 到与之配对的 "};"
    start = text.index('class RedisConPool {')
    i = text.index('{', start)
    depth = 0
    end = None
    for j in range(i, len(text)):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                # 类结束：} 后面可能还有分号
                k = j + 1
                if k < len(text) and text[k] == ';':
                    k += 1
                end = k
                break
    assert end is not None, '没找到类结束'
    removed = text[start:end]
    # 连同类前面的空行一起清掉
    pre = text[:start].rstrip()
    post = text[end:].lstrip('\r\n')
    new = pre + CRLF + CRLF + '#include "RedisConPool.h"   // 共享连接池（common/，替代原本地实现）' + CRLF + post
    brace_balance_ok(removed, p)
    save(p, new, enc)
    print('  删除本地 RedisConPool %d 字符' % len(removed))

# ---------------- GateServer/RedisMgr.h ----------------
p = 'GateServer/RedisMgr.h'
text, enc, _ = detect(p)
if 'RedisConPool.h"' in text and 'class RedisConPool' not in text:
    print('SKIP（已改）', p)
else:
    start = text.index('class RedisConPool {')
    i = text.index('{', start)
    depth = 0
    end = None
    for j in range(i, len(text)):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                k = j + 1
                if k < len(text) and text[k] == ';':
                    k += 1
                end = k
                break
    assert end is not None
    pre = text[:start].rstrip()
    post = text[end:].lstrip('\r\n')
    new = pre + CRLF + CRLF + '#include "RedisConPool.h"   // 共享连接池（common/，替代原本地实现）' + CRLF + post
    brace_balance_ok(removed, p)
    save(p, new, enc)

# ---------------- GateServer/RedisMgr.cpp ----------------
p = 'GateServer/RedisMgr.cpp'
text, enc, _ = detect(p)
if 'RedisConPool::' not in text:
    print('SKIP（cpp 里已无 RedisConPool 实现）', p)
else:
    # 删掉所有 RedisConPool:: 成员函数的定义（从行首注释/返回类型到函数体结束）
    out = []
    lines = text.split(CRLF)
    i = 0
    removed_blocks = []
    while i < len(lines):
        line = lines[i]
        # 函数定义的起始行特征：含 "RedisConPool::"
        if 'RedisConPool::' in line:
            # 找到这个函数的结束大括号
            j = i
            depth = 0
            started = False
            while j < len(lines):
                depth += lines[j].count('{') - lines[j].count('}')
                if '{' in lines[j]:
                    started = True
                if started and depth <= 0:
                    break
                j += 1
            block = CRLF.join(lines[i:j + 1])
            if block.count('{') != block.count('}'):
                print('FAIL %s: 删除的函数块 {} 不配平: %s'
                      % (p, line[:60]))
                sys.exit(1)
            removed_blocks.append(block)
            i = j + 1
            continue
        out.append(line)
        i += 1
    new = CRLF.join(out)
    assert 'RedisConPool::' not in new, '还有残留的 RedisConPool:: 定义'
    save(p, new, enc)
    print('  删除 RedisConPool 成员函数定义 %d 个' % len(removed_blocks))

print('完成')
