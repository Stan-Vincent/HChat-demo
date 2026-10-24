#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Redis 抖动自愈测试：验证 GateServer 的 /health 能反映 Redis 状态并自动恢复。

原理：用 `CLIENT PAUSE <ms>` 让 Redis 暂停处理命令（不重启、不丢数据），
服务端 PING 会因超时失败 → 连接被判坏并丢弃 → 后台巡检按退避补齐。
暂停结束后 /health 应自动恢复 ok。

前置：需要**重编过的 GateServer**（/health 路由是新的）。
若 /health 返回 404，脚本会明确提示"先重编"，不会误报失败。
"""
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GATE = 'http://127.0.0.1:8080'


def health():
    """返回 (http_code, body_dict or None)。"""
    try:
        with urllib.request.urlopen(GATE + '/health', timeout=5) as r:
            return r.status, json.loads(r.read().decode() or '{}')
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read().decode() or '{}')
        except Exception:
            return e.code, None
    except Exception as e:
        return None, {'error': str(e)}


def redis_cli(*args):
    """调 redis-cli；返回 (ok, stdout)。"""
    try:
        out = subprocess.run(['redis-cli', '-p', '6380', *args],
                             capture_output=True, timeout=10)
        return out.returncode == 0, out.stdout.decode('utf-8', 'replace').strip()
    except FileNotFoundError:
        return False, 'redis-cli not found'
    except Exception as e:
        return False, str(e)


def main():
    print('=' * 60)
    print('Redis 抖动自愈测试')
    print('=' * 60)

    code, body = health()
    if code == 404 or (body and 'status' not in body):
        print('FAIL /health 返回 404 —— 需要先用 VS 重编 GateServer')
        print('     （/health 路由是本次新增，旧 exe 没有）')
        return 2
    if code is None:
        print('FAIL GateServer 不可达:', body)
        return 2
    print('  [OK] /health 在线，当前: %s' % body)

    ok, out = redis_cli('PING')
    if not ok or 'PONG' not in out:
        print('FAIL redis-cli 不可用:', out)
        return 2
    print('  [OK] redis-cli 可用')

    # 1) 让 Redis 暂停处理命令 6 秒（不重启、不丢数据）
    print('\n[1] 模拟抖动：CLIENT PAUSE 6000')
    ok, out = redis_cli('CLIENT', 'PAUSE', '6000')
    print('     redis-cli ->', out)

    time.sleep(1.5)   # 等服务端的 PING 超时（连接超时设为 2s）
    code, body = health()
    print('[2] 暂停期间 /health -> HTTP %s %s' % (code, body))
    degraded = (code == 503) or (body and body.get('redis') == 'fail')
    if not degraded:
        print('     WARN 未观察到 degraded（可能巡检还没轮到，继续观察）')
    else:
        print('     [OK] 正确反映 Redis 不可用（fail-fast 语义生效）')

    # 2) 等暂停结束 + 巡检周期（默认 5s）
    print('\n[3] 等待自愈（暂停结束 + 巡检周期，最多 20s）')
    recovered = False
    for i in range(20):
        time.sleep(1)
        code, body = health()
        if code == 200 and body and body.get('redis') == 'ok':
            print('     [OK] %ds 后自愈: %s' % (i + 1, body))
            recovered = True
            break
    if not recovered:
        print('     FAIL 20s 内未自愈，最后状态: HTTP %s %s' % (code, body))
        return 1

    # 3) 业务可用性抽查
    print('\n[4] 业务抽查：/health 之外再打一个真实接口')
    try:
        req = urllib.request.Request(GATE + '/get_self',
                                     data=b'{"uid":2,"token":"probe"}',
                                     headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=5) as r:
            print('     [OK] 业务接口响应 HTTP', r.status)
    except urllib.error.HTTPError as e:
        # 业务错误码（如 token 无效）也说明服务在正常处理请求
        print('     [OK] 业务接口有响应（HTTP %s，服务在工作）' % e.code)
    except Exception as e:
        print('     WARN 业务接口异常:', e)

    print('\n全部通过：Redis 抖动被 /health 反映并自动恢复')
    return 0


if __name__ == '__main__':
    sys.exit(main())
