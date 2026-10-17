#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
群聊回归用例（任务 #46）。

★ 前置条件：ChatServer / ChatServer2 必须是【加了群聊代码之后重新编译】的版本。
  老版本没有 ID_CREATE_GROUP_CHAT_REQ(1033) / ID_ADD_GROUP_MEMBER_REQ(1035)，
  也不会按 type 分支投递 —— 这时所有用例都会失败（不是用例本身的问题）。

前置服务：GateServer 8080、StatusServer 50052、ChatServer 8990、ChatServer2 8991。
（两个 ChatServer 都要在，因为不同账号可能被 StatusServer 分到不同机器上，
  扇出用例正好顺便覆盖跨服 gRPC 这条路。）

用法：
    python test/group_chat_test.py
    python test/group_chat_test.py --keep     # 不删测试建出来的群（排查用）
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from protocol_test import (          # noqa: E402  复用登录/收发包那套
    Client, login, wait_rsp,
    ID_TEXT_CHAT_MSG_REQ, ID_TEXT_CHAT_MSG_RSP,
    ID_LOAD_CHAT_THREAD_REQ, ID_LOAD_CHAT_THREAD_RSP,
)
import dbconf                        # noqa: E402

# protocol_test 里没定义这个（它不测接收侧），这里补上
ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019

# ---- 群聊新增的消息 ID（与 Client/global.h、ChatServer/const.h 对齐）----
ID_CREATE_GROUP_CHAT_REQ = 1033
ID_CREATE_GROUP_CHAT_RSP = 1034
ID_ADD_GROUP_MEMBER_REQ = 1035
ID_ADD_GROUP_MEMBER_RSP = 1036

GREEN, RED, YELLOW, RESET = '\033[32m', '\033[31m', '\033[33m', '\033[0m'

UID_OWNER = 2        # 建群者（0@1.com）
UID_MEMBER = 3       # 群成员，用来验证扇出
UID_OUTSIDER = 5     # 局外人，用来验证越权被拒
UID_LATER = 6        # 后面被拉进群的人

OWNER_EMAIL = os.environ.get('HCAT_OWNER_EMAIL', '0@1.com')
MEMBER_EMAIL = os.environ.get('HCAT_MEMBER_EMAIL', '19178223162@163.com')
OUTSIDER_EMAIL = os.environ.get('HCAT_OUTSIDER_EMAIL', '1@qq.com')

STATE = {}           # 用例之间共享：建出来的 thread_id 等


class Fail(Exception):
    pass


def q(sql):
    return dbconf.query(sql)


def check(name, fn):
    try:
        info = fn()
    except Fail as e:
        print('%s  [FAIL] %-28s %s%s' % (RED, name, e, RESET))
        return False
    except Exception as e:
        print('%s  [FAIL] %-28s 异常: %s%s' % (RED, name, e, RESET))
        return False
    print('%s  [PASS] %-28s %s%s' % (GREEN, name, info or '', RESET))
    return True


def send_text(cli, uid, thread_id, text, touid=0):
    """发一条文本消息，返回服务端的 ID_TEXT_CHAT_MSG_RSP body"""
    import uuid
    unique_id = str(uuid.uuid4())
    cli.send(ID_TEXT_CHAT_MSG_REQ, {
        'fromuid': uid,
        'touid': touid,
        'thread_id': thread_id,
        'text_array': [{'content': text, 'unique_id': unique_id}],
    })
    body = wait_rsp(cli, ID_TEXT_CHAT_MSG_RSP, match={'thread_id': thread_id}, timeout=8)
    if body is None:
        raise Fail('没收到 ID_TEXT_CHAT_MSG_RSP')
    return body


# ==================== 用例 ====================

def t_create_group():
    """建群：owner + 1 个成员，落 chat_thread / group_chat / group_chat_member 三处"""
    cli = STATE['owner_cli']
    name = '群聊用例_%d' % int(time.time())
    cli.send(ID_CREATE_GROUP_CHAT_REQ, {
        'uid': UID_OWNER, 'name': name, 'members': [UID_MEMBER],
    })
    body = wait_rsp(cli, ID_CREATE_GROUP_CHAT_RSP, timeout=8)
    if body is None:
        raise Fail('没收到 ID_CREATE_GROUP_CHAT_RSP（服务端是旧版？）')
    if body.get('error') != 0:
        raise Fail('error=%s' % body.get('error'))
    tid = body.get('thread_id', 0)
    if not tid:
        raise Fail('回包没带 thread_id: %s' % body)
    STATE['thread_id'] = tid
    STATE['group_name'] = name
    rows = q('SELECT thread_id,name,owner_id FROM group_chat WHERE thread_id=%d;' % tid)
    if not rows:
        raise Fail('group_chat 里没有 thread_id=%d' % tid)
    members = q('SELECT user_id FROM group_chat_member WHERE thread_id=%d ORDER BY user_id;' % tid)
    got = sorted(int(r.split('\t')[0]) for r in members.split('\n') if r.strip())
    if got != sorted([UID_OWNER, UID_MEMBER]):
        raise Fail('成员不对，期望 %s 实际 %s' % (sorted([UID_OWNER, UID_MEMBER]), got))
    return 'thread_id=%d owner=%d 成员=%s' % (tid, UID_OWNER, got)


def t_thread_list_has_name():
    """会话列表：群会话要带 type=group 和群名"""
    cli = STATE['owner_cli']
    tid = STATE['thread_id']
    cli.send(ID_LOAD_CHAT_THREAD_REQ, {'uid': UID_OWNER, 'thread_id': 0})
    body = wait_rsp(cli, ID_LOAD_CHAT_THREAD_RSP, timeout=8)
    if body is None:
        raise Fail('没收到 ID_LOAD_CHAT_THREAD_RSP')
    hit = None
    for t in body.get('threads', []):
        if t.get('thread_id') == tid:
            hit = t
            break
    if hit is None:
        raise Fail('会话列表里没有 thread_id=%d（可能要翻页）' % tid)
    if hit.get('type') != 'group':
        raise Fail('type=%r，期望 group' % hit.get('type'))
    if hit.get('name') != STATE['group_name']:
        raise Fail('name=%r，期望 %r' % (hit.get('name'), STATE['group_name']))
    return 'type=group name=%s' % hit.get('name')


def t_member_send():
    """成员发消息：error=0，touid=0，thread_type=group"""
    body = send_text(STATE['owner_cli'], UID_OWNER, STATE['thread_id'], '群里的第一条消息')
    if body.get('error') != 0:
        raise Fail('error=%s' % body.get('error'))
    if body.get('touid') != 0:
        raise Fail('touid=%s，群消息应为 0' % body.get('touid'))
    if body.get('thread_type') != 'group':
        raise Fail('thread_type=%r，期望 group' % body.get('thread_type'))
    if not body.get('chat_datas'):
        raise Fail('回包没有 chat_datas')
    return 'touid=0 thread_type=group'


def t_recv_id_zero():
    """落库：群消息的 chat_message.recv_id 必须是 0"""
    tid = STATE['thread_id']
    row = q('SELECT message_id,sender_id,recv_id,content FROM chat_message '
            'WHERE thread_id=%d ORDER BY message_id DESC LIMIT 1;' % tid)
    if not row.strip():
        raise Fail('chat_message 里没有 thread_id=%d 的消息' % tid)
    parts = row.strip().split('\t')
    if int(parts[2]) != 0:
        raise Fail('recv_id=%s，群消息应为 0' % parts[2])
    if int(parts[1]) != UID_OWNER:
        raise Fail('sender_id=%s，期望 %d' % (parts[1], UID_OWNER))
    return 'message_id=%s sender=%s recv_id=0' % (parts[0], parts[1])


def t_fanout():
    """扇出：另一个在线成员要收到 ID_NOTIFY_TEXT_CHAT_MSG_REQ"""
    tid = STATE['thread_id']
    cli3 = STATE['member_cli']
    send_text(STATE['owner_cli'], UID_OWNER, tid, '这条应该扇出给成员')
    body = wait_rsp(cli3, ID_NOTIFY_TEXT_CHAT_MSG_REQ,
                    match={'thread_id': tid}, timeout=8)
    if body is None:
        raise Fail('成员 %d 没收到通知' % UID_MEMBER)
    if body.get('fromuid') != UID_OWNER:
        raise Fail('fromuid=%s，期望 %d' % (body.get('fromuid'), UID_OWNER))
    if body.get('touid') != UID_MEMBER:
        raise Fail('touid=%s，期望接收者自己的 uid %d' % (body.get('touid'), UID_MEMBER))
    return '成员 %d 收到通知，touid=%d' % (UID_MEMBER, body.get('touid'))


def t_no_self_echo():
    """发送者自己不应该再收到一份自己的消息（扇出要排除 sender）"""
    tid = STATE['thread_id']
    cli = STATE['owner_cli']
    send_text(cli, UID_OWNER, tid, '自己不该收到这条')
    body = wait_rsp(cli, ID_NOTIFY_TEXT_CHAT_MSG_REQ,
                    match={'thread_id': tid}, timeout=2.5)
    if body is not None:
        raise Fail('发送者收到了自己的消息：%s' % body)
    return '发送者没收到回环'


def t_outsider_rejected():
    """局外人往群里发消息：UidInvalid(1011)"""
    tid = STATE['thread_id']
    cli = STATE['outsider_cli']
    body = send_text(cli, UID_OUTSIDER, tid, '我不是群里的人')
    if body.get('error') != 1011:
        raise Fail('error=%s，期望 1011(UidInvalid)' % body.get('error'))
    return 'error=1011 UidInvalid'


def t_add_member():
    """成员拉人进群"""
    tid = STATE['thread_id']
    cli = STATE['owner_cli']
    cli.send(ID_ADD_GROUP_MEMBER_REQ, {
        'uid': UID_OWNER, 'thread_id': tid, 'members': [UID_LATER],
    })
    body = wait_rsp(cli, ID_ADD_GROUP_MEMBER_RSP, timeout=8)
    if body is None:
        raise Fail('没收到 ID_ADD_GROUP_MEMBER_RSP')
    if body.get('error') != 0:
        raise Fail('error=%s' % body.get('error'))
    rows = q('SELECT user_id FROM group_chat_member WHERE thread_id=%d AND user_id=%d;'
             % (tid, UID_LATER))
    if not rows.strip():
        raise Fail('group_chat_member 里没有 (%d,%d)' % (tid, UID_LATER))
    return 'added=%s，uid %d 已入群' % (body.get('added'), UID_LATER)


def t_outsider_add_rejected():
    """局外人拉人：UidInvalid(1011)"""
    tid = STATE['thread_id']
    cli = STATE['outsider_cli']
    cli.send(ID_ADD_GROUP_MEMBER_REQ, {
        'uid': UID_OUTSIDER, 'thread_id': tid, 'members': [7],
    })
    body = wait_rsp(cli, ID_ADD_GROUP_MEMBER_RSP, timeout=8)
    if body is None:
        raise Fail('没收到 ID_ADD_GROUP_MEMBER_RSP')
    if body.get('error') != 1011:
        raise Fail('error=%s，期望 1011(UidInvalid)' % body.get('error'))
    return 'error=1011 UidInvalid'


CASES = [
    ('建群', t_create_group),
    ('会话列表带群名', t_thread_list_has_name),
    ('成员发消息', t_member_send),
    ('落库 recv_id=0', t_recv_id_zero),
    ('扇出给其他成员', t_fanout),
    ('发送者不回环', t_no_self_echo),
    ('局外人发消息被拒', t_outsider_rejected),
    ('拉人进群', t_add_member),
    ('局外人拉人被拒', t_outsider_add_rejected),
]


def cleanup():
    tid = STATE.get('thread_id')
    if not tid:
        return
    print('%s清理：删除 thread_id=%d 的群与其消息%s' % (YELLOW, tid, RESET))
    for sql in (
        'DELETE FROM chat_message WHERE thread_id=%d;' % tid,
        'DELETE FROM group_chat_member WHERE thread_id=%d;' % tid,
        'DELETE FROM group_chat WHERE thread_id=%d;' % tid,
        'DELETE FROM chat_thread WHERE thread_id=%d;' % tid,
    ):
        try:
            q(sql)
        except Exception as e:
            print('  清理失败: %s | %s' % (sql, e))


def main():
    keep = '--keep' in sys.argv
    print('群聊回归用例 —— 需要重新编译过的 ChatServer')
    print('-' * 62)
    try:
        STATE['owner_cli'], o_uid, _ = login(OWNER_EMAIL)
        STATE['member_cli'], m_uid, _ = login(MEMBER_EMAIL)
        STATE['outsider_cli'], x_uid, _ = login(OUTSIDER_EMAIL)
    except Exception as e:
        print('%s[致命] 登录失败：%s%s' % (RED, e, RESET))
        print('      确认 GateServer(8080) 与 StatusServer(50052) 都活着。')
        return 2
    if o_uid != UID_OWNER or m_uid != UID_MEMBER or x_uid != UID_OUTSIDER:
        print('%s[警告] 登录到的 uid 与预期不同：owner=%s member=%s outsider=%s%s'
              % (YELLOW, o_uid, m_uid, x_uid, RESET))
        STATE['uid_owner'], STATE['uid_member'], STATE['uid_outsider'] = o_uid, m_uid, x_uid
    print('%s已登录 owner=%s member=%s outsider=%s%s'
          % (GREEN, o_uid, m_uid, x_uid, RESET))
    print('-' * 62)

    passed = 0
    failed = []
    for name, fn in CASES:
        if check(name, fn):
            passed += 1
        else:
            failed.append(name)
        time.sleep(0.2)

    if not keep:
        cleanup()
    else:
        print('%s--keep：保留 thread_id=%s%s' % (YELLOW, STATE.get('thread_id'), RESET))

    for c in ('owner_cli', 'member_cli', 'outsider_cli'):
        try:
            STATE[c].close()
        except Exception:
            pass

    print('-' * 62)
    print('通过 %d / %d' % (passed, len(CASES)))
    if failed:
        print('%s失败: %s%s' % (RED, ', '.join(failed), RESET))
        print('如果全部失败并报"没收到 xxx_RSP"，多半是服务端还是旧版 —— 用 VS2022')
        print('重新生成 ChatServer / ChatServer2 的 Debug|x64 再跑。')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
