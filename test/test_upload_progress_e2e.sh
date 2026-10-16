#!/bin/bash
# ============================================================
# 上传进度改动回归：客户端能正常登录（不崩）+ 上传链路仍通
#   客户端上传 UI 路径需要文件对话框交互，无法全自动，
#   所以这里验证"没把登录/上传链路搞坏"，进度回调本身靠
#   client.log 里的 "upload progress: N%" 日志由人工上传时确认。
# ============================================================
set -u
ROOT=/d/QTcode/HChat
BIN=$ROOT/Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug/bin
export QT_LOGGING_RULES='*.debug=true'

echo "[1] 起后端（ChatServer）..."
( cd "$ROOT/ChatServer" && ./x64/Debug/ChatServer.exe > /tmp/upload_e2e_chat.log 2>&1 & )
sleep 6
echo "    GateServer: $(netstat -ano 2>/dev/null | grep -c ':8080 .*LISTENING')"
echo "    ChatServer: $(netstat -ano 2>/dev/null | grep -c ':8990 .*LISTENING')"

echo "[2] auto_login=true 并启动客户端..."
cp "$BIN/login.ini" "$BIN/login.ini.bak"
sed -i 's/auto_login=false/auto_login=true/' "$BIN/login.ini"
cp "$BIN/crash.log" /tmp/crash_before.log
rm -f "$BIN/client.log"
( cd "$BIN" && ./Hcat.exe > /dev/null 2>&1 & )
sleep 22

echo "[3] 客户端状态"
tasklist //FI "IMAGENAME eq Hcat.exe" 2>/dev/null | grep -i hcat || echo "    客户端已退出"
if diff -q /tmp/crash_before.log "$BIN/crash.log" >/dev/null; then echo "    crash.log 无新增"; else echo "    ★ 有新增崩溃!"; fi
echo "    登录相关日志:"
grep -E "Token is|Connected to server|Connected to chat|chat login" "$BIN/client.log" | head -5

echo "[4] 上传链路回归（HTTP 层，19MB 应成功 / 21MB 应明确拒绝）"
cd "$ROOT"
/c/Users/19178/.workbuddy/binaries/python/versions/3.13.12/python.exe test/upload_big_test.py 2>&1 | tail -6

echo "[5] 清理"
taskkill //F //IM Hcat.exe 2>/dev/null | head -1
taskkill //F //IM ChatServer.exe 2>/dev/null | head -1
cp "$BIN/login.ini.bak" "$BIN/login.ini"
rm -f "$BIN/login.ini.bak" "$BIN/client.log"
grep -o 'auto_login=[a-z]*' "$BIN/login.ini"
echo done
