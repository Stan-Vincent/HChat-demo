# -*- coding: utf-8 -*-
"""修复三项：
1. 登录/注册主按钮交互色统一为珊瑚橘三态（原来 hover 绿 #35c953、press 蓝 #2a70f1）
2. 次要按钮 hover 从亮橘改为主色
3. 登录三件套背景 #f6fbff 淡蓝 -> 暖橘渐变（解决纯白单调 + 和主色不搭）
"""
p = 'style/stylesheet.qss'
s = open(p, 'rb').read().decode('utf-8')
assert not s.startswith('\ufeff')
# qss 是 CRLF 换行，统一成 LF 处理，写回时保持 LF（git 本就计划转 LF）
s = s.replace('\r\n', '\n')

# 1. 主按钮三态（normal 此时是 #ffa06b，需回到主色；hover/press 是漏网的绿/蓝）
old_block = """#login_btn, #reg_btn, #submit_btn, #sure_btn, #up_btn {
    background-color: #ffa06b;
    border: none;
    border-radius: 4px;
    color: #ffffff;
    font-size: 12pt;
    font-weight: bold;
}
#login_btn:hover, #reg_btn:hover, #submit_btn:hover, #sure_btn:hover, #up_btn:hover {
    background-color: #35c953;
}
#login_btn:pressed, #reg_btn:pressed, #submit_btn:pressed, #sure_btn:pressed, #up_btn:pressed {
    background-color: #2a70f1;
}"""
new_block = """#login_btn, #reg_btn, #submit_btn, #sure_btn, #up_btn {
    background-color: #ff7a45;
    border: none;
    border-radius: 4px;
    color: #ffffff;
    font-size: 12pt;
    font-weight: bold;
}
#login_btn:hover, #reg_btn:hover, #submit_btn:hover, #sure_btn:hover, #up_btn:hover {
    background-color: #ff9264;
}
#login_btn:pressed, #reg_btn:pressed, #submit_btn:pressed, #sure_btn:pressed, #up_btn:pressed {
    background-color: #e85d26;
}"""
assert old_block in s, 'main button block not found'
s = s.replace(old_block, new_block)
print('1. 主按钮三态 -> #ff7a45 / #ff9264 / #e85d26')

# 2. 次要按钮 hover 亮橘 -> 主色
old_sec = """#return_btn:hover, #cancel_btn:hover, #varify_btn:hover, #find_btn:hover,
#logout_btn:hover {
    border: 1px solid #ffa06b;
    color: #ffa06b;
}"""
new_sec = """#return_btn:hover, #cancel_btn:hover, #varify_btn:hover, #find_btn:hover,
#logout_btn:hover {
    border: 1px solid #ff7a45;
    color: #ff7a45;
}"""
assert old_sec in s, 'secondary button hover block not found'
s = s.replace(old_sec, new_sec)
print('2. 次要按钮 hover -> #ff7a45')

# 3. 登录三件套背景 -> 暖橘渐变
old_bg = """QDialog#LoginDialog,
QDialog#RegisterDialog,
QDialog#ResetDialog {
    background-color: #f6fbff;
}"""
new_bg = """QDialog#LoginDialog,
QDialog#RegisterDialog,
QDialog#ResetDialog {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                stop:0 #fff8f3, stop:1 #ffe9d8);
}"""
assert old_bg in s, 'login dialog bg block not found'
s = s.replace(old_bg, new_bg)
print('3. 登录页背景 -> 暖橘渐变')

open(p, 'wb').write(s.encode('utf-8'))
print('saved')

# 验证无残留脏色
for bad in ['#35c953', '#2a70f1', '#f6fbff', '#3bbb4b', '#07c160', '#41e464', '#05a050']:
    if bad in s:
        print('RESIDUE:', bad)
print('residue check done')
