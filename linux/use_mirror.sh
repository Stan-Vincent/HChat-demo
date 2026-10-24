#!/usr/bin/env bash
# ============================================================
# HChat 后端 Linux 安装 - 方案2：纯国内镜像（无需国外代理）
# 适用：github 直连/经代理均 000，但 gitclone.com 与 mirrors.tuna.tsinghua.edu.cn 可达
# 用法：bash linux/use_mirror.sh
# ============================================================
set -euo pipefail

# 清掉可能残留的 Windows 代理（避免 github 经死代理返回 000）
unset HTTP_PROXY HTTPS_PROXY ALL_PROXY http_proxy https_proxy

echo "==> [1/3] git 走 gitclone.com 镜像"
git config --global url."https://gitclone.com/github.com/".insteadOf "https://github.com/"

echo "==> [2/3] 克隆 vcpkg 本体（清华 git 镜像，含完整 ports 注册表）"
cd ~
if [ ! -d vcpkg/.git ]; then
  git clone https://mirrors.tuna.tsinghua.edu.cn/git/vcpkg.git vcpkg
fi
cd vcpkg

echo "==> [3/3] bootstrap vcpkg（资源走清华镜像，应能取到预编译二进制）"
export X_VCPKG_ASSET_SOURCES="x-azurl,https://mirrors.tuna.tsinghua.edu.cn/vcpkg"
export VCPKG_DOWNLOAD_MIRROR="https://mirrors.tuna.tsinghua.edu.cn/vcpkg"
./bootstrap-vcpkg.sh

echo "=== vcpkg 就绪: $(pwd)/vcpkg ==="
echo "后续：GBK->UTF-8 转码 + 起草 CMakeLists.txt 后构建。"
echo "若 bootstrap 报 github 相关下载失败，把整段输出贴回，我加源码编译兜底。"
