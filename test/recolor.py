# -*- coding: utf-8 -*-
"""HChat 客户端主色调：微信绿系 -> 珊瑚橘系。
色值映射（含原来 press 态无反馈的缺陷修复）：
  #07c160 主色(27处)   -> #ff7a45
  #3bbb4b hover亮(2处) -> #ff9264
  #41e464 选中/进度(6) -> #ffa06b
  #05a050 渐变深端(1)  -> #e85d26
  press 态 background #07c160 -> #e85d26（原来 press=normal，无按压反馈）
"""
import re

p = 'style/stylesheet.qss'
s = open(p, 'rb').read().decode('utf-8')
assert not s.startswith('\ufeff'), 'qss should be BOM-free'

mapping = {
    '#07c160': '#ff7a45',
    '#3bbb4b': '#ff9264',
    '#41e464': '#ffa06b',
    '#05a050': '#e85d26',
}
counts = {}
for old, new in mapping.items():
    counts[old] = s.count(old)
    s = s.replace(old, new)

# press 态补深色（此时 press 行已是 #ff7a45）
for btn in ['add_friend_btn', 'fail_sure_btn']:
    old_line = "#%s[state='press']  { background: #ff7a45;" % btn
    new_line = "#%s[state='press']  { background: #e85d26;" % btn
    assert old_line in s, 'press line not found for ' + btn
    s = s.replace(old_line, new_line)

open(p, 'wb').write(s.encode('utf-8'))

for old, new in mapping.items():
    print('%s -> %s : %d 处' % (old, new, counts[old]))
print('press 态优化: 2 处 (add_friend_btn / fail_sure_btn)')

# 验证无残留
left = [v for v in mapping if v in s]
print('残留旧色值:', left if left else '无')
