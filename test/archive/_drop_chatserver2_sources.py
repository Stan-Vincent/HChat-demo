# -*- coding: utf-8 -*-
"""1) message.proto 也收进共享清单；2) 删除 ChatServer2 的源码副本；3) 修 filters 路径。"""
import os
import re
import subprocess

PROPS = 'ChatServer/SharedSources.props'
VCXS = ['ChatServer/ChatServer.vcxproj', 'ChatServer2/ChatServer.vcxproj']
FILTERS = ['ChatServer/ChatServer.vcxproj.filters', 'ChatServer2/ChatServer.vcxproj.filters']
PREFIX = r'..\ChatServer\\'

# ---- 1) props 里加 message.proto ----
props = open(PROPS, encoding='utf-8-sig').read()
if 'message.proto' not in props:
    props = props.replace('</Project>',
                          '  <ItemGroup>\r\n' if '\r\n' in props else '  <ItemGroup>\n')
    # 上面写法不稳，改为按 eol 精确插入
    props = open(PROPS, encoding='utf-8-sig').read()
    eol = '\r\n' if '\r\n' in props else '\n'
    block = ('  <ItemGroup>%s'
             '    <None Include="$(MSBuildThisFileDirectory)message.proto" />%s'
             '  </ItemGroup>%s') % (eol, eol, eol)
    props = props.replace('</Project>', block + '</Project>')
    open(PROPS, 'w', encoding='utf-8-sig', newline='').write(props)
    print('props: 已加入 message.proto')
else:
    print('props: message.proto 已存在')

# ---- 2) vcxproj 里去掉本地 message.proto 的 None 项（config.ini 各自保留）----
for p in VCXS:
    t = open(p, encoding='utf-8-sig').read()
    if '<None Include="message.proto" />' not in t:
        print('  ', p, '已无 message.proto 项'); continue
    new = t.replace('    <None Include="message.proto" />\n', '')
    new = new.replace('    <None Include="message.proto" />\r\n', '')
    assert '<None Include="config.ini" />' in new, 'config.ini 项丢了'
    open(p, 'w', encoding='utf-8-sig', newline='').write(new)
    print('  ', p, '已移除本地 message.proto 项')

# ---- 3) 删除 ChatServer2 的源码副本 ----
names = re.findall(r'Include="\$\(MSBuildThisFileDirectory\)([^"]+)"', props)
print('共享清单文件数:', len(names))
to_del = [os.path.join('ChatServer2', n) for n in names if os.path.isfile(os.path.join('ChatServer2', n))]
print('ChatServer2 待删:', len(to_del))
if to_del:
    subprocess.run(['git', 'rm', '-q', '--'] + to_del, check=True)
    print('已 git rm', len(to_del), '个文件')

# ---- 4) filters 里源文件路径加 ..\ChatServer\ 前缀 ----
for p in FILTERS:
    if not os.path.isfile(p):
        print('  无 filters:', p); continue
    t = open(p, encoding='utf-8-sig').read()
    if PREFIX in t:
        print('  filters 已改:', p); continue
    new = re.sub(r'<(ClCompile|ClInclude|None) Include="(?!\.\.\\)([^"]+)"',
                 lambda m: '<%s Include="%s%s"' % (m.group(1), PREFIX, m.group(2)), t)
    open(p, 'w', encoding='utf-8-sig', newline='').write(new)
    print('  filters 已加前缀:', p)

print('OK')
