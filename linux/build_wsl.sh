#!/bin/bash
# ============================================================
# HChat Linux/WSL 构建：git clone + GBK->UTF-8 转码 + 移植补丁 + cmake 编译
# 用法（WSL 内）：bash build_wsl.sh
# 说明：
#   ~/HChat      = Windows 仓库的 pristine 克隆（GBK，仅供 git 操作，保持干净）
#   ~/HChat_src  = 一次性 UTF-8 构建树（转码+补丁后，cmake 在这编）
# 前置（一次性环境，已在 WSL 装好）：
#   apt: libmysqlclient-dev libprotobuf-dev libssl-dev zlib1g-dev liblz4-dev libzstd-dev cmake build-essential + 系统 Boost
#   主树 vcpkg: ~/vcpkg_installed（protobuf 6.33.4/grpc/hiredis/jsoncpp/spdlog 静态）
#   连接器:     ~/mysql_conn（libmysqlcppconn.so，手动编的 JDBC 库）
# 内存：VM 仅 ~7G，编 protobuf .pb.cc 用 -j2，避免 OOM。
# ============================================================
set -eu
REPO=/home/xxxl/HCat
SRC=/home/xxxl/HCat_src
WIN_REPO=/mnt/d/QTcode/HChat
LINUX="$REPO/linux"

echo "== 1) 克隆/更新 pristine 仓库到 WSL 原生 FS =="
if [ ! -d "$REPO/.git" ]; then
  git clone "$WIN_REPO" "$REPO"
else
  git -C "$REPO" fetch "$WIN_REPO" 2>/dev/null \
    && git -C "$REPO" reset --hard FETCH_HEAD 2>/dev/null \
    || echo "  (fetch 跳过，用当前副本)"
fi

echo "== 2) 生成一次性 UTF-8 构建树 ~/HChat_src =="
rm -rf "$SRC"; mkdir -p "$SRC"
for d in GateServer ChatServer ChatServer2 StatusServer common; do
  [ -d "$REPO/$d" ] && cp -r "$REPO/$d" "$SRC/" || true
done
cp "$LINUX/CMakeLists.txt" "$SRC/CMakeLists.txt"

echo "== 3) GBK->UTF-8 转码（只转干净 GBK，跳过 UTF-8/腐蚀文件）=="
python3 "$LINUX/transcode_gbk_to_utf8.py" \
  "$SRC/GateServer" "$SRC/ChatServer" "$SRC/StatusServer" "$SRC/common"

echo "== 4) 应用 Linux 移植补丁（幂等）=="
python3 "$LINUX/apply_port.py" \
  "$SRC/GateServer" "$SRC/ChatServer" "$SRC/StatusServer"

echo "== 5) cmake 配置 + 编译（-j2）=="
cmake -S "$SRC" -B "$SRC/build"
cmake --build "$SRC/build" -j2

echo "== 完成：$SRC/build/{GateServer,StatusServer,ChatServer,ChatServer2} =="
echo "   起服务：bash $SRC/../HChat/linux/start_services.sh（或见 start_services.sh）"
