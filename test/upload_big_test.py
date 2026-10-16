#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""大文件上传回归测试 —— 验证 GateServer body_limit 8MB -> 32MB 的修复

背景（2026-10-15 修的 bug）：
    GateServer/HttpConnection.cpp 里 body_limit 原为 8MB，而客户端上传是 base64
    编码（体积 ×4/3），20MB 文件约 27MB，在 HTTP 解析层就被 http::error::body_limit
    掐断，客户端只看到 WinError 10053（连接被重置）；
    LogicSystem 里 20MB 的判断根本轮不到，报不出"文件太大"这种可读错误。
    现在 body_limit = 32MB，于是分工变成：
        raw <= 20MB  -> LogicSystem 正常接收，返回 url
        20MB < raw <= ~24MB -> LogicSystem 明确返回 "file too large (max 20MB)"
        base64 > 32MB -> HTTP 解析层拒绝（连接断开）

用例（raw = 解码后的原始文件大小）：
    1) 19MB -> 必须成功（HTTP 层 + LogicSystem 层都放过）   <- 修复前必挂
    2) 21MB -> 必须拿到 200 + error != 0 + "file too large"  <- 关键：可读错误而非断连
    3) 35MB -> base64 ≈ 47MB > 32MB，HTTP 层拒绝（允许连接断开）
"""
import base64
import json
import os
import sys
import urllib.error
import urllib.request

BASE = 'http://127.0.0.1:8080'
UID = 2
GREEN, RED, YELLOW, RESET = '\033[32m', '\033[31m', '\033[33m', '\033[0m'

results = []


def report(name, ok, detail=''):
    results.append((name, ok))
    print('%s  [%s] %-34s %s%s' % (GREEN if ok else RED,
                                    'PASS' if ok else 'FAIL', name, detail, RESET))


def upload_file(path, raw_size):
    """把 path 指向的文件 base64 后 POST /upload_file，返回 (status, obj, err)"""
    with open(path, 'rb') as f:
        b64 = base64.b64encode(f.read()).decode('ascii')
    body = json.dumps({'uid': UID, 'file_name': os.path.basename(path),
                       'data': b64}).encode('utf-8')
    req = urllib.request.Request(
        BASE + '/upload_file', data=body,
        headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8', 'replace')), None
    except urllib.error.HTTPError as e:
        # 非 2xx：服务端回了响应体（我们的 LogicSystem 错误就是走这条路）
        try:
            obj = json.loads(e.read().decode('utf-8', 'replace'))
        except Exception:
            obj = {}
        return e.code, obj, None
    except Exception as e:
        # 连接被重置 / 解析层拒绝
        return None, {}, repr(e)


def main():
    tmp = 'upload_big_tmp.bin'
    try:
        # ---------- 1) 19MB：必须成功 ----------
        raw = os.urandom(19 * 1024 * 1024)
        with open(tmp, 'wb') as f:
            f.write(raw)
        st, obj, err = upload_file(tmp, len(raw))
        if err:
            report('19MB 上传成功', False, '连接失败: %s' % err)
        else:
            report('19MB 上传成功', st == 200 and obj.get('error') == 0,
                   'status=%s error=%s url=%s' % (st, obj.get('error'), obj.get('url', '')))

        # ---------- 2) 21MB：必须被 LogicSystem 明确拒绝 ----------
        raw = os.urandom(21 * 1024 * 1024)
        with open(tmp, 'wb') as f:
            f.write(raw)
        st, obj, err = upload_file(tmp, len(raw))
        if err:
            report('21MB 被明确拒绝', False,
                   '连接被断（%s）—— 说明 HTTP 层还在掐，而不是 LogicSystem 的 20MB 判断' % err)
        else:
            msg = str(obj.get('msg', ''))
            report('21MB 被明确拒绝',
                   st == 200 and obj.get('error') != 0 and 'too large' in msg,
                   'status=%s error=%s msg=%s' % (st, obj.get('error'), msg))

        # ---------- 3) 35MB：base64 > 32MB，HTTP 层拒绝 ----------
        raw = os.urandom(35 * 1024 * 1024)
        with open(tmp, 'wb') as f:
            f.write(raw)
        st, obj, err = upload_file(tmp, len(raw))
        if err:
            report('35MB 被 HTTP 层拒绝', True, '连接断开: %s' % err)
        else:
            # 万一 body_limit 真放到 64MB 以上，LogicSystem 的 20MB 也会拦住，同样算过
            ok = (st == 200 and obj.get('error') != 0) or (st != 200)
            report('35MB 被 HTTP 层拒绝', ok,
                   'status=%s error=%s msg=%s' % (st, obj.get('error'), obj.get('msg', '')))
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
        # 清理服务端落盘的测试文件
        for name in os.listdir('.'):
            if name.startswith('upload_big_tmp'):
                try:
                    os.remove(name)
                except OSError:
                    pass

    print('-' * 62)
    bad = [n for n, ok in results if not ok]
    if bad:
        print('%s未通过: %s%s' % (RED, ', '.join(bad), RESET))
        return 1
    print('%s全部通过%s' % (GREEN, RESET))
    return 0


if __name__ == '__main__':
    sys.exit(main())
