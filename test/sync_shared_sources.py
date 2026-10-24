#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 ChatServer/SharedSources.props（唯一真源）里的源码清单展开进
ChatServer / ChatServer2 两个 vcxproj，并同步两份 .vcxproj.filters。

为什么不用 <Import>：VS 的解决方案资源管理器对"通过 Import 引进来的 item"
套用 .filters 分组时不可靠（实测分组出现但文件不进组）。改成 item 在工程文件里
本地声明后，filters 按 Include 字符串匹配就能稳定生效。

用法：
    python test/sync_shared_sources.py          # 同步（幂等）
    python test/sync_shared_sources.py --check  # 只检查是否一致，不同步

改动源码清单时：只改 ChatServer/SharedSources.props，然后跑一次本脚本。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROPS = os.path.join(ROOT, 'ChatServer', 'SharedSources.props')
VCXS = [os.path.join(ROOT, 'ChatServer', 'ChatServer.vcxproj'),
        os.path.join(ROOT, 'ChatServer2', 'ChatServer.vcxproj')]
FILTERS = [os.path.join(ROOT, 'ChatServer', 'ChatServer.vcxproj.filters'),
           os.path.join(ROOT, 'ChatServer2', 'ChatServer.vcxproj.filters')]

BEGIN = '  <!-- BEGIN SharedSources (由 ChatServer\\SharedSources.props 生成，勿手改) -->'
END = '  <!-- END SharedSources -->'
IMPORT_RE = re.compile(r'[ \t]*<Import Project="\$\(MSBuildThisFileDirectory\)'
                       r'\.\.\\ChatServer\\SharedSources\.props" />[ \t]*\r?\n')


def read(p):
    return open(p, encoding='utf-8-sig').read()


def write(p, s):
    open(p, 'w', encoding='utf-8-sig', newline='').write(s)


def grab():
    t = read(PROPS)
    eol = '\r\n' if '\r\n' in t else '\n'
    groups = {}
    for m in re.finditer(r'<ItemGroup>(.*?)</ItemGroup>', t, re.S):
        for tag, inc in re.findall(r'<(ClCompile|ClInclude|None) Include="[^"]*?([^"\\/]+)"', m.group(1)):
            groups.setdefault(tag, []).append(inc)
    return groups, eol


def build_block(groups, eol):
    lines = [BEGIN]
    for tag in ('ClCompile', 'ClInclude', 'None'):
        if tag not in groups:
            continue
        lines.append('  <ItemGroup>')
        for n in groups[tag]:
            lines.append('    <Cl%s Include="..\\ChatServer\\%s" />' % (tag[2:], n))
        lines.append('  </ItemGroup>')
    lines.append(END)
    return eol.join(lines) + eol


def main():
    check = '--check' in sys.argv
    groups, eol = grab()
    block = build_block(groups, eol)
    ok = True

    for p in VCXS:
        t = read(p)
        if BEGIN in t and END in t:
            new = re.sub(re.escape(BEGIN) + r'.*?' + re.escape(END),
                         lambda m: block.rstrip('\r\n'), t, flags=re.S)
        elif IMPORT_RE.search(t):
            new = IMPORT_RE.sub(lambda m: block, t, count=1)
        else:
            print('FAIL 找不到 SharedSources 区块或 Import 行:', p)
            ok = False
            continue
        if new != t:
            if check:
                print('DIFF 需要同步:', p); ok = False
            else:
                write(p, new)
                print('已同步:', p)
        else:
            print('无变化:', p)

    # 顺带确认 filters 的 Include 与清单一致（字符串级）
    want = set()
    for tag in groups:
        for n in groups[tag]:
            want.add('..\\ChatServer\\' + n)
    for f in FILTERS:
        t = read(f)
        have = set(re.findall(r'<(?:ClCompile|ClInclude|None) Include="([^"]+)"', t))
        miss = want - have
        if miss:
            print('WARN filters 缺少条目 %s: %s' % (f, sorted(miss)[:3]))
            ok = False
        else:
            print('filters 对齐 OK:', f)

    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
