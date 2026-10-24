#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""配置一致性校验：把"必须同值"的配置从注释约束升级为测试约束。

检查项：
 1. [Upload] MaxBytes：Client 与 GateServer 必须相同
    （否则客户端放过一个服务端要拒的文件，或者反过来用户被无谓拒绝）
 2. ErrorCodes 1006~1012：4 份定义（ChatServer/GateServer/StatusServer/Client）数值必须一致
    （防止以后某一边改了码值，客户端按错码提示）
 3. 端口唯一性：各服务的监听端口（含 RPC）不能撞车
 4. MySQL / Redis 连接参数在各服务间一致

退出码 0 = 全部通过。
"""
import configparser
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 端口归属表：文件 -> [(段, 键, 说明)]
PORT_SPEC = [
    ('GateServer/config.ini',  'GateServer', 'Port',    'GateServer HTTP'),
    ('ChatServer/config.ini',  'SelfServer', 'Port',     'ChatServer#1 TCP'),
    ('ChatServer/config.ini',  'SelfServer', 'RPCPort',  'ChatServer#1 RPC'),
    ('ChatServer2/config.ini', 'SelfServer', 'Port',     'ChatServer#2 TCP'),
    ('ChatServer2/config.ini', 'SelfServer', 'RPCPort',  'ChatServer#2 RPC'),
    ('StatusServer/config.ini', 'StatusServer', 'Port',  'StatusServer RPC'),
]

ERR_SOURCES = [
    ('ChatServer/const.h',   'ChatServer'),
    ('GateServer/const.h',   'GateServer'),
    ('StatusServer/const.h', 'StatusServer'),
    ('Client/common/global.h', 'Client'),
]
# 这些码客户端会直接用来提示用户，数值必须跨模块一致。
# 注意 1012(DbErr) 只有 GateServer/Client 有；1017~1021 是 ChatServer 专用，
# 所以按"该模块自己定义了才校验"处理，见下面的逐模块比对逻辑。
CHECK_CODES = ['PasswdErr', 'EmailNotMatch', 'PasswdUpFailed',
               'PasswdInvalid', 'TokenInvalid', 'UidInvalid', 'DbErr',
               'CREATE_CHAT_FAILED', 'LOAD_CHAT_FAILED', 'Error_MySql',
               'CREATE_GROUP_FAILED', 'ADD_GROUP_MEMBER_FAILED']
# 每个码应该在哪些模块里存在（不在名单里的模块不强制要求）
CODE_OWNERS = {
    'DbErr': ['GateServer', 'Client'],
    'CREATE_CHAT_FAILED': ['ChatServer', 'Client'],
    'LOAD_CHAT_FAILED': ['ChatServer', 'Client'],
    'Error_MySql': ['ChatServer', 'Client'],
    'CREATE_GROUP_FAILED': ['ChatServer', 'Client'],
    'ADD_GROUP_MEMBER_FAILED': ['ChatServer', 'Client'],
}

fail = []


def read_ini(rel):
    p = os.path.join(ROOT, rel)
    if not os.path.isfile(p):
        fail.append('缺少配置文件: %s' % rel)
        return None
    cp = configparser.ConfigParser()
    cp.read(p, encoding='utf-8')
    return cp


print('=' * 60)
print('配置一致性校验')
print('=' * 60)

# ---- 1. 上传上限 ----
cli = read_ini('Client/config.ini')
gate = read_ini('GateServer/config.ini')
if cli and gate:
    c = cli.get('Upload', 'MaxBytes', fallback=None)
    g = gate.get('Upload', 'MaxBytes', fallback=None)
    if c is None or g is None:
        fail.append('[Upload] MaxBytes 缺失: Client=%s GateServer=%s' % (c, g))
    elif int(c) != int(g):
        fail.append('[Upload] MaxBytes 不一致: Client=%s GateServer=%s' % (c, g))
    else:
        print('  [OK] 上传上限一致: %s bytes (%.0f MB)'
              % (c, int(c) / 1024 / 1024))

# ---- 2. 错误码数值 ----
def parse_codes(rel):
    """从 C++ 头里抠出 Name = 1234 形式的枚举值。"""
    text = open(os.path.join(ROOT, rel), encoding='utf-8',
                errors='replace').read()
    m = re.search(r'enum\s+(?:class\s+)?ErrorCodes\s*\{(.*?)\}', text, re.S)
    if not m:
        return None
    out = {}
    for name, val in re.findall(r'(\w+)\s*=\s*(\d+)', m.group(1)):
        out[name] = int(val)
    return out

maps = {}
for rel, tag in ERR_SOURCES:
    codes = parse_codes(rel)
    if codes is None:
        fail.append('未找到 ErrorCodes: %s' % rel)
        continue
    maps[tag] = codes

if len(maps) == len(ERR_SOURCES):
    for name in CHECK_CODES:
        owners = CODE_OWNERS.get(name)   # None = 四个模块都该有
        want = owners if owners else [t for _, t in ERR_SOURCES]
        vals = {}
        for tag in want:
            if tag not in maps or name not in maps[tag]:
                fail.append('%s 缺少错误码 %s' % (tag, name))
            else:
                vals[tag] = maps[tag][name]
        if len(set(vals.values())) > 1:
            fail.append('错误码 %s 数值不一致: %s' % (name, vals))
    if not any('错误码' in f for f in fail):
        print('  [OK] %d 个错误码在各模块数值一致' % len(CHECK_CODES))

# ---- 3. 端口唯一性 ----
seen = {}
for rel, sec, key, desc in PORT_SPEC:
    cp = read_ini(rel)
    if not cp:
        continue
    v = cp.get(sec, key, fallback=None)
    if v is None:
        fail.append('%s 缺少 [%s] %s' % (rel, sec, key))
        continue
    v = int(v)
    if v in seen:
        fail.append('端口冲突 %d: %s 与 %s' % (v, seen[v], desc))
    else:
        seen[v] = desc
if not any('端口' in f for f in fail):
    print('  [OK] %d 个监听端口无冲突' % len(seen))

# ---- 4. MySQL / Redis 参数一致 ----
def get_pair(rel, sec, keys):
    cp = read_ini(rel)
    if not cp:
        return None
    return tuple(cp.get(sec, k, fallback='') for k in keys)

svc_inis = ['ChatServer/config.ini', 'ChatServer2/config.ini',
            'GateServer/config.ini', 'StatusServer/config.ini']
for sec, keys in (('Mysql', ['Host', 'Port', 'Schema']),
                  ('Redis', ['Host', 'Port'])):
    vals = set()
    for rel in svc_inis:
        v = get_pair(rel, sec, keys)
        if v:
            vals.add(v)
    if len(vals) > 1:
        fail.append('[%s] 各服务参数不一致: %s' % (sec, vals))
    elif vals:
        print('  [OK] [%s] 各服务一致: %s' % (sec, '/'.join(vals.pop())))

print('-' * 60)
if fail:
    print('失败 %d 项:' % len(fail))
    for f in fail:
        print('  -', f)
    sys.exit(1)
print('全部通过')
