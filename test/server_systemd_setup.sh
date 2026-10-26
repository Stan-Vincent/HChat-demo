#!/bin/bash
# 在阿里云 ECS 上执行：生成 4 个 systemd unit 并启动 C++ 后端服务
# 用法（服务器上）：bash /root/server_systemd_setup.sh
set -e

gen() {
  local name=$1 dir=$2 bin=$3 desc=$4
  cat > /etc/systemd/system/hchat-$name.service <<EOF
[Unit]
Description=$desc
After=network-online.target mysql.service redis-server.service
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/opt/hchat/$dir
Environment=LD_LIBRARY_PATH=/opt/hchat/lib
ExecStart=/opt/hchat/$dir/$bin
Restart=always
RestartSec=3
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
EOF
}

gen status StatusServer StatusServer "HChat StatusServer (gRPC 50052)"
gen chat1  ChatServer   ChatServer   "HChat ChatServer1 (TCP 8990)"
gen chat2  ChatServer2  ChatServer2  "HChat ChatServer2 (TCP 8991)"
gen gate   GateServer   GateServer   "HChat GateServer (HTTP 8080)"

systemctl daemon-reload
echo "== 启动 status/chat1/chat2 =="
systemctl enable --now hchat-status hchat-chat1 hchat-chat2
sleep 3
echo "== 启动 gate（依赖 status/varify）=="
systemctl enable --now hchat-gate
sleep 2

echo "== 各服务状态 =="
for s in status chat1 chat2 gate; do
  printf "hchat-%-7s : " "$s"
  systemctl is-active hchat-$s 2>/dev/null || true
done

echo "== 监听端口（应见 8080/8990/8991/50052）=="
ss -tlnp 2>/dev/null | grep -E ":8080|:8990|:8991|:50052" || echo "  未见预期端口"

echo "== GateServer /health =="
curl -s --max-time 8 http://127.0.0.1:8080/health || echo "  /health 无响应"
echo
