#!/bin/bash
# 服务端改动后的快速语法体检（不需要链接，因此 MinGW 也能跑 MSVC 工程的源码）。
#
# 为什么要 -include：MSVC 的 <atomic>/<memory> 会顺带把 <condition_variable>、
# <thread> 传递包含进来，MinGW 不会，所以这里手动补上，避免看到一堆与本改动
# 无关的既有报错。
#
# 用法： bash test/syntax_check.sh [额外要检查的 .cpp ...]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH=/d/QT6/Tools/mingw1120_64/bin:$PATH

INC=(-I. "-I..\common" "-I..\common\third_party\spdlog\include"
     -I/d/software/vcpkg/vcpkg/installed/x64-windows/include
     -I/d/software/vcpkg/vcpkg/installed/x64-windows/include/hiredis
     "-I/d/Program Files/mysql-connector-c++-26.7.0-winx64/include"
     -I/d/software/boost_1_92_0)
PRE=(-include condition_variable -include thread -include mutex -include chrono)

if [ $# -gt 0 ]; then
    FILES=("$@")
else
    FILES=(MysqlDao.cpp MysqlMgr.cpp LogicSystem.cpp ChatServiceImpl.cpp)
fi

fail=0
# 只检查 ChatServer：ChatServer2 自 2026-10-18 起不再持有源码副本，
# 它通过 ..\ChatServer\SharedSources.props 引用同一份源码，所以查一份就够。
for srv in ChatServer; do
    for f in "${FILES[@]}"; do
        cd "$ROOT/$srv" || continue
        out=$(g++ -fsyntax-only -std=c++17 -w "${PRE[@]}" "${INC[@]}" \
              -DFMT_UNICODE=0 "$f" 2>&1 | grep -E "error:" | head -8)
        if [ -z "$out" ]; then
            echo "OK   $srv/$f"
        else
            echo "FAIL $srv/$f"
            echo "$out"
            fail=1
        fi
    done
done
exit $fail
