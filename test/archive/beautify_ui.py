# -*- coding: utf-8 -*-
"""UI 美化：输入框 + ListItem 选中态 + 滚动条暖化。
1. 登录/注册/重置输入框：暖灰边框、圆角 4->6、padding 加大、新增 hover、
   focus 底色微亮、选中文本用浅橘（替代系统蓝）
2. 主界面搜索框同步
3. ListItem hover/selected：灰绿/中性灰 -> 暖橘系；selected 用渐变做
   "左侧主色竖条 + 浅橘底"（QSS 无 border-left on item，渐变模拟）
4. 滚动条 handle 中性灰 -> 暖灰
"""
p = 'style/stylesheet.qss'
s = open(p, 'rb').read().decode('utf-8').replace('\r\n', '\n')

fixes = []

# ---------- 1. 登录/注册/重置输入框 ----------
fixes.append((
"""#RegisterDialog QLineEdit,
#ResetDialog QLineEdit,
#LoginDialog QLineEdit {
    border: 1px solid #e0e0e0;
    border-radius: 4px;
    padding: 4px 8px;
    background: #ffffff;
}
#RegisterDialog QLineEdit:focus,
#ResetDialog QLineEdit:focus,
#LoginDialog QLineEdit:focus {
    border: 1px solid #ff7a45;
}""",
"""#RegisterDialog QLineEdit,
#ResetDialog QLineEdit,
#LoginDialog QLineEdit {
    border: 1px solid #e5dfd9;
    border-radius: 6px;
    padding: 6px 10px;
    background: #ffffff;
    selection-background-color: #ffd9c4;
    selection-color: #6b3413;
}
#RegisterDialog QLineEdit:hover,
#ResetDialog QLineEdit:hover,
#LoginDialog QLineEdit:hover {
    border: 1px solid #c9bcb1;
}
#RegisterDialog QLineEdit:focus,
#ResetDialog QLineEdit:focus,
#LoginDialog QLineEdit:focus {
    border: 1px solid #ff7a45;
    background: #fffaf5;
}"""))

# ---------- 2. 搜索框 ----------
fixes.append((
"""#search_edit {
    border: 1px solid #e8e8e8;
    border-radius: 4px;
    background: #ffffff;
    padding: 2px 6px;
}
#search_edit:focus {
    border: 1px solid #ff7a45;
}""",
"""#search_edit {
    border: 1px solid #e5dfd9;
    border-radius: 6px;
    background: #ffffff;
    padding: 3px 8px;
    selection-background-color: #ffd9c4;
    selection-color: #6b3413;
}
#search_edit:hover {
    border: 1px solid #c9bcb1;
}
#search_edit:focus {
    border: 1px solid #ff7a45;
    background: #fffaf5;
}"""))

# ---------- 3. ListItem hover / selected ----------
fixes.append((
"""#chat_user_list::item:selected,
#con_user_list::item:selected,
#search_list::item:selected {
    background-color: #d3d7d4;
    border: none;
    outline: none;
}
#chat_user_list::item:hover,
#con_user_list::item:hover,
#search_list::item:hover {
    background-color: #cecfd0;
    border: none;
    outline: none;
}""",
"""#chat_user_list::item:selected,
#con_user_list::item:selected,
#search_list::item:selected {
    /* 左侧主色竖条(~3px) + 浅橘底：比纯色块更有层次，
       竖条用渐变模拟（QSS 的 ::item 不支持 border-left） */
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                stop:0 #ff7a45, stop:0.012 #ff7a45,
                                stop:0.012 #ffead9, stop:1 #ffead9);
    border: none;
    outline: none;
}
#chat_user_list::item:hover,
#con_user_list::item:hover,
#search_list::item:hover {
    background-color: #fdf2ea;
    border: none;
    outline: none;
}"""))

# ---------- 4. 滚动条 handle 暖化（纵向 + 横向） ----------
fixes.append((
"""QScrollBar::handle:vertical {
    background: #adaaa9;
    min-height: 20px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover {
    background: #8c8a89;
}""",
"""QScrollBar::handle:vertical {
    background: #c0b4ab;
    min-height: 20px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover {
    background: #a3938a;
}"""))
fixes.append((
"""QScrollBar::handle:horizontal {
    background: #adaaa9;
    min-width: 20px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover {
    background: #8c8a89;
}""",
"""QScrollBar::handle:horizontal {
    background: #c0b4ab;
    min-width: 20px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover {
    background: #a3938a;
}"""))

for old, new in fixes:
    assert old in s, 'NOT FOUND:\n' + old[:80]
    s = s.replace(old, new)

open(p, 'wb').write(s.encode('utf-8'))
print('applied %d fixes' % len(fixes))

# 验证
t = open(p, 'rb').read().decode('utf-8')
print('大括号:', t.count('{'), '/', t.count('}'), ' BOM:', t.startswith('\ufeff'))
for bad in ['#d3d7d4', '#cecfd0', '#adaaa9', '#8c8a89', '#e0e0e0', '#e8e8e8']:
    if bad in t:
        print('RESIDUE:', bad)
print('done')
