#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HChat Linux 移植补丁（在转码后的 UTF-8 构建树上执行，幂等）
----------------------------------------------------------
把 MSVC 容忍、libstdc++ 报错的 4 处差异统一修掉。全部只碰 ASCII 代码行，
不触碰中文注释/字符串（避免编码风险）；重复执行无副作用。

  [1] GateServer/const.h        删除死 include <sw/redis++/redis++.h>（0 处使用）
  [2] 各服务 *.cpp/*.h          补 <condition_variable>/<mutex>/<shared_mutex>
                                （用了 std::condition_variable 等却没包对应头）
  [3] GateServer/LogicSystem.cpp ofstream/ifstream(wstring) 包 #ifdef _WIN32
                                （libstdc++ 无 wstring 构造，Linux 走窄路径）

用法：python3 apply_port.py GateServer ChatServer StatusServer
"""
import sys, os, re

EXTS = (".cpp", ".h", ".cc")

def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8")

def write(path, text):
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))

def eol_of(text):
    return "\r\n" if "\r\n" in text else "\n"

def edit1_const_h(root):
    """删 GateServer/const.h 里的死 include <sw/redis++/redis++.h>"""
    p = os.path.join(root, "GateServer", "const.h")
    if not os.path.exists(p):
        return
    text = read(p); eol = eol_of(text)
    lines = text.split(eol)
    new = [ln for ln in lines if "sw/redis++/redis++.h" not in ln]
    if len(new) != len(lines):
        write(p, eol.join(new)); print("  [1] const.h: 删除死 include sw/redis++")

def edit2_std_headers(root):
    """给用了 std::condition_variable/mutex/shared_mutex 却没包头的文件补头"""
    need = {"<condition_variable>": "std::condition_variable",
            "<mutex>": "std::mutex",
            "<shared_mutex>": "std::shared_mutex"}
    for svc in ("GateServer", "ChatServer", "StatusServer"):
        base = os.path.join(root, svc)
        for dp, _d, files in os.walk(base):
            for name in files:
                if not name.lower().endswith(EXTS):
                    continue
                p = os.path.join(dp, name)
                text = read(p)
                missing = [h for h, tok in need.items()
                           if tok in text and ("#include " + h) not in text]
                if not missing:
                    continue
                eol = eol_of(text)
                ins = "".join("#include " + h + eol for h in missing)
                if "#pragma once" in text:
                    text = text.replace("#pragma once" + eol,
                                        "#pragma once" + eol + ins, 1)
                else:
                    m = re.search(r'^#include[^\n]*\n', text, re.M)
                    if m:
                        text = text[:m.end()] + ins + text[m.end():]
                write(p, text)
                print("  [2] %s: 补 %s" % (os.path.join(svc, name), " ".join(missing)))

def edit3_wstring_streams(root):
    """GateServer/LogicSystem.cpp: ofstream/ifstream(wstring) 包 #ifdef _WIN32"""
    p = os.path.join(root, "GateServer", "LogicSystem.cpp")
    if not os.path.exists(p):
        return
    text = read(p); eol = eol_of(text)
    if "_WIN32" in text and "abs_path.string()" in text:
        print("  [3] LogicSystem.cpp: 已是 #ifdef 形态，跳过"); return
    lines = text.split(eol)
    out = []; i = 0; n = len(lines); changed = False
    while i < n:
        ln = lines[i]
        # ofstream(wstring) 占两行
        if ("std::ofstream ofs(boost::filesystem::path(con->_upload_path).wstring()," in ln
                and i + 1 < n and "std::ios::binary | std::ios::trunc);" in lines[i+1]):
            out += ["#ifdef _WIN32", ln, lines[i+1], "#else",
                    "    std::ofstream ofs(con->_upload_path, std::ios::binary | std::ios::trunc);",
                    "#endif"]
            i += 2; changed = True; continue
        # ifstream(wstring) 单行
        if "std::ifstream ifs(abs_path.wstring(), std::ios::binary);" in ln:
            indent = ln[:len(ln) - len(ln.lstrip())]
            out += ["#ifdef _WIN32", ln, "#else",
                    indent + "std::ifstream ifs(abs_path.string(), std::ios::binary);",
                    "#endif"]
            i += 1; changed = True; continue
        out.append(ln); i += 1
    if changed:
        write(p, eol.join(out)); print("  [3] LogicSystem.cpp: wstring 流包 #ifdef _WIN32")

def main():
    roots = sys.argv[1:] or ["."]
    for root in roots:
        print("补丁目录：%s" % root)
        edit1_const_h(root)
        edit2_std_headers(root)
        edit3_wstring_streams(root)
    print("移植补丁完成")

if __name__ == "__main__":
    main()
