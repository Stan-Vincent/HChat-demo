#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""大文件上传回归测试 —— 适配 2026-10-17 落地的【流式原始字节】协议

协议变了（与 2026-10-15 那版的 base64/JSON 信封完全不同）：
    旧：POST /upload_file，application/json，body = {"uid":..,"file_name":..,
                                                     "data": "<base64>"}
    新：POST /upload_file?uid=<n>&file_name=<name>
        Content-Type: application/octet-stream
        body = 文件原始字节（不编码、不包 JSON）
    回包不变：{error, url, unique_name, size, md5}

为什么必须改协议：base64 让体积 ×4/3，客户端还要再过 QString(UTF-16) 和
QJsonValue 深拷贝，300MB 的文件客户端峰值要到 1.5–2.7GB。改成流式之后
服务端用 http::buffer_body 边收边写盘，实测 300MB 落盘服务端峰值 10.6MB。

用例：
    1) 19MB  -> 成功；再 GET 回来逐字节比对（证明真的落盘了，不是空壳）
    2) 301MB -> 必须拿到【可读的 JSON 错误】，不能是连接重置
               （这正是旧实现的病：body_limit 在解析层掐断，客户端只看到
                 WinError 10053，UI 上就是一个没头没尾的"网络错误"）
    3) 老协议（base64 + JSON 信封）-> 必须被拒，msg 含 not application/json
    4) 缺 file_name -> 必须被拒

用法：
    python test/upload_big_test.py                      # 打真实 GateServer:8080
    GATE_URL=http://127.0.0.1:18080 python test/upload_big_test.py   # 打 mock
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request
import http.client
from urllib.parse import urlparse, quote

BASE = os.environ.get('GATE_URL', 'http://127.0.0.1:8080')
UID = 2
GREEN, RED, RESET = '\033[32m', '\033[31m', '\033[0m'

results = []
PATTERN = bytes((i * 31 + 7) & 0xFF for i in range(1024 * 1024))   # 1MB 确定图案


def report(name, ok, detail=''):
    results.append((name, ok))
    print('%s  [%s] %-30s %s%s' % (GREEN if ok else RED,
                                    'PASS' if ok else 'FAIL', name, detail, RESET))


def _host():
    u = urlparse(BASE)
    return u.hostname, (u.port or 80)


def chunks(size):
    """按 1MB 图案分块产出 size 字节，避免把几百 MB 一次性装进内存"""
    left = size
    while left > 0:
        n = min(left, len(PATTERN))
        yield PATTERN[:n]
        left -= n


def post_raw(path, size, ctype='application/octet-stream', timeout=300):
    """发一条 body 为原始字节的 POST。返回 (status, obj, err)"""
    host, port = _host()
    conn = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        conn.putrequest('POST', path, skip_accept_encoding=True)
        conn.putheader('Content-Type', ctype)
        conn.putheader('Content-Length', str(size))
        conn.endheaders()
        for c in chunks(size):
            conn.send(c)
        resp = conn.getresponse()
        raw = resp.read().decode('utf-8', 'replace')
        try:
            return resp.status, json.loads(raw), None
        except Exception:
            return resp.status, {}, raw[:200]
    except Exception as e:
        return None, {}, repr(e)
    finally:
        conn.close()


def get_back(url_path, timeout=120):
    """把服务端上的文件 GET 回来，返回 bytes 或 None"""
    try:
        req = urllib.request.Request(BASE + '/' + url_path.lstrip('/'))
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.read()
    except Exception:
        return None


def main():
    tmp = 'upload_big_tmp.bin'
    server_files = []
    try:
        # ---------- 1) 19MB：必须成功，且能原样取回 ----------
        n = 19 * 1024 * 1024
        with open(tmp, 'wb') as f:
            for c in chunks(n):
                f.write(c)
        t0 = time.time()
        st, obj, err = post_raw(
            '/upload_file?uid=%d&file_name=%s' % (UID, quote('big19.bin')), n)
        cost = time.time() - t0
        if err:
            report('19MB 上传成功', False, '连接失败: %s' % err)
        else:
            ok = (st == 200 and obj.get('error') == 0 and obj.get('size') == n)
            if ok and obj.get('url'):
                server_files.append(obj['url'])
                back = get_back(obj['url'])
                with open(tmp, 'rb') as f:
                    local = f.read()
                ok = (back is not None and back == local)
                if not ok:
                    report('19MB 上传成功', False,
                           'GET 回来的内容不一致（len=%s）' %
                           (len(back) if back is not None else 'None'))
                else:
                    report('19MB 上传成功', True,
                           '%.1fs size=%s url=%s' % (cost, obj.get('size'), obj.get('url')))
            else:
                report('19MB 上传成功', ok,
                       'status=%s error=%s msg=%s' % (st, obj.get('error'), obj.get('msg', '')))

        # ---------- 2) 301MB：必须被明确拒绝，不能是连接重置 ----------
        n = 301 * 1024 * 1024
        t0 = time.time()
        st, obj, err = post_raw(
            '/upload_file?uid=%d&file_name=%s' % (UID, quote('huge301.bin')), n,
            timeout=300)
        cost = time.time() - t0
        if err:
            # ★ 这里失败就是旧病复发：被 HTTP 解析层掐断，客户端只见到 reset
            report('301MB 被明确拒绝', False,
                   '连接被断（%s）—— HTTP 层还在掐，不是 LogicSystem 的 300MB 判断' % err)
        else:
            msg = str(obj.get('msg', ''))
            report('301MB 被明确拒绝',
                   st == 200 and obj.get('error') != 0 and 'too large' in msg,
                   '%.1fs status=%s error=%s msg=%s' % (cost, st, obj.get('error'), msg))

        # ---------- 3) 老协议（base64 + JSON 信封）必须被拒 ----------
        body = json.dumps({'uid': UID, 'file_name': 'old.bin',
                           'data': 'AAAA'}).encode('utf-8')
        req = urllib.request.Request(
            BASE + '/upload_file?uid=%d&file_name=old.bin' % UID, data=body,
            headers={'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                st, obj, err = r.status, json.loads(r.read().decode('utf-8', 'replace')), None
        except urllib.error.HTTPError as e:
            st, obj, err = e.code, {}, None
        except Exception as e:
            st, obj, err = None, {}, repr(e)
        if err:
            report('老 JSON 信封被拒', False, '连接失败: %s' % err)
        else:
            report('老 JSON 信封被拒',
                   st == 200 and obj.get('error') != 0
                   and 'not application/json' in str(obj.get('msg', '')),
                   'status=%s msg=%s' % (st, obj.get('msg', '')))

        # ---------- 4) 缺 file_name ----------
        st, obj, err = post_raw('/upload_file?uid=%d' % UID, 1024)
        if err:
            report('缺 file_name 被拒', False, '连接失败: %s' % err)
        else:
            report('缺 file_name 被拒',
                   st == 200 and obj.get('error') != 0
                   and obj.get('msg') == 'missing file_name',
                   'status=%s msg=%s' % (st, obj.get('msg', '')))
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
        # 清理服务端落盘的测试文件（GateServer 的工作目录，相对仓库根目录）
        gate_upload = os.path.join('GateServer', 'upload')
        for url in server_files:
            path = os.path.join(gate_upload, os.path.basename(url))
            if os.path.exists(path):
                try:
                    os.remove(path)
                    print('    已删除服务端测试文件:', path)
                except OSError as e:
                    print('    删不掉服务端测试文件 %s: %s' % (path, e))

    print('-' * 62)
    bad = [n for n, ok in results if not ok]
    if bad:
        print('%s未通过: %s%s' % (RED, ', '.join(bad), RESET))
        return 1
    print('%s全部通过%s' % (GREEN, RESET))
    return 0


if __name__ == '__main__':
    sys.exit(main())
