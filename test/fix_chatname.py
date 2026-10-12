# -*- coding: utf-8 -*-
"""修复两个问题：
1. 聊天消息气泡上的发送者名字 + 聊天页标题改用昵称（GetDisplayName）
   ——原来 5 处全用 _name，与上次修的好友列表/会话列表不一致
2. 好友资料页新增"用户名"行（name_lb 顶部大标题不动，信息区加一行）
"""
def patch(path, fixes, insert=None):
    b = open(path, 'rb').read()
    crlf = b'\r\n' in b
    conv = lambda s: s.replace(b'\n', b'\r\n') if crlf else s
    for old, new, cnt in fixes:
        o, n = conv(old), conv(new)
        c = b.count(o)
        assert c == cnt, '%s: expect %d, got %d for %r' % (path, cnt, c, o[:70])
        b = b.replace(o, n)
    if insert:
        anchor, block = insert
        a, blk = conv(anchor), conv(block)
        assert a in b, '%s: anchor not found: %r' % (path, a[:70])
        b = b.replace(a, blk + a, 1)
    open(path, 'wb').write(b)
    print('OK %s (%d fixes, %s)' % (path, len(fixes), 'CRLF' if crlf else 'LF'))

# ---------- 1. chatpage.cpp：气泡名字 + 标题改用昵称 ----------
patch('chatpage.cpp', [
    # 聊天页顶部标题
    (b'ui->title_lb->setText(friend_info->_name);',
     b'ui->title_lb->setText(friend_info->GetDisplayName());', 1),
    # AppendChatMsg：自己消息
    (b'        userName = self_info->_name;',
     b'        userName = self_info->GetDisplayName();', 1),
    # AppendChatMsg：对方消息（用户报的点）
    (b'        userName = friend_info->_name;\n        userIcon = friend_info->_icon;',
     b'        userName = friend_info->GetDisplayName();\n        userIcon = friend_info->_icon;', 1),
    # on_send_btn_clicked：自己发送
    (b'    QString userName = user_info->_name;\n    QString userIcon = user_info->_icon;',
     b'    QString userName = user_info->GetDisplayName();\n    QString userIcon = user_info->_icon;', 1),
    # on_receive_btn_clicked：模拟对方接收
    (b'    QString userName = friend_info->_name;\n    QString userIcon = friend_info->_icon;',
     b'    QString userName = friend_info->GetDisplayName();\n    QString userIcon = friend_info->_icon;', 1),
])

# ---------- 2a. friendinfopage.ui：昵称行前插入"用户名"行 ----------
USERNAME_ROW = (
    '            <item>\n'
    '             <widget class="QLabel" name="user_name_tip">\n'
    '              <property name="minimumSize">\n'
    '               <size>\n'
    '                <width>0</width>\n'
    '                <height>30</height>\n'
    '               </size>\n'
    '              </property>\n'
    '              <property name="maximumSize">\n'
    '               <size>\n'
    '                <width>50</width>\n'
    '                <height>30</height>\n'
    '               </size>\n'
    '              </property>\n'
    '              <property name="text">\n'
    '               <string>\u7528\u6237\u540d\uff1a</string>\n'
    '              </property>\n'
    '             </widget>\n'
    '            </item>\n'
    '            <item>\n'
    '             <widget class="QLabel" name="user_name_lb">\n'
    '              <property name="text">\n'
    '               <string>\u7528\u6237\u540d</string>\n'
    '              </property>\n'
    '             </widget>\n'
    '            </item>\n'
)
patch('friendinfopage.ui', [],
      insert=(b'            <item>\n             <widget class="QLabel" name="nick_tip">',
              USERNAME_ROW.encode('utf-8')))

# ---------- 2b. friendinfopage.cpp：SetInfo 填用户名 ----------
patch('friendinfopage.cpp', [
    (b'    ui->name_lb->setText(user_info->_name);\n'
     b'    ui->nick_lb->setText(user_info->_nick);',
     b'    ui->name_lb->setText(user_info->_name);\n'
     b'    ui->user_name_lb->setText(user_info->_name);\n'
     b'    ui->nick_lb->setText(user_info->_nick);', 1),
])

print('all done')
