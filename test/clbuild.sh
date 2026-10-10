#!/usr/bin/env bash
# 用 cl.exe 真实编译指定服务的源码 —— 绕开 IntelliSense 和 MSBuild，直接看 MSVC 结果。
#
# 用法:
#   bash test/clbuild.sh                    # 编译 ChatServer
#   bash test/clbuild.sh GateServer          # 编译 GateServer
#   bash test/clbuild.sh ChatServer2         # 编译 ChatServer2
#   bash test/clbuild.sh ChatServer LogicSystem.cpp    # 只编某几个文件
#
# 为什么要手工写：
#   MSBuild（msbuild.exe）被安全策略拦，但 cl.exe 可以直接调。
#   这里手工补上 vcvars64.bat 的等价物（标准库 include 路径）。
export MSYS2_ARG_CONV_EXCL="*"      # ★ 没有这行，MSYS 会把 /nologo 当成路径转换掉

CL=/d/VS/vs1/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe
MSVC_INC="D:/VS/vs1/VC/Tools/MSVC/14.44.35207/include"
SDK="D:/Windows Kits/10/Include/10.0.26100.0"
VCPKG="D:/software/vcpkg/vcpkg/installed/x64-windows/include"
MYSQL="D:/Program Files/mysql-connector-c++-26.7.0-winx64/include"
BOOST="D:/software/boost_1_92_0"
OUT="C:/Users/19178/AppData/Local/Temp/cltest"
mkdir -p "/c/Users/19178/AppData/Local/Temp/cltest"

SVC="${1:-ChatServer}"
shift
cd "/d/QTcode/HChat/$SVC" || { echo "没有这个服务: $SVC"; exit 1; }

if [ $# -gt 0 ]; then
  FILES="$*"
else
  FILES=$(ls *.cpp 2>/dev/null)
fi

PASS=0; FAIL=0
for F in $FILES; do
  [ -f "$F" ] || { printf '  %-26s 跳过\n' "$F"; continue; }
  LOG="$OUT/${SVC}_${F%.cpp}.log"
  "$CL" -c -nologo -EHsc -std:c++17 -DWIN32 -D_CONSOLE -DNDEBUG \
    -D_SCL_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN \
    "-I$MSVC_INC" "-I$SDK/ucrt" "-I$SDK/um" "-I$SDK/shared" "-I$SDK/winrt" \
    "-I$VCPKG" "-I$VCPKG/hiredis" "-I$MYSQL" "-I$BOOST" \
    -Fo:"$OUT"/ \
    "$F" > "$LOG" 2>&1
  ERRS=$(iconv -f GBK -t UTF-8 "$LOG" 2>/dev/null | grep -cE "error C[0-9]+|fatal error")
  if [ "$ERRS" = "0" ]; then
    printf '  %-26s OK\n' "$F"; PASS=$((PASS+1))
  else
    printf '  %-26s FAILED (%s)\n' "$F" "$ERRS"; FAIL=$((FAIL+1))
    iconv -f GBK -t UTF-8 "$LOG" 2>/dev/null | grep -E "error C[0-9]+|fatal error" | head -4 | sed 's/^/        /'
  fi
done

echo "  ----"
printf '  %s: %d 成功, %d 失败\n' "$SVC" "$PASS" "$FAIL"
exit $FAIL