# -*- coding: utf-8 -*-
"""错误码段冲突修复 + 冗余配置清理。

**Bug**：ChatServer/const.h 把 1012 用作 CREATE_CHAT_FAILED，而 GateServer/const.h 的
1012 是 DbErr —— 同一码值两个含义。客户端（Client/common/global.h）只定义了 DbErr=1012，
于是 ChatServer 说"创建会话失败"时，客户端按 1012 提示成"数据库操作失败"，张冠李戴；
ChatServer 的 1015/1016（建群/加成员失败）客户端则完全没定义，只能显示"未知错误"。

**修法**：把 ChatServer 的 5 个专用码整体搬到 1017~1021，与 GateServer 段（1001~1012）
不再重叠；客户端同步补 1017~1021 的定义。枚举名不变，所以 ChatServer 的调用代码零改动。
已确认全仓无硬编码这些数值（客户端那个 1015 是 ReqId，另一个命名空间）。

顺带删掉 ChatServer/config.ini 里没人读的 [GateServer] 段（从 GateServer/config.ini
复制过来的残留，ChatServer 代码没有任何地方读它）。
"""
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


# ---------- 1) ChatServer/const.h：1012~1016 -> 1017~1021 ----------
p = 'ChatServer/const.h'
text, enc, _ = detect(p)
pairs = [
    ('CREATE_CHAT_FAILED = 1012',       'CREATE_CHAT_FAILED = 1017'),
    ('LOAD_CHAT_FAILED = 1013',         'LOAD_CHAT_FAILED = 1018'),
    ('Error_MySql = 1014',              'Error_MySql = 1019'),
    ('CREATE_GROUP_FAILED = 1015',      'CREATE_GROUP_FAILED = 1020'),
    ('ADD_GROUP_MEMBER_FAILED = 1016',  'ADD_GROUP_MEMBER_FAILED = 1021'),
]
if 'CREATE_CHAT_FAILED = 1017' in text:
    print('SKIP 已改过:', p)
else:
    for old, new in pairs:
        assert text.count(old) == 1, (p, old, text.count(old))
        text = text.replace(old, new, 1)
    # 说明为什么跳段：防止以后有人"顺手"把新码填回 1012
    note = ('\t// NOTE: 1012~1016 are reserved by GateServer (DbErr = 1012 etc.);\n'
            '\t// ChatServer-specific codes start at 1017 so the same numeric value\n'
            '\t// never means two different things across services.\n')
    if 'reserved by GateServer' not in text:
        text = text.replace('\tSuccess = 0,', '\tSuccess = 0,' + CRLF + note, 1)
    save(p, text, enc)

# ---------- 2) ChatServer/config.ini：删没人读的 [GateServer] 段 ----------
p = 'ChatServer/config.ini'
text, enc, _ = detect(p)
if '[GateServer]' in text:
    new = text.replace('[GateServer]' + CRLF + 'Port = 8080' + CRLF, '')
    if new == text:  # 换行风格可能是 LF
        new = text.replace('[GateServer]\nPort = 8080\n', '')
    assert new != text, '没删掉 [GateServer] 段'
    assert '[VarifyServer]' in new
    save(p, new, enc)
else:
    print('SKIP 已无 [GateServer] 段:', p)

print('补丁完成')
