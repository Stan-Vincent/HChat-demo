#!/bin/bash
# ============================================================
# HChat Linux/WSL 后端一键启动：MySQL(3308) + Redis(6380) + 4 个 C++ 服务
# 用法（WSL 内）：bash start_services.sh
# 构建树由 build_wsl.sh 生成（~/HChat_src）；各服务从自己目录读 config.ini。
# ============================================================
set -u
ROOT=/home/xxxl/HCat_src
LOG=/home/xxxl/svc_logs
mkdir -p "$LOG"

echo "== 确保 MySQL(3308) / Redis(6380) 在跑 =="
ss -tlnp 2>/dev/null | grep -q ":3308 " || service mysql start >/dev/null 2>&1
ss -tlnp 2>/dev/null | grep -q ":6380 " || redis-server /etc/redis/redis.conf --daemonize yes >/dev/null 2>&1

echo "== 清理旧服务实例 =="
pkill -9 -f "$ROOT/build/(Status|Chat|Gate)Server" 2>/dev/null
sleep 1

echo "== 启动 4 个服务（各从自己的目录读 config.ini）=="
setsid bash -c "cd $ROOT/StatusServer && exec $ROOT/build/StatusServer" </dev/null >"$LOG/status.log" 2>&1 &
setsid bash -c "cd $ROOT/ChatServer  && exec $ROOT/build/ChatServer"  </dev/null >"$LOG/chat1.log" 2>&1 &
setsid bash -c "cd $ROOT/ChatServer2 && exec $ROOT/build/ChatServer2" </dev/null >"$LOG/chat2.log" 2>&1 &
setsid bash -c "cd $ROOT/GateServer  && exec $ROOT/build/GateServer"  </dev/null >"$LOG/gate.log"  2>&1 &

sleep 7
echo "== 监听端口 =="
ss -tlnp 2>/dev/null | grep -oE ":50052|:8990|:8991|:8080|:3308|:6380" | sort -u
echo "== GateServer /health（MySQL+Redis 活检）=="
curl -s --max-time 10 http://127.0.0.1:8080/health
echo
echo "== 服务日志目录：$LOG =="
