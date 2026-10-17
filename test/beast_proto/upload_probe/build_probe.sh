#!/usr/bin/env bash
# 编译 upload_probe：把【真实】的 Client 上传链路（uploadmanager + httpmgr）
# 链接成一个控制台程序，打到 mock_gate 上验证契约。
#
#   ./build_probe.sh
#   ./upload_probe.exe 18082 ./mock_upload
set -e

QT=/d/QT6/6.5.3/mingw_64
CLIENT=/d/QTcode/HChat/Client
HERE=/d/QTcode/HChat/test/beast_proto/upload_probe
BUILD=$HERE/build

export PATH=/d/QT6/Tools/mingw1120_64/bin:$PATH

INC="-I$HERE -I$CLIENT -I$QT/include -I$QT/include/QtCore -I$QT/include/QtNetwork \
     -I$QT/include/QtGui -I$QT/include/QtWidgets"

mkdir -p "$BUILD"

echo "[1/4] moc ..."
"$QT/bin/moc.exe" $INC "$CLIENT/httpmgr.h"       -o "$BUILD/moc_httpmgr.cpp"
"$QT/bin/moc.exe" $INC "$CLIENT/uploadmanager.h" -o "$BUILD/moc_uploadmanager.cpp"

echo "[2/4] compile ..."
CXX="g++ -std=c++17 -O1 -Wall -DUNICODE -D_UNICODE -DWIN32 -DQT_CORE_LIB -DQT_NETWORK_LIB \
     -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_NO_DEBUG_OUTPUT=0 $INC"

$CXX -c "$HERE/main.cpp"                  -o "$BUILD/main.o"
$CXX -c "$CLIENT/uploadmanager.cpp"       -o "$BUILD/uploadmanager.o"
$CXX -c "$CLIENT/httpmgr.cpp"             -o "$BUILD/httpmgr.o"
$CXX -c "$CLIENT/global.cpp"              -o "$BUILD/global.o"
$CXX -c "$CLIENT/logcategory.cpp"         -o "$BUILD/logcategory.o"
$CXX -c "$BUILD/moc_httpmgr.cpp"          -o "$BUILD/moc_httpmgr.o"
$CXX -c "$BUILD/moc_uploadmanager.cpp"    -o "$BUILD/moc_uploadmanager.o"

echo "[3/4] link ..."
g++ -o "$HERE/upload_probe.exe" \
    "$BUILD/main.o" "$BUILD/uploadmanager.o" "$BUILD/httpmgr.o" "$BUILD/global.o" \
    "$BUILD/logcategory.o" "$BUILD/moc_httpmgr.o" "$BUILD/moc_uploadmanager.o" \
    -L"$QT/lib" \
    -lQt6Widgets -lQt6Gui -lQt6Network -lQt6Core \
    -lws2_32 -lpsapi -luser32 -lgdi32 -ladvapi32 -lole32 -luuid \
    -lshell32 -lcomdlg32 -loleaut32 -luxtheme -ldwmapi -limm32 -lwinmm \
    -lnetapi32 -lversion -lwinspool -lcrypt32 -loleaut32 -lmpr -luserenv

echo "[4/4] done: $HERE/upload_probe.exe"
