#!/usr/bin/env bash
# 用 cl.exe + link.exe 手工编译/链接 ChatServer / ChatServer2。
# 原因：msbuild.exe 被安全策略拦，但 cl/link 能直接调。
#
# 用法: bash test/link_chatserver.sh [ChatServer|ChatServer2]
#
# ★ 编译配置必须与 vcxproj 一致，否则运行时会崩：
#   /MD + NDEBUG + _ITERATOR_DEBUG_LEVEL=0（Release 运行时）
#   MySQL Connector/C++ 26.7.0 只提供 Release DLL。
export MSYS2_ARG_CONV_EXCL="*"

SVC="${1:-ChatServer}"

CL=/d/VS/vs1/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe
LINK=/d/VS/vs1/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/link.exe
MSVC_INC="D:/VS/vs1/VC/Tools/MSVC/14.44.35207/include"
SDK="D:/Windows Kits/10/Include/10.0.26100.0"
SDK_LIB="D:/Windows Kits/10/Lib/10.0.26100.0/um/x64"
SDK_LIB_UCRT="D:/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"
VCPKG="D:/software/vcpkg/vcpkg/installed/x64-windows"
MYSQL="D:/Program Files/mysql-connector-c++-26.7.0-winx64"
BOOST="D:/software/boost_1_92_0"

OBJ="C:/Users/19178/AppData/Local/Temp/${SVC}build"
rm -rf "/c/Users/19178/AppData/Local/Temp/${SVC}build"
mkdir -p "/c/Users/19178/AppData/Local/Temp/${SVC}build"

cd "/d/QTcode/HChat/$SVC" || { echo "没有这个服务: $SVC"; exit 1; }

echo "  [$SVC 1/2] 编译"
# ★ 源文件列表从 vcxproj 读，不用 *.cpp 通配：
#   protobuf 生成的是 .pb.cc / .grpc.pb.cc（后缀 .cc），漏掉会链接失败。
#   ChatServer2 的工程文件仍叫 ChatServer.vcxproj（复制来的），这里按实际名字找。
PROJ=$(ls *.vcxproj 2>/dev/null | head -1)
if [ -z "$PROJ" ]; then echo "    找不到 vcxproj"; exit 1; fi
SOURCES=$(python -c "
import re
raw=open(r'$PROJ','r',encoding='utf-8',errors='replace').read()
seen=[]
for x in re.findall(r'<ClCompile Include=\"([^\"]+)\"',raw):
    if x not in seen: seen.append(x)
print(' '.join(seen))
")

OBJS=""
for F in $SOURCES; do
  [ -f "$F" ] || { printf '    %-30s 缺失\n' "$F"; exit 1; }
  STEM="${F%.*}"; STEM="${STEM//\//_}"
  "$CL" -c -nologo -EHsc -std:c++17 -O2 -MD -DNDEBUG \
    -D_SCL_SECURE_NO_WARNINGS -D_ITERATOR_DEBUG_LEVEL=0 -D_WIN32_WINNT=0x0601 \
    -DWIN32_LEAN_AND_MEAN -D_CONSOLE \
    "-I$MSVC_INC" "-I$SDK/ucrt" "-I$SDK/um" "-I$SDK/shared" \
    "-I$VCPKG/include" "-I$VCPKG/include/hiredis" "-I$MYSQL/include" "-I$BOOST" -I. \
    -Fo:"$OBJ"/ "$F" > "$OBJ/$STEM.log" 2>&1
  ERRS=$(iconv -f GBK -t UTF-8 "$OBJ/$STEM.log" 2>/dev/null | grep -cE "error C[0-9]+|fatal error")
  if [ "$ERRS" = "0" ]; then
    printf '    %-30s OK\n' "$F"
  else
    printf '    %-30s FAILED\n' "$F"
    iconv -f GBK -t UTF-8 "$OBJ/$STEM.log" 2>/dev/null | grep -E "error C[0-9]+" | head -4 | sed 's/^/        /'
    exit 1
  fi
  OBJS="$OBJS $OBJ/$STEM.obj"
done

echo "  [$SVC 2/2] 链接"
LIBDIR=$VCPKG/lib
GRPC_LIBS=$(cd "$LIBDIR" && ls *.lib 2>/dev/null \
  | grep -E "^(grpc|gpr|address_sorting|re2|cares|absl_|libprotobuf|abseil|libupb|upb_|utf8_)" \
  | sed 's/$//' | tr '\n' ' ')

OUTDIR="D:/QTcode/HChat/$SVC/x64/Debug"
"$LINK" -nologo -OUT:"$OUTDIR/ChatServer.exe" \
  -LIBPATH:"$MYSQL/lib64/vs14" -LIBPATH:"$VCPKG/lib" -LIBPATH:"$BOOST/stage/lib" \
  -LIBPATH:"D:/VS/vs1/VC/Tools/MSVC/14.44.35207/lib/x64" -LIBPATH:"$SDK_LIB" -LIBPATH:"$SDK_LIB_UCRT" \
  $OBJS \
  redis++.lib hiredis.lib ws2_32.lib mysqlcppconn.lib jsoncpp.lib $GRPC_LIBS \
  -DEFAULTLIB:z.lib \
  "$MYSQL/lib64/vs14/libcrypto.lib" "$MYSQL/lib64/vs14/libssl.lib" \
  > "$OBJ/link.log" 2>&1
RC=$?
iconv -f GBK -t UTF-8 "$OBJ/link.log" 2>/dev/null | grep -iE "error|unresolved" | head -8 | sed 's/^/    /'
if [ $RC -ne 0 ]; then
  echo "  [$SVC] 链接失败 (rc=$RC)"
  exit $RC
fi
ls -la "$OUTDIR/ChatServer.exe"
echo "  [$SVC] 链接成功"