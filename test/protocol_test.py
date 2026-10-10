#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
HChat 服务端协议级自动化测试

直接用 socket 模拟 Qt 客户端，向 ChatServer 发各种消息，
重点测【边界/异常输入】—— 这类输入最容易打崩没有 try/catch 的工作线程。

用法:
    python test/protocol_test.py            # 跑全部用例
    python test/protocol_test.py --list     # 只列用例
    python test/protocol_test.py 空搜索      # 只跑名字包含"空搜索"的用例
"""
import socket, json, struct, sys, time, subprocess, os

HOST = '127.0.0.1'
CHAT_PORT = 8990
GATE = 'http://127.0.0.1:8080'

# ---- 消息 ID（与 Client/global.h 一致）----
ID_CHAT_LOGIN = 1005
ID_CHAT_LOGIN_RSP = 1006
ID_SEARCH_USER_REQ = 1007
ID_SEARCH_USER_RSP = 1008
ID_ADD_FRIEND_REQ = 1009
ID_AUTH_FRIEND_REQ = 1013
ID_TEXT_CHAT_MSG_REQ = 1017
ID_TEXT_CHAT_MSG_RSP = 1018
ID_HEART_BEAT_REQ = 1023
ID_LOAD_CHAT_THREAD_REQ = 1025
ID_LOAD_CHAT_THREAD_RSP = 1026
ID_CREATE_PRIVATE_CHAT_REQ = 1027
ID_LOAD_CHAT_MSG_REQ = 1029
ID_LOAD_CHAT_MSG_RSP = 1030

GREEN, RED, YELLOW, RESET = '\033[32m', '\033[31m', '\033[33m', '\033[0m'


def xor_string(s):
    """复现 Client/global.cpp 里的 xorString"""
    x = len(s) % 255
    return ''.join(chr(ord(c) ^ x) for c in s)


def http_post(path, obj):
    """极简 HTTP POST（避免依赖 requests）"""
    body = json.dumps(obj).encode('utf-8')
    req = ('POST %s HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n'
           'Content-Type: application/json\r\nContent-Length: %d\r\n'
           'Connection: close\r\n\r\n' % (path, len(body))).encode() + body
    s = socket.create_connection(('127.0.0.1', 8080), timeout=5)
    s.sendall(req)
    data = b''
    while True:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
    s.close()
    head, _, payload = data.partition(b'\r\n\r\n')
    try:
        return json.loads(payload.decode('utf-8', 'replace'))
    except Exception:
        return {'_raw': payload.decode('utf-8', 'replace')[:200], '_head': head.decode('utf-8', 'replace')[:200]}


class Client:
    """模拟 Qt 客户端的 TCP 长连接"""

    def __init__(self, host=HOST, port=CHAT_PORT):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(5)
        self.buf = b''

    def send_raw(self, head_bytes, body=b''):
        """直接发原始字节（用于畸形包测试）"""
        self.sock.sendall(head_bytes + body)

    def send(self, msg_id, obj):
        """帧头是 2字节 msg_id + 2字节 len，均大端
        （对应 ChatServer 的 HEAD_ID_LEN=2 / HEAD_TOTAL_LEN=4 / network_to_host_short）"""
        body = json.dumps(obj).encode('utf-8')
        self.sock.sendall(struct.pack('>HH', msg_id, len(body)) + body)

    HEAD = 4      # 2字节 msg_id + 2字节 len

    def recv(self):
        """读一条消息；超时返回 None"""
        try:
            while len(self.buf) < self.HEAD:
                chunk = self.sock.recv(4096)
                if not chunk:
                    return None
                self.buf += chunk
            msg_id, length = struct.unpack('>HH', self.buf[:self.HEAD])
            while len(self.buf) < self.HEAD + length:
                chunk = self.sock.recv(4096)
                if not chunk:
                    return None
                self.buf += chunk
            body = self.buf[self.HEAD:self.HEAD + length]
            self.buf = self.buf[self.HEAD + length:]
            try:
                return msg_id, json.loads(body.decode('utf-8', 'replace'))
            except Exception:
                return msg_id, {'_raw': body.decode('utf-8', 'replace')}
        except socket.timeout:
            return None

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


def server_alive():
    """检查 ChatServer 端口还在不在监听"""
    try:
        s = socket.create_connection((HOST, CHAT_PORT), timeout=2)
        s.close()
        return True
    except Exception:
        return False


# ==================== 用例 ====================
CASES = []


def case(name):
    def deco(fn):
        CASES.append((name, fn))
        return fn
    return deco


# DB 里 user.pwd 存的是 xorString 之后的密文，用户在输入框里敲的是明文。
# 例如明文 123456 -> xor -> 745230，正好等于库里存的值。
PLAIN_PWD = '123456'


def _http_login(email, sent_pwd):
    return http_post('/user_login', {'email': email, 'passwd': sent_pwd})


def login(email, pwd=PLAIN_PWD):
    """走 HTTP 登录拿到 uid/token，返回 (Client, uid, token)"""
    # 先按"明文 xor 后发送"（客户端的真实行为），失败再退回明文，兼容两种服务端实现
    rsp = _http_login(email, xor_string(pwd))
    if rsp.get('error') != 0:
        rsp = _http_login(email, pwd)
    if rsp.get('error') != 0:
        raise RuntimeError('HTTP 登录失败: %s（明文=%s密文=%s）'
                           % (rsp, pwd, xor_string(pwd)))
    c = Client(port=int(rsp['port']))
    c.send(ID_CHAT_LOGIN, {'uid': rsp['uid'], 'token': rsp['token']})
    r = c.recv()
    if not r or r[0] != ID_CHAT_LOGIN_RSP:
        raise RuntimeError('聊天登录失败: %s' % (r,))
    if r[1].get('error') != 0:
        raise RuntimeError('聊天登录被拒: %s' % r[1])
    return c, rsp['uid'], rsp['token']


@case('正常登录')
def t_login(c):
    return 'uid=%s, 好友数=%d' % (c['uid'], len(c['rsp'].get('friend_list', [])))


@case('空搜索')
def t_search_empty(c):
    """★回归用例：曾经让 ChatServer 整个进程崩溃的那条"""
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': ''})
    return '已发送空搜索'


@case('空格搜索')
def t_search_space(c):
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': '   '})
    return '已发送纯空格搜索'


@case('超长搜索(1000字符)')
def t_search_long(c):
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': 'a' * 1000})
    return '已发送超长串'


@case('特殊字符搜索')
def t_search_special(c):
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': "'; DROP TABLE user;--"})
    return '已发送注入尝试'


@case('负数搜索')
def t_search_neg(c):
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': '-1'})
    return '已发送负数'


@case('浮点搜索')
def t_search_float(c):
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': '3.14'})
    return '已发送浮点数'


@case('邮箱搜索')
def t_search_email(c):
    c['cli'].send(ID_SEARCH_USER_REQ, {'uid': '111112@qq.com'})
    return '已发送邮箱'


@case('空消息文本')
def t_send_empty_text(c):
    c['cli'].send(ID_TEXT_CHAT_MSG_REQ,
                  {'fromuid': c['uid'], 'touid': 3, 'thread_id': 1, 'text_array': []})
    return '已发送空 text_array'


@case('文本消息含空内容')
def t_send_empty_content(c):
    c['cli'].send(ID_TEXT_CHAT_MSG_REQ,
                  {'fromuid': c['uid'], 'touid': 3, 'thread_id': 1,
                   'text_array': [{'content': '', 'unique_id': 'u_empty'}]})
    return '已发送 content=""'


@case('文本消息 touid=0')
def t_send_touid_zero(c):
    c['cli'].send(ID_TEXT_CHAT_MSG_REQ,
                  {'fromuid': c['uid'], 'touid': 0, 'thread_id': 1,
                   'text_array': [{'content': 'hi', 'unique_id': 'u_z'}]})
    return '已发送 touid=0'


@case('文本消息 uid=0')
def t_send_uid_zero(c):
    c['cli'].send(ID_TEXT_CHAT_MSG_REQ,
                  {'fromuid': 0, 'touid': 3, 'thread_id': 1,
                   'text_array': [{'content': 'hi', 'unique_id': 'u_y'}]})
    return '已发送 fromuid=0'


@case('加载会话列表 uid=0')
def t_threads_uid_zero(c):
    c['cli'].send(ID_LOAD_CHAT_THREAD_REQ, {'uid': 0, 'thread_id': 0})
    return '已发送 uid=0'


@case('加载历史消息 thread_id=0')
def t_msgs_thread_zero(c):
    c['cli'].send(ID_LOAD_CHAT_MSG_REQ, {'thread_id': 0, 'message_id': 0})
    return '已发送 thread_id=0'


@case('加载历史消息 巨大分页')
def t_msgs_huge(c):
    c['cli'].send(ID_LOAD_CHAT_MSG_REQ, {'thread_id': 1, 'message_id': -1})
    return '已发送 message_id=-1'


@case('创建私聊 other_id=0')
def t_create_zero(c):
    c['cli'].send(ID_CREATE_PRIVATE_CHAT_REQ, {'uid': c['uid'], 'other_id': 0})
    return '已发送 other_id=0'


@case('创建私聊 other_id=自己')
def t_create_self(c):
    c['cli'].send(ID_CREATE_PRIVATE_CHAT_REQ, {'uid': c['uid'], 'other_id': c['uid']})
    return '已发送 other_id=自己'


@case('通过好友 touid=0')
def t_auth_zero(c):
    c['cli'].send(ID_AUTH_FRIEND_REQ, {'fromuid': c['uid'], 'touid': 0, 'back': 'x'})
    return '已发送 touid=0'


@case('心跳')
def t_heartbeat(c):
    c['cli'].send(ID_HEART_BEAT_REQ, {'fromuid': c['uid']})
    return '已发送心跳'


@case('畸形JSON')
def t_bad_json(c):
    body = b'{not valid json'
    c['cli'].send_raw(struct.pack('>HH', ID_SEARCH_USER_REQ, len(body)), body)
    return '已发送畸形JSON'


@case('包体长度声明为0')
def t_zero_len(c):
    c['cli'].send_raw(struct.pack('>HH', ID_SEARCH_USER_REQ, 0))
    return '已发送 len=0'


@case('长度声明超过MAX_LENGTH(2048)')
def t_len_too_big(c):
    c['cli'].send_raw(struct.pack('>HH', ID_SEARCH_USER_REQ, 60000))
    return '已发送 len=60000（超过 MAX_LENGTH）'


@case('长度声明为负(65535)')
def t_len_max(c):
    c['cli'].send_raw(struct.pack('>HH', ID_SEARCH_USER_REQ, 65535))
    return '已发送 len=65535'


@case('未注册的消息ID')
def t_unknown_id(c):
    c['cli'].send(60000, {'x': 1})
    return '已发送未知 msg_id=60000'


# ==================== 主流程 ====================
def main():
    args = sys.argv[1:]
    if '--list' in args:
        for n, _ in CASES:
            print(' -', n)
        return 0
    filt = [a for a in args if not a.startswith('-')]

    if not server_alive():
        print('%s[致命] ChatServer(%d) 没在监听，先启动服务端再跑测试%s' % (RED, CHAT_PORT, RESET))
        return 2

    try:
        cli, uid, token = login('111112@qq.com')
    except Exception as e:
        print('%s[致命] 登录失败：%s%s' % (RED, e, RESET))
        return 2

    rsp = cli.recv()
    print('%s已登录 uid=%s%s' % (GREEN, uid, RESET))
    print('-' * 62)

    passed, failed = 0, []
    crashed_by = '(无)'
    print('每个用例发完都会检查一次服务端存活 —— 这样能精确指出')
    print('是哪个用例把它打崩的。')
    print('-' * 62)

    for idx, (name, fn) in enumerate(CASES, 1):
        if filt and not any(f in name for f in filt):
            continue
        if name == '正常登录':
            print('%s  [PASS] #%-2d %s' % (GREEN, 0, name))
            passed += 1
            continue
        if not server_alive():
            print('%s  [DEAD] 第 %d 个用例之前服务端就死了' % (RED, idx))
            crashed_by = CASES[idx - 2][0]
            break
        try:
            info = fn({'cli': cli, 'uid': uid, 'rsp': {}})
            passed += 1
            print('%s  [PASS] #%-2d %-26s %s%s' % (GREEN, idx, name, info or '', RESET))
        except Exception as e:
            print('%s  [FAIL] #%-2d %-26s %s%s' % (RED, idx, name, e, RESET))
            failed.append(name)

        # ★ 关键：每发一条就确认服务端还活着。
        #   TCP 缓冲会让 send() 成功返回，即使对端进程已经死了，
        #   所以不逐条检查的话，所有用例都会显示 PASS，
        #   最后才发现服务早就崩了 —— 这就是之前"24个全过"是假阳性的原因。
        time.sleep(0.25)
        if not server_alive():
            crashed_by = name
            print('%s  [KILLED] <- 服务端死在第 %d 个用例「%s」%s' % (RED, idx, name, RESET))
            break

    cli.close()
    time.sleep(0.6)
    alive = server_alive()
    print('-' * 62)
    print('通过 %d' % passed)
    if failed:
        print('%s用例异常: %s%s' % (YELLOW, ', '.join(failed), RESET))
    if alive:
        print('%s★ ChatServer 存活%s' % (GREEN, RESET))
    else:
        print('%s★★ ChatServer 已崩溃 —— 凶手是「%s」%s' % (RED, crashed_by, RESET))
        print('   打开 ChatServer 的控制台，最后几行日志就是崩溃点。')
        return 1
    return 0 if not failed else 1


if __name__ == '__main__':
    sys.exit(main())