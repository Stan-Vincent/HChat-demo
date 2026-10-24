#!/usr/bin/env bash
# ============================================================
# HChat 后端 Linux/WSL2 一键环境搭建脚本
# 由 WorkBuddy 生成, 需在 WSL2 (Ubuntu 24.04) 终端内执行。
# 项目位于 Windows D:\QTcode\HChat 时, WSL 路径为 /mnt/d/QTcode/HChat
# 进入后执行:  bash linux/setup_wsl.sh
# ============================================================
set -euo pipefail

echo "==> [1/4] 安装基础构建工具 (apt, 走 Ubuntu 官方源)"
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    cmake ninja-build autoconf libtool pkg-config build-essential \
    git ca-certificates curl

echo "==> [2/4] 网络出口检查 (github 必须可达才能拉 vcpkg 依赖)"
# 若 Windows 侧有代理 (Clash / V2Ray 等), 取消下面两行注释并填入真实网关 IP:
#   WIN_IP=$(grep -m1 nameserver /etc/resolv.conf | awk '{print $2}')
#   export HTTPS_PROXY="http://${WIN_IP}:7890" HTTP_PROXY="http://${WIN_IP}:7890"
# 查看网关 IP 可用:  grep nameserver /etc/resolv.conf
if ! curl -sI --max-time 8 https://github.com >/dev/null 2>&1; then
    echo "!! github 仍不可达。请先在第 [2/4] 步配置代理 (或改用 vcpkg 镜像) 后重跑本脚本。"
    exit 1
fi
echo "    github 连通 OK"

echo "==> [3/4] 克隆并 bootstrap vcpkg"
cd ~
if [ ! -d vcpkg/.git ]; then
    git clone https://github.com/microsoft/vcpkg.git
fi
cd vcpkg
./bootstrap-vcpkg.sh

echo "==> [4/4] 用 manifest 安装后端依赖"
PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cp "$PROJ_ROOT/linux/vcpkg.json" "$PROJ_ROOT/vcpkg.json"
./vcpkg install --triplet=x64-linux

echo "==> 校验 protobuf 版本 (后端代码硬校验 PROTOBUF_VERSION == 6033004, 即 6.33.4)"
PB_LINE=$(./vcpkg list protobuf 2>/dev/null | head -1 || true)
echo "    已安装: ${PB_LINE:-<未知>}"
if echo "$PB_LINE" | grep -q "6.33"; then
    echo "    protobuf 版本匹配 (6.33.x) OK"
else
    echo "!! 警告: protobuf 不是 6.33.x。后端 message.pb.cc 会因 PROTOBUF_VERSION 校验编译失败。"
    echo "!! 处理方案: (a) 在 linux/vcpkg.json 中对 protobuf 加版本约束并重跑;"
    echo "!!           (b) 或将 Windows 侧 vcpkg 的 protobuf 升级/对齐到同一小版本。"
fi
echo "环境搭建完成。下一步: GBK->UTF-8 转码 + 起草 CMakeLists.txt 后构建。"
