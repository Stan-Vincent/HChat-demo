#!/bin/bash
# ============================================================
# 断线自动重连端到端验证
#   1. 起 ChatServer + 客户端（临时 auto_login）
#   2. 等登录完成
#   3. kill ChatServer          -> 模拟服务端掐线
#   4. 3s 后重启 ChatServer     -> 落在重试窗口内，应该被某次重试捞回来
#   5. 收集客户端日志 + crash.log + 进程状态
# 用法: bash test_reconnect_e2e.sh
# ============================================================
set -u
ROOT=/d/QTcode/HChat
BIN=$ROOT/Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug/bin
CHAT=$ROOT/ChatServer
LOG=$BIN/reconnect_e2e.log
export QT_LOGGING_RULES='*.debug=true'

echo "[1] 启动 ChatServer ..."
( cd "$CHAT" && ./x64/Debug/ChatServer.exe > "$CHAT/reconnect_e2e_chat.log" 2>&1 & )
sleep 6
netstat -ano 2>/dev/null | grep -c ":8990 .*LISTENING" | xargs -I{} echo "    8990 listeners: {}"

echo "[2] 备份 login.ini 并开启 auto_login ..."
cp "$BIN/login.ini" "$BIN/login.ini.bak"
sed -i 's/auto_login=false/auto_login=true/' "$BIN/login.ini"
grep -o 'auto_login=[a-z]*' "$BIN/login.ini"

echo "[3] 启动客户端（后台，日志 -> bin/client.log）..."
cp "$BIN/crash.log" /tmp/crash_before.log
rm -f "$BIN/client.log"
( cd "$BIN" && ./Hcat.exe > /dev/null 2>&1 & )
sleep 22
echo "    客户端进程: $(tasklist //FI "IMAGENAME eq Hcat.exe" 2>/dev/null | grep -c Hcat) 个"
echo "    client.log 行数: $(wc -l < "$BIN/client.log" 2>/dev/null || echo 0)"

echo "[4] 杀掉 ChatServer 模拟断线 ..."
taskkill //F //IM ChatServer.exe 2>&1 | head -1
sleep 3

echo "[5] 重启 ChatServer（应被第 2/3 次重试捞回）..."
( cd "$CHAT" && ./x64/Debug/ChatServer.exe >> "$CHAT/reconnect_e2e_chat.log" 2>&1 & )
sleep 18

echo "[6] 结果采集 ..."
echo "--- 客户端进程 ---"
tasklist //FI "IMAGENAME eq Hcat.exe" 2>/dev/null | grep -i hcat || echo "    客户端已退出"
echo "--- crash.log 是否有新增 ---"
if diff -q /tmp/crash_before.log "$BIN/crash.log" >/dev/null; then echo "    无新增崩溃"; else echo "    ★ 有新增崩溃!"; fi
echo "--- 客户端日志中的重连关键行 ---"
grep -nE "reconnect|Disconnected|Connected to server|redo chat login|reconnect login ok|refused|sig_recon" "$BIN/client.log" | tail -40
echo "--- 客户端日志尾部 20 行 ---"
tail -20 "$BIN/client.log"

echo "[7] 清理 ..."
taskkill //F //IM Hcat.exe 2>/dev/null | head -1
taskkill //F //IM ChatServer.exe 2>/dev/null | head -1
cp "$BIN/login.ini.bak" "$BIN/login.ini"
rm -f "$BIN/login.ini.bak"
grep -o 'auto_login=[a-z]*' "$BIN/login.ini"
echo "done"
