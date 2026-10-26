#!/bin/bash
# 在阿里云 ECS 上执行：配置并启动 VarifyServer（Node.js gRPC 50051）
# 用法（服务器上）：bash /root/server_varify_setup.sh
set -e
cd /opt/hchat/VarifyServer

echo "== 1) 修正 config.json 的 redis 密码（对齐本机 Redis）=="
sed -i 's/"passwd": *"[^"]*"/"passwd": "Hcat@2026Secure"/' config.json
grep -nA4 '"redis"' config.json | head -8

echo "== 2) npm install（生产依赖，纯 JS 无需编译）=="
npm install --omit=dev 2>&1 | tail -6

echo "== 3) systemd unit =="
cat > /etc/systemd/system/hchat-varify.service <<'EOF'
[Unit]
Description=HChat VarifyServer (Node gRPC 50051)
After=network-online.target redis-server.service
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/opt/hchat/VarifyServer
ExecStart=/usr/bin/node /opt/hchat/VarifyServer/server.js
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable --now hchat-varify
sleep 2

echo "== 状态 =="; systemctl is-active hchat-varify
echo "== 端口 =="; ss -tlnp 2>/dev/null | grep 50051 || echo "50051 未监听"
echo "== 日志（应见 grpc server started）=="; journalctl -u hchat-varify --no-pager -n 12 || true
