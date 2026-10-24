# -*- coding: utf-8 -*-
"""把后端三个服务的 .vcxproj.filters 从「源文件/头文件」按扩展名分组，
改成按职责分组（入口 / 网络层 / 业务逻辑 / gRPC / 数据访问 / 会话与状态 /
配置与公共 / 数据模型 / 协议）。

只影响 VS 解决方案资源管理器的显示，**不移动文件、不参与编译**，零风险。
"""
import os
import re
import uuid

NS = uuid.UUID('12345678-1234-5678-1234-567812345678')

ORDER = ['入口', '网络层', '业务逻辑', 'gRPC', '数据访问',
         '会话与状态', '配置与公共', '数据模型', '协议(protobuf)', '其他']

NET = {'AsioIOServicePool', 'CServer', 'CSession', 'MsgNode', 'HttpConnection'}
DATA = {'MysqlDao', 'MysqlMgr', 'RedisMgr', 'DistLock'}
CONF = {'ConfigMgr', 'utils', 'Singleton', 'const', 'config'}


ENTRIES = {'GateServer', 'ChatServer', 'StatusServer'}  # 各服务 main 所在文件


def classify(name):
    base = os.path.splitext(os.path.basename(name))[0]
    ext = os.path.splitext(name)[1].lower()
    if base.startswith('message'):
        return '协议(protobuf)'
    # 注意：CServer 也以 Server 结尾，所以先判网络层，入口用白名单
    if base in NET:
        return '网络层'
    if ext == '.cpp' and base in ENTRIES:
        return '入口'
    if base == 'LogicSystem':
        return '业务逻辑'
    if base.endswith('ServiceImpl') or base.endswith('GrpcClient'):
        return 'gRPC'
    if base in DATA:
        return '数据访问'
    if base == 'UserMgr':
        return '会话与状态'
    if ext == '.props':
        return '配置与公共'
    if base in CONF:
        return '配置与公共'
    if base == 'data':
        return '数据模型'
    return '其他'


TARGETS = ['ChatServer/ChatServer.vcxproj.filters',
           'ChatServer2/ChatServer.vcxproj.filters',
           'GateServer/GateServer.vcxproj.filters',
           'StatusServer/StatusServer.vcxproj.filters']

for p in TARGETS:
    if not os.path.isfile(p):
        print('跳过（不存在）', p); continue
    t = open(p, encoding='utf-8-sig').read()
    # 说明：总是重生成（从当前文件重新提取条目，不会丢文件），所以不做幂等跳过
    eol = '\r\n' if '\r\n' in t else '\n'

    # 抽出所有条目：标签名 + Include
    entries = re.findall(r'<(ClCompile|ClInclude|None) Include="([^"]+)"', t)
    if not entries:
        print('跳过（无条目）', p); continue

    used = {}
    for tag, inc in entries:
        used.setdefault(classify(inc), []).append((tag, inc))

    groups = [g for g in ORDER if g in used] + [g for g in used if g not in ORDER]

    def q(s):
        return (s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                 .replace('"', '&quot;'))

    lines = ['<?xml version="1.0" encoding="utf-8"?>',
             '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">',
             '  <ItemGroup>']
    for g in groups:
        lines.append('    <Filter Include="%s">' % q(g))
        lines.append('      <UniqueIdentifier>{%s}</UniqueIdentifier>' %
                     str(uuid.uuid5(NS, g)).upper())
        lines.append('    </Filter>')
    lines.append('  </ItemGroup>')
    lines.append('  <ItemGroup>')
    for g in groups:
        for tag, inc in sorted(used[g]):
            lines.append('    <%s Include="%s">' % (tag, q(inc)))
            lines.append('      <Filter>%s</Filter>' % q(g))
            lines.append('    </%s>' % tag)
    lines.append('  </ItemGroup>')
    lines.append('</Project>')

    new = eol.join(lines) + eol
    # 自检：条目数不变
    assert new.count('<ClCompile Include=') == t.count('<ClCompile Include='), p
    assert new.count('<ClInclude Include=') == t.count('<ClInclude Include='), p
    open(p, 'w', encoding='utf-8-sig', newline='').write(new)
    stat = ', '.join('%s=%d' % (g, len(used[g])) for g in groups)
    other = used.get('其他')
    if other:
        stat += '  <未分类: %s>' % ', '.join(os.path.basename(i) for _, i in other)
    print('OK  %-42s %s' % (p, stat))
