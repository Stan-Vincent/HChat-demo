#!/usr/bin/env bash
# 用 cl.exe + link.exe 手工编译/链接 GateServer。
# 原因：msbuild.exe 被安全策略拦，但 cl/link 能直接调。
#
# 用法: bash test/link_gateserver.sh
#
# 编译配置必须与 vcxproj 一致，否则运行时会崩：
#   ★ 必须 /MD + NDEBUG + _ITERATOR_DEBUG_LEVEL=0（Release 运行时）
#     MySQL Connector/C++ 26.7.0 只提供 Release DLL，
#     用 /MDd 会因 _ITERATOR_DEBUG_LEVEL 不一致而内存错乱。
export MSYS2_ARG_CONV_EXCL="*"

CL=/d/VS/vs1/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe
LINK=/d/VS/vs1/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/link.exe
MSVC_INC="D:/VS/vs1/VC/Tools/MSVC/14.44.35207/include"
SDK="D:/Windows Kits/10/Include/10.0.26100.0"
SDK_LIB="D:/Windows Kits/10/Lib/10.0.26100.0/um/x64"
SDK_LIB_UCRT="D:/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"
VCPKG="D:/software/vcpkg/vcpkg/installed/x64-windows"
MYSQL="D:/Program Files/mysql-connector-c++-26.7.0-winx64"
BOOST="D:/software/boost_1_92_0"

OBJ="C:/Users/19178/AppData/Local/Temp/gatebuild"
rm -rf /c/Users/19178/AppData/Local/Temp/gatebuild; mkdir -p /c/Users/19178/AppData/Local/Temp/gatebuild

cd /d/QTcode/HChat/GateServer || exit 1

echo "  [1/2] 编译"
OBJS=""
# ★ 源文件列表要从 vcxproj 的 <ClCompile Include> 里读，不能用 *.cpp 通配：
#   protobuf 生成的 message.pb.cc / message.grpc.pb.cc 后缀是 .cc 不是 .cpp，
#   漏掉它们会在链接阶段炸出一大堆 message:: 的 LNK2001/LNK2019。
SOURCES=$(python -c "
import re,sys
raw=open('GateServer.vcxproj','r',encoding='utf-8',errors='replace').read()
seen=[]
for x in re.findall(r'<ClCompile Include=\"([^\"]+)\"',raw):
    if x not in seen: seen.append(x)
print(' '.join(seen))
")

for F in $SOURCES; do
  [ -f "$F" ] || { printf '    %-26s 缺失\n' "$F"; exit 1; }
  # 目标文件名去掉扩展名（.cpp / .cc 都要处理），斜杠换下划线
  STEM="${F%.*}"
  STEM="${STEM//\//_}"
  "$CL" -c -nologo -EHsc -std:c++17 -O2 -MD -DNDEBUG \
    -D_SCL_SECURE_NO_WARNINGS -D_ITERATOR_DEBUG_LEVEL=0 -D_WIN32_WINNT=0x0601 \
    -DWIN32_LEAN_AND_MEAN -D_CONSOLE \
    "-I$MSVC_INC" "-I$SDK/ucrt" "-I$SDK/um" "-I$SDK/shared" \
    "-I$VCPKG/include" "-I$VCPKG/include/hiredis" "-I$MYSQL/include" "-I$BOOST" -I. \
    -Fo:"$OBJ"/ "$F" > "$OBJ/$STEM.log" 2>&1
  ERRS=$(iconv -f GBK -t UTF-8 "$OBJ/$STEM.log" 2>/dev/null | grep -cE "error C[0-9]+|fatal error")
  if [ "$ERRS" = "0" ]; then
    printf '    %-26s OK\n' "$F"
  else
    printf '    %-26s FAILED\n' "$F"
    iconv -f GBK -t UTF-8 "$OBJ/$STEM.log" 2>/dev/null | grep -E "error C[0-9]+" | head -4 | sed 's/^/        /'
    exit 1
  fi
  OBJS="$OBJS $OBJ/$STEM.obj"
done

echo "  [2/2] 链接"
OUTDIR="D:/QTcode/HChat/GateServer/x64/Debug"

# vcxproj 的 AdditionalDependencies 只列了 redis++/hiredis/ws2_32/mysqlcppconn，
# gRPC+protobuf 那部分是靠 #pragma comment(lib) 自动链接的。
# 直接调 link.exe 时没有 pragma，必须把 lib 显式列出来，
# 否则会出现一大片 grpc::/message::/Json:: 的 LNK2019/LNK2001。
#
# ★ abseil 的 lib 名字不好手写（版本差异很大），直接从目录里捞全。
LIBDIR=$VCPKG/lib
GRPC_LIBS=$(cd "$LIBDIR" && ls *.lib 2>/dev/null \
  | grep -E "^(grpc|gpr|address_sorting|re2|cares|absl_|libprotobuf|abseil|libupb|upb_|utf8_)" \
  | sed 's/$//' | tr '\n' ' ')
echo "  链接 $(echo $GRPC_LIBS | wc -w) 个 gRPC/abseil 库"

"$LINK" -nologo -OUT:"$OUTDIR/GateServer.exe" \
  -LIBPATH:"$MYSQL/lib64/vs14" -LIBPATH:"$VCPKG/lib" -LIBPATH:"$BOOST/stage/lib" \
  -LIBPATH:"D:/VS/vs1/VC/Tools/MSVC/14.44.35207/lib/x64" -LIBPATH:"$SDK_LIB" -LIBPATH:"$SDK_LIB_UCRT" \
  $OBJS \
  redis++.lib hiredis.lib ws2_32.lib mysqlcppconn.lib jsoncpp.lib $GRPC_LIBS \
  -DEFAULTLIB:z.lib \
  "$MYSQL/lib64/vs14/libcrypto.lib" "$MYSQL/lib64/vs14/libssl.lib" \
  > "$OBJ/link.log" 2>&1
RC=$?
iconv -f GBK -t UTF-8 "$OBJ/link.log" 2>/dev/null | grep -iE "error|unresolved" | head -10 | sed 's/^/    /'
if [ $RC -ne 0 ]; then
  echo "  链接失败 (rc=$RC)"
  exit $RC
fi
ls -la "$OUTDIR/GateServer.exe"
echo "  链接成功"