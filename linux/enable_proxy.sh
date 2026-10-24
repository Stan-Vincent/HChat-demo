#!/usr/bin/env bash
# ============================================================
# HChat WSL 代理一键启用（方案1：Windows 侧 Clash/V2Ray 导出）
# 用法（务必 source，让环境变量留在当前 shell）:
#   source linux/enable_proxy.sh 7897        # 端口按你的代理软件改
# 脚本会自动: 取 Windows 网关IP -> 试 127.0.0.1/网关 -> 设代理+git -> 验证 github -> 持久化 -> 继续安装
# ============================================================
PORT="${1:-7897}"

# Windows 在 WSL 网络里的网关 IP（拿不到再退回 resolv.conf 的 nameserver）
WIN_HOST=$(ip route 2>/dev/null | awk '/default/ {print $3; exit}')
if [ -z "$WIN_HOST" ]; then
  WIN_HOST=$(grep -m1 nameserver /etc/resolv.conf | awk '{print $2}')
fi
echo "Windows 网关 = ${WIN_HOST:-<未知>}, 代理端口 = $PORT"

# 先试 127.0.0.1（新版 WSL2 + Clash 监听 0.0.0.0 时可用），失败回退网关 IP
# 用 HTTP 状态码判断（2xx/3xx 即视为可达），超时放宽到 15s 避免节点冷启动误判
PROXY_HOST=""
for cand in "127.0.0.1" "$WIN_HOST"; do
  [ -z "$cand" ] && continue
  code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 15 --proxy "http://${cand}:${PORT}" https://github.com 2>/dev/null)
  if [ "$code" = "200" ] || [ "$code" = "301" ] || [ "$code" = "302" ]; then
    PROXY_HOST="$cand"
    break
  fi
done

if [ -z "$PROXY_HOST" ]; then
  echo "!! 127.0.0.1 与网关都连不通代理。请确认:"
  echo "   1) Windows 代理软件已开 '允许局域网' (allow-lan: true) 且监听 0.0.0.0"
  echo "   2) 端口正确（Clash 的 mixed-port / V2Ray 的 HTTP 入站端口）"
  echo "   3) Windows 防火墙放行该端口（或允许 WSL 子网入站）"
  return 1 2>/dev/null || exit 1
fi

export HTTP_PROXY="http://${PROXY_HOST}:${PORT}"
export HTTPS_PROXY="http://${PROXY_HOST}:${PORT}"
export ALL_PROXY="http://${PROXY_HOST}:${PORT}"
export http_proxy="$HTTP_PROXY"
export https_proxy="$HTTPS_PROXY"

git config --global http.proxy "$HTTP_PROXY"
git config --global https.proxy "$HTTPS_PROXY"

echo "代理出口 = $PROXY_HOST:$PORT"
code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 15 https://github.com 2>/dev/null)
if [ "$code" = "200" ] || [ "$code" = "301" ] || [ "$code" = "302" ]; then
  echo "=== github 连通 OK (HTTP $code) ==="
else
  echo "!! github 仍不通（代理设上了但出口异常，HTTP ${code:-无响应}），请检查代理节点/规则"
  return 1 2>/dev/null || exit 1
fi

# 持久化：写入专用文件并 source 进 ~/.bashrc（避免重复追加）
cat > ~/.hchat_proxy.sh <<EOF
export HTTP_PROXY="http://${PROXY_HOST}:${PORT}"
export HTTPS_PROXY="http://${PROXY_HOST}:${PORT}"
export ALL_PROXY="http://${PROXY_HOST}:${PORT}"
export http_proxy="\$HTTP_PROXY"
export https_proxy="\$HTTPS_PROXY"
EOF
if ! grep -q 'source ~/.hchat_proxy.sh' ~/.bashrc 2>/dev/null; then
  echo 'source ~/.hchat_proxy.sh' >> ~/.bashrc
  echo "已加入 ~/.bashrc（新终端自动加载代理）"
fi

echo "继续安装 vcpkg 依赖..."
bash "$(dirname "$0")/setup_wsl.sh"
