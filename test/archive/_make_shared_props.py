# -*- coding: utf-8 -*-
"""抽取 ChatServer / ChatServer2 共享的源码清单到 SharedSources.props，
两个 vcxproj 改为 Import 它（对称写法），从此源码只此一份。

改前：两台各存一份 38 个源文件的副本，改一处必须手工同步两台。
改后：ChatServer2 不持有源码，vcxproj 通过 ..\\ChatServer\\SharedSources.props 引用同一份。
"""
import os
import re

VCX = 'ChatServer/ChatServer.vcxproj'
TARGETS = ['ChatServer/ChatServer.vcxproj', 'ChatServer2/ChatServer.vcxproj']
PROPS = 'ChatServer/SharedSources.props'
MARKER = 'SharedSources.props'

for p in TARGETS:
    t = open(p, encoding='utf-8-sig').read()
    if MARKER in t:
        print('SKIP: 已改过', p)
        continue
    break
else:
    print('SKIP: 两个工程都已引用 SharedSources.props')
    raise SystemExit(0)

text = open(VCX, encoding='utf-8-sig').read()
eol = '\r\n' if '\r\n' in text else '\n'
print('vcxproj 换行:', 'CRLF' if eol == '\r\n' else 'LF')

def grab(tag):
    return re.findall(r'<Cl%s Include="([^"]*)"' % tag, text)

cpps = grab('Compile')
hpps = grab('Include')
print('抽取 ClCompile=%d ClInclude=%d' % (len(cpps), len(hpps)))

# 1) 生成 props
def items(tag, names):
    return ''.join('    <Cl%s Include="$(MSBuildThisFileDirectory)%s" />%s' % (tag, n, eol)
                   for n in names)

props = (
    '<?xml version="1.0" encoding="utf-8"?>%s'
    '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">%s'
    '  <!-- ChatServer 与 ChatServer2 共享的源码清单（唯一一份，改一处两台同时生效）。%s'
    '       路径基于本文件所在目录，所以两个工程可以用完全相同的一行 Import 引用它。%s'
    '       新增源文件：只改这里（不要把文件再复制一份到 ChatServer2）。 -->%s'
    '  <ItemGroup>%s%s  </ItemGroup>%s'
    '  <ItemGroup>%s%s  </ItemGroup>%s'
    '</Project>%s'
) % (eol, eol, eol, eol, eol, eol, items('Compile', cpps), eol,
     eol, items('Include', hpps), eol, eol)
open(PROPS, 'w', encoding='utf-8-sig', newline='').write(props)
print('已生成', PROPS)

# 2) 两个 vcxproj：删掉本地源码 ItemGroup，换成一行 Import
pat = re.compile(r'[ \t]*<ItemGroup>\s*(?:[ \t]*<Cl(?:Compile|Include) Include="[^"]*" />\s*)+'
                 r'[ \t]*</ItemGroup>\s*')
import_line = ('  <Import Project="$(MSBuildThisFileDirectory)..\\ChatServer\\SharedSources.props" />%s'
               % eol)

for p in TARGETS:
    t = open(p, encoding='utf-8-sig').read()
    if MARKER in t:
        print('  跳过（已改）', p); continue
    ms = list(pat.finditer(t))
    if len(ms) != 2:
        print('  FAIL: %s 期望 2 个源码 ItemGroup，实际 %d' % (p, len(ms))); raise SystemExit(1)
    new = t[:ms[0].start()] + import_line + pat.sub('', t[ms[0].start():])
    # 自检：源码条目清零，且总行数只减少
    assert '<ClCompile Include=' not in new and '<ClInclude Include=' not in new, p
    assert new.count('<ItemGroup>') == t.count('<ItemGroup>') - 2, p
    open(p, 'w', encoding='utf-8-sig', newline='').write(new)
    print('  已改', p)

# 3) 自检：props 里每个文件都真实存在
missing = []
for n in cpps + hpps:
    if not os.path.isfile(os.path.join('ChatServer', n)):
        missing.append(n)
print('props 引用文件缺失:', missing if missing else '无')
assert not missing
print('OK')
