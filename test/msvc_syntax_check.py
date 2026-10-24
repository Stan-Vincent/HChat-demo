#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用真正的 MSVC（cl.exe）做语法检查，替代"只能靠 MinGW 间接猜"。

背景：本环境 MSBuild 被安全策略拦截，但 cl.exe 可以直接跑。之前服务端改动只能靠
MinGW g++ -fsyntax-only 验证，而 MinGW 和 MSVC 对头文件严格程度不同
（典型：缺 <chrono> 时 GCC 通过 <thread> 间接带入不报错，MSVC 直接报错），
导致"本地检查通过、用户 VS 一编译就红一片"。

用法：
    python test/msvc_syntax_check.py GateServer/RedisMgr.cpp [ChatServer/LogicSystem.cpp ...]

要点：
  - 通过 vcvars64.bat 初始化 INCLUDE/LIB（否则连 cstddef 都找不到）；
  - MSYS_NO_PATHCONV 无关（这里不走 bash）；
  - include 路径按服务端目录自动推导，额外路径写在下表里。
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VCVARS = r"D:\VS\vs1\VC\Auxiliary\Build\vcvars64.bat"
CL_INCLUDES = [
    r"D:\software\vcpkg\vcpkg\installed\x64-windows\include",
    r"D:\software\vcpkg\vcpkg\installed\x64-windows\include\hiredis",
    r"D:\Program Files\mysql-connector-c++-26.7.0-winx64\include",
    r"D:\software\boost_1_92_0",
]
# 源码是平铺结构：GateServer/*.cpp、ChatServer/*.cpp，共享 common。
# 这里只列"相对当前服务目录（ROOT/<svc>）的额外 include"，运行时统一再加上 /I.（服务自身目录）
# 与 /I../common 系列。
PER_SERVICE = {
    'GateServer':   [r"../common", r"../common/third_party/spdlog/include"],
    'ChatServer':   [r"../common", r"../common/third_party/spdlog/include"],
    'ChatServer2':  [r"../common", r"../common/third_party/spdlog/include"],
    'StatusServer': [r"../common", r"../common/third_party/spdlog/include"],
}


def check(files):
    if not os.path.isfile(VCVARS):
        print("FAIL 找不到 vcvars64.bat:", VCVARS)
        return 2

    # 按所属服务端分组（约定入参形如 Service/file.cpp）
    groups = {}
    for f in files:
        parts = f.replace('\\', '/').split('/')
        top = parts[0]
        rel = '/'.join(parts[1:]) if len(parts) > 1 else parts[0]
        if top not in PER_SERVICE:
            print("FAIL 无法识别 %s 属于哪个服务（应为 GateServer/ChatServer/...）" % f)
            return 2
        groups.setdefault(top, []).append(rel)

    total_err = 0
    for svc, group in groups.items():
        inc = ['/I.'] + ['/I' + p for p in PER_SERVICE[svc]] + ['/I' + p for p in CL_INCLUDES]
        for f in group:
            cmd = ('"%s" >nul 2>&1 && cd /d %s && cl /c /nologo /EHsc /std:c++17 '
                   '/DFMT_UNICODE=0 /W3 /permissive- %s %s'
                   % (VCVARS, os.path.join(ROOT, svc), ' '.join(inc), f))
            r = subprocess.run(['cmd', '/c', cmd], capture_output=True)
            out = (r.stdout + r.stderr).decode('utf-8', 'replace')
            errs = [l for l in out.splitlines() if 'error C' in l]
            total_err += len(errs)
            name = svc + '/' + f
            if errs:
                print('[FAIL] %s  (%d errors)' % (name, len(errs)))
                for l in errs[:6]:
                    print('   ', l.strip()[:160])
            else:
                print('[ OK ] %s' % name)

    print('-' * 60)
    print('合计 %d 个文件，%d 个 error' % (len(files), total_err))
    return 0 if total_err == 0 else 1


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(check(sys.argv[1:]))
