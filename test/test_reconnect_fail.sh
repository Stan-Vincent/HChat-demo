#!/bin/bash
# ============================================================
# 重连失败分支验证：kill ChatServer 后【不重启】
#   期望：5 次退避重试 -> sig_reconnect_failed -> 回登录页
#   关键：整个过程客户端不能崩（以前 offlineLogin 后心跳定时器会打崩它）
# ============================================================
set -u
ROOT=/d/QTcode/HChat
BIN=$ROOT/Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug/bin
CHAT=$ROOT/ChatServer

# qCDebug 用的日志分类默认不输出，必须显式打开（见 test_reconnect_e2e.sh）
export QT_LOGGING_RULES='*.debug=true'

echo "[1] 起 ChatServer ..."
( cd "$CHAT" && ./x64/Debug/ChatServer.exe > "$CHAT/fail_e2e_chat.log" 2>&1 & )
sleep 6

echo "[2] auto_login=true ..."
cp "$BIN/login.ini" "$BIN/login.ini.bak"
sed -i 's/auto_login=false/auto_login=true/' "$BIN/login.ini"

echo "[3] 启动客户端 ..."
cp "$BIN/crash.log" /tmp/crash_before.log
rm -f "$BIN/client.log"
( cd "$BIN" && ./Hcat.exe > /dev/null 2>&1 & )
sleep 22

echo "[4] kill ChatServer 且不重启 ..."
taskkill //F //IM ChatServer.exe 2>&1 | head -1

echo "[5] 等 30s（5 次退避共 1+2+4+8+8=23s）..."
sleep 48

echo "[6] 结果采集 ..."
echo "--- 客户端进程 ---"
tasklist //FI "IMAGENAME eq Hcat.exe" 2>/dev/null | grep -i hcat || echo "    客户端已退出"
echo "--- crash.log ---"
if diff -q /tmp/crash_before.log "$BIN/crash.log" >/dev/null; then echo "    无新增崩溃"; else echo "    ★ 有新增崩溃!"; tail -12 "$BIN/crash.log"; fi
echo "--- 重连相关日志 ---"
grep -E "StartReconnect|reconnect attempt|reconnect attempt failed|reconnect failed|reconnect attempts exhausted|back to login" "$BIN/client.log" | tail -20
echo "--- 是否回了登录页（淡入动画）---"
grep -c "setOpacity" "$BIN/client.log" | xargs -I{} echo "    setOpacity 次数: {}"

echo "[7] 清理 ..."
taskkill //F //IM Hcat.exe 2>/dev/null | head -1
taskkill //F //IM ChatServer.exe 2>/dev/null | head -1
cp "$BIN/login.ini.bak" "$BIN/login.ini"
rm -f "$BIN/login.ini.bak"
grep -o 'auto_login=[a-z]*' "$BIN/login.ini"
echo done
