#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GBK -> UTF-8 安全转码（HChat Linux 构建用）
-------------------------------------------
背景：Windows 仓库后端源码是 GBK（MSVC 按 936 解析）；Linux/g++ 需要 UTF-8。
本脚本把构建树里的后端源码批量转成 UTF-8，但**足够保守，绝不二次破坏**：

  * 已含 U+FFFD 的文件（历史遗留腐蚀）-> 原样跳过，不碰；
  * 能按 UTF-8 解码的文件（本就是 UTF-8）-> 原样跳过；
  * 仅当 UTF-8 解码失败时，才按 GBK 解码并改写为 UTF-8。

用法：
  python3 transcode_gbk_to_utf8.py <目录> [<目录> ...]
  python3 transcode_gbk_to_utf8.py GateServer ChatServer StatusServer common
"""
import sys, os

FFFD = b"\xef\xbf\xbd"          # U+FFFD 的 UTF-8 字节
EXTS = (".cpp", ".h", ".cc", ".hpp", ".cxx")

def transcode_file(path):
    with open(path, "rb") as f:
        raw = f.read()
    if FFFD in raw:                       # 已腐蚀 -> 不碰
        return "corrupt-skip"
    try:
        raw.decode("utf-8")               # 已是 UTF-8 -> 不碰
        return "utf8-skip"
    except UnicodeDecodeError:
        pass
    try:
        text = raw.decode("gbk")          # 干净 GBK -> 转 UTF-8
    except UnicodeDecodeError:
        return "undecodable-skip"         # 连 GBK 都解不了 -> 不碰
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))
    return "converted"

def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    stats = {"converted":0, "utf8-skip":0, "corrupt-skip":0, "undecodable-skip":0}
    for root in sys.argv[1:]:
        for dirpath, _dirs, files in os.walk(root):
            for name in files:
                if name.lower().endswith(EXTS):
                    r = transcode_file(os.path.join(dirpath, name))
                    stats[r] += 1
    print("转码完成：转换(GBK->UTF-8)={converted}  已是UTF8跳过={utf8-skip}  "
          "腐蚀跳过={corrupt-skip}  无法解码跳过={undecodable-skip}".format(**stats))

if __name__ == "__main__":
    main()
