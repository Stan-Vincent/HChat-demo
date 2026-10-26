#!/bin/bash
# HChat 客户端 Qt 运行时部署脚本
# 用途：把官方 Qt 6.5.3 运行时复制到两个构建输出目录，使 exe 不依赖 PATH 找 DLL。
# 背景（2026-10-26 恶性bug）：bin/ 无本地运行时，exe 按 PATH 加载 DLL；
#   本机存在 官方6.5.3 / MSYS2 Qt 6.11 / anaconda Qt6 三个提供者，
#   PATH 顺序不同会加载错 Qt → qsvg 插件失效 → 全部 SVG 图标空白。
# 何时需要重跑：重装系统/换机/升级 Qt 版本/整个删掉 build 目录之后。
# 用法：bash test/deploy_qt_runtime.sh

set -e
QT=${QT_DIR:-/d/QT6/6.5.3/mingw_64}
REPO=$(cd "$(dirname "$0")/.." && pwd)

DLLS_BIN="Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll Qt6Svg.dll \
libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll"
PLAT="platforms:qwindows.dll"
IMGFMT="imageformats:qsvg.dll qjpeg.dll qgif.dll qico.dll"
ICONENG="iconengines:qsvgicon.dll"
STYLES="styles:qwindowsvistastyle.dll"

deploy_one() {
    local dest="$1"
    mkdir -p "$dest"
    for f in $DLLS_BIN; do cp "$QT/bin/$f" "$dest/"; done
    for grp in "$PLAT" "$IMGFMT" "$ICONENG" "$STYLES"; do
        local dir="${grp%%:*}"; local files="${grp#*:}"
        mkdir -p "$dest/$dir"
        for f in $files; do cp "$QT/plugins/$dir/$f" "$dest/$dir/"; done
    done
    echo "[OK] $dest"
}

# 1) in-source release 构建（命令行 qmake/mingw32-make 产物）
deploy_one "$REPO/Client/bin"
# 2) Qt Creator shadow build（IDE 运行键实际启动的 exe）
for d in "$REPO"/Client/build/Desktop_*/bin; do
    [ -d "$d" ] && deploy_one "$d"
done
echo "部署完成。验证：恶劣PATH下运行 Hcat.exe，图标应全部显示。"
