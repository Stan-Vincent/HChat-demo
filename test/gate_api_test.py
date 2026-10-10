#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
GateServer 新接口的自动化测试（上传图片 / 更新资料）

用法: python test/gate_api_test.py
"""
import base64, json, socket, os, sys

HOST, PORT = '127.0.0.1', 8080

GREEN, RED, YELLOW, RESET = '\033[32m', '\033[31m', '\033[33m', '\033[0m'


def post(path, obj, timeout=6):
    body = json.dumps(obj).encode('utf-8')
    req = ('POST %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\n'
           'Content-Length: %d\r\nConnection: close\r\n\r\n'
           % (path, HOST, PORT, len(body))).encode() + body
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall(req)
    data = b''
    while True:
        try:
            c = s.recv(4096)
        except socket.timeout:
            break
        if not c:
            break
        data += c
    s.close()
    head, _, payload = data.partition(b'\r\n\r\n')
    try:
        return json.loads(payload.decode('utf-8', 'replace'))
    except Exception:
        return {'_http_raw': payload.decode('utf-8', 'replace')[:200],
                '_status': head.decode('utf-8', 'replace').split('\r\n')[0]}


# 1x1 像素的合法 PNG
PNG_1PX = base64.b64decode(
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==')

CASES = []


def case(name):
    def d(fn):
        CASES.append((name, fn))
        return fn
    return d


@case('上传 1x1 PNG')
def t_upload_ok():
    r = post('/upload_image', {'uid': 5, 'file_name': 'test.png',
                               'data': base64.b64encode(PNG_1PX).decode()})
    assert r.get('error') == 0, '上传失败: %s' % r
    p = os.path.join(r'D:\QTcode\HChat\GateServer', 'upload', r['unique_name'])
    assert os.path.exists(p), '文件没落盘: %s' % p
    size = os.path.getsize(p)
    assert size == len(PNG_1PX), '大小不对 %d != %d' % (size, len(PNG_1PX))
    return 'url=%s size=%d 落盘✓' % (r['url'], size)


@case('上传空数据应被拒')
def t_upload_empty():
    r = post('/upload_image', {'uid': 5, 'file_name': 'a.png', 'data': ''})
    assert r.get('error') != 0, '空数据居然成功了'
    return 'error=%s msg=%s' % (r.get('error'), r.get('msg'))


@case('上传非法 base64')
def t_upload_bad_b64():
    r = post('/upload_image', {'uid': 5, 'file_name': 'a.png', 'data': '!!!not-base64!!!'})
    assert r.get('error') != 0, '非法 base64 居然成功了'
    return 'error=%s msg=%s' % (r.get('error'), r.get('msg'))


@case('上传带换行的 base64（模拟传输折行）')
def t_upload_wrapped():
    b64 = base64.b64encode(PNG_1PX).decode()
    wrapped = '\r\n'.join(b64[i:i + 20] for i in range(0, len(b64), 20))
    r = post('/upload_image', {'uid': 5, 'file_name': 'w.png', 'data': wrapped})
    assert r.get('error') == 0, '折行 base64 应该能解码: %s' % r
    return 'size=%d ✓' % r.get('size')


@case('上传 .exe 后缀（应被强制改成 .jpg）')
def t_upload_ext():
    b64 = base64.b64encode(PNG_1PX).decode()
    r = post('/upload_image', {'uid': 5, 'file_name': 'evil.exe', 'data': b64})
    assert r.get('error') == 0
    assert r['unique_name'].endswith('.jpg'), '后缀没被改: %s' % r['unique_name']
    return 'evil.exe -> %s ✓' % r['unique_name']


@case('更新资料')
def t_update_ok():
    r = post('/update_userinfo', {'uid': 5, 'nick': '自动化测试昵称',
                                  'desc': '由 gate_api_test 写入', 'sex': 1,
                                  'icon': ':/res/head_2.jpg'})
    assert r.get('error') == 0, '更新失败: %s' % r
    return 'nick=%s desc长度=%d' % (r.get('nick'), len(r.get('desc', '')))


@case('更新资料 uid=0 应被拒')
def t_update_uid0():
    r = post('/update_userinfo', {'uid': 0, 'nick': 'x', 'desc': '', 'sex': 0, 'icon': ''})
    assert r.get('error') != 0, 'uid=0 居然成功了'
    return 'error=%s ✓' % r.get('error')


@case('昵称超长应被截断到 21')
def t_update_truncate():
    long_nick = 'A' * 50
    r = post('/update_userinfo', {'uid': 5, 'nick': long_nick, 'desc': '', 'sex': 0,
                                  'icon': ''})
    assert r.get('error') == 0
    assert len(r.get('nick', '')) <= 21, '没截断: %d' % len(r.get('nick', ''))
    return '50 -> %d ✓' % len(r.get('nick', ''))


@case('签名超长应被截断到 200')
def t_update_desc_truncate():
    r = post('/update_userinfo', {'uid': 5, 'nick': 'n', 'desc': 'B' * 500,
                                  'sex': 0, 'icon': ''})
    assert r.get('error') == 0
    assert len(r.get('desc', '')) <= 200, '没截断: %d' % len(r.get('desc', ''))
    return '500 -> %d ✓' % len(r.get('desc', ''))


def main():
    print('\n' + '=' * 56)
    print('  GateServer 新接口自动化测试')
    print('=' * 56)
    passed, failed = 0, []
    for name, fn in CASES:
        try:
            info = fn()
            print('%s  [PASS] %-28s %s%s' % (GREEN, name, info or '', RESET))
            passed += 1
        except AssertionError as e:
            print('%s  [FAIL] %-28s %s%s' % (RED, name, e, RESET))
            failed.append(name)
        except Exception as e:
            print('%s  [ERR ] %-28s %s%s' % (YELLOW, name, e, RESET))
            failed.append(name)
    print('-' * 56)
    print('通过 %d / %d' % (passed, len(CASES)))
    if failed:
        print('%s失败: %s%s' % (RED, ', '.join(failed), RESET))
        return 1
    print('%s全部通过%s' % (GREEN, RESET))
    return 0


if __name__ == '__main__':
    sys.exit(main())