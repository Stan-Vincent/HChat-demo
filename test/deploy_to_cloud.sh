#!/bin/bash
# ============================================================
# HChat 后端上云部署：WSL 暂存 -> rsync 到阿里云 ECS -> 服务器端依赖自检
# 前置：WSL 已 bash linux/build_wsl.sh 出 4 个二进制；HCAT_SSH_KEY 指向的私钥可用
# 用法（WSL 内）：bash test/deploy_to_cloud.sh
# ============================================================
set -e
SRC="${HCAT_SRC:-/home/xxxl/HCat_src}"
ST=/tmp/hchat_deploy
# 敏感信息从环境变量读取，避免硬编码真实服务器 IP / 私钥路径
SERVER_IP="${HCAT_SERVER_IP:?请先 export HCAT_SERVER_IP=服务器公网IP}"
SSH_KEY="${HCAT_SSH_KEY:-$HOME/.ssh/hchat_aliyun}"
SERVER="root@${SERVER_IP}"

echo "== 1) 暂存到 $ST（二进制+各自 config.ini+连接器 so）=="
rm -rf "$ST"; mkdir -p "$ST"/{GateServer,StatusServer,ChatServer,ChatServer2,lib}
cp "$SRC/build/GateServer"   "$ST/GateServer/"
cp "$SRC/build/StatusServer" "$ST/StatusServer/"
cp "$SRC/build/ChatServer"   "$ST/ChatServer/"
cp "$SRC/build/ChatServer2"  "$ST/ChatServer2/"
cp "$SRC/GateServer/config.ini"   "$ST/GateServer/"
cp "$SRC/StatusServer/config.ini" "$ST/StatusServer/"
cp "$SRC/ChatServer/config.ini"   "$ST/ChatServer/"
cp "$SRC/ChatServer2/config.ini"  "$ST/ChatServer2/"
# StatusServer 下发给客户端的 chatserver 地址改为公网 IP（仅 [chatserver1]/[chatserver2] 段，
# 绝不能动 [Mysql]/[Redis] 的 127.0.0.1，否则服务连不上本机库/缓存）
awk -v ip="$SERVER_IP" '
  /^\[/ { sec=$0 }
  /^Host = 127\.0\.0\.1/ && (sec=="[chatserver1]" || sec=="[chatserver2]") { print "Host = " ip; next }
  { print }
' "$ST/StatusServer/config.ini" > "$ST/StatusServer/config.ini.tmp" \
  && mv "$ST/StatusServer/config.ini.tmp" "$ST/StatusServer/config.ini"
cp /home/xxxl/mysql_conn/lib/libmysqlcppconn.so* "$ST/lib/"

echo "--- StatusServer chatserver 段（应=${SERVER_IP}）---"
grep -nE '\[chatserver|Host|Port' "$ST/StatusServer/config.ini"

echo "== 2) 暂存清单 =="
find "$ST" -maxdepth 2 -type f | sort

echo "== 3) rsync 上云 -> $SERVER:/opt/hchat/ =="
rsync -avz -e "ssh -i $SSH_KEY -o StrictHostKeyChecking=accept-new" "$ST/" "$SERVER:/opt/hchat/"

echo "== 4) 服务器端依赖自检（LD_LIBRARY_PATH=/opt/hchat/lib）=="
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=accept-new "$SERVER" \
  'for b in GateServer StatusServer ChatServer ChatServer2; do
     printf "[%s] " "$b"
     LD_LIBRARY_PATH=/opt/hchat/lib ldd /opt/hchat/$b/$b 2>&1 | grep -i "not found" || echo OK
   done
   echo "--- /opt/hchat 结构 ---"
   find /opt/hchat -maxdepth 2 -type f | sort'

echo "== 部署完成 =="
