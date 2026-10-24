# -*- coding: utf-8 -*-
"""HChat -> Hcat 项目改名。
范围（只动源码/构建配置/文档/启动脚本，全部 ASCII 级替换，与文件编码无关）：
  1. 代码标识符：hchat::log 命名空间、hchat.* 日志分类名、HChatClient UA、DPAPI entropy
  2. 显示名：窗口标题、README/NextVersion/architecture.svg、start/stop.bat
  3. 构建文件名：Client/Hchat.pro -> Hcat.pro（exe 随之变 Hcat.exe）
不改：目录名 D:\\QTcode\\HChat、test/ 工具脚本、.workbuddy/memory 历史日志、.vs 缓存。
"""
import os
import shutil

RULES = [
    (b'HChat', b'Hcat'),
    (b'Hchat', b'Hcat'),
    (b'hchat::', b'hcat::'),
    (b'namespace hchat', b'namespace hcat'),
    (b'"hchat.', b'"hcat.'),
    (b'"hchat"', b'"hcat"'),
    (b'hchat.', b'hcat.'),
]

FILES = [
    'common/logger.h',
    'GateServer/GateServer.cpp',
    'ChatServer/ChatServer.cpp',
    'ChatServer2/ChatServer.cpp',
    'StatusServer/StatusServer.cpp',
    'Client/logcategory.h',
    'Client/logcategory.cpp',
    'Client/main.cpp',
    'Client/logindialog.cpp',
    'Client/imagemanager.cpp',
    'Client/mainwindow.ui',
    'Client/Hchat.pro',
    'Start/start.bat',
    'Start/stop.bat',
    'README.md',
    'NextVersion.md',
    'docs/architecture.svg',
]

total = 0
for f in FILES:
    if not os.path.exists(f):
        print('SKIP (missing):', f)
        continue
    b = open(f, 'rb').read()
    orig = b
    n = 0
    for old, new in RULES:
        c = b.count(old)
        if c:
            n += c
            b = b.replace(old, new)
    if b != orig:
        open(f, 'wb').write(b)
        total += n
        print('%-32s %3d 处' % (f, n))

# pro 文件重命名（放在替换之后，替换列表里用的是旧名）
if os.path.exists('Client/Hcat.pro') and os.path.exists('Client/Hchat.pro'):
    os.remove('Client/Hcat.pro')
shutil.move('Client/Hchat.pro', 'Client/Hcat.pro')
print('renamed: Client/Hchat.pro -> Client/Hcat.pro')
print('total replacements:', total)
