# -*- coding: utf-8 -*-
"""Task16: 资料页信息区排版重构。
1. widget_2 里拆出昵称行成独立 widget_6（放备注行 widget_5 之前），
   解决"用户名：hoo 昵称：hoo"挤一行的丑排版
2. 备注行加"修改"按钮 back_edit_btn
3. cpp 大标题 name_lb 改显 GetBackName()（备注优先，与信息区去重）
"""
NICK_TIP = ('            <item>\n'
            '             <widget class="QLabel" name="nick_tip">\n'
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
            '               <string>\u6635\u79f0\uff1a</string>\n'
            '              </property>\n'
            '             </widget>\n'
            '            </item>\n')
NICK_LB = ('            <item>\n'
           '             <widget class="QLabel" name="nick_lb">\n'
           '              <property name="text">\n'
           '               <string>\u7433\u7433</string>\n'
           '              </property>\n'
           '             </widget>\n'
           '            </item>\n')
SPACER = ('          <item>\n'
          '           <spacer name="%s">\n'
          '            <property name="orientation">\n'
          '             <enum>Qt::Horizontal</enum>\n'
          '            </property>\n'
          '            <property name="sizeHint" stdset="0">\n'
          '             <size>\n'
          '              <width>40</width>\n'
          '              <height>20</height>\n'
          '             </size>\n'
          '            </property>\n'
          '           </spacer>\n'
          '          </item>\n')

# 从 widget_2 删掉 nick_tip + nick_lb 两个 item
REMOVE = NICK_TIP + NICK_LB
# 新 widget_6：昵称独立行（结构照 widget_5）
WIDGET6 = ('          <widget class="QWidget" name="widget_6" native="true">\n'
           '           <property name="minimumSize">\n'
           '            <size>\n'
           '             <width>0</width>\n'
           '             <height>40</height>\n'
           '            </size>\n'
           '           </property>\n'
           '           <property name="maximumSize">\n'
           '            <size>\n'
           '             <width>16777215</width>\n'
           '             <height>40</height>\n'
           '            </size>\n'
           '           </property>\n'
           '           <layout class="QHBoxLayout" name="horizontalLayout_5">\n'
           '            <property name="leftMargin">\n'
           '             <number>0</number>\n'
           '            </property>\n'
           '            <property name="topMargin">\n'
           '             <number>0</number>\n'
           '            </property>\n'
           '            <property name="rightMargin">\n'
           '             <number>0</number>\n'
           '            </property>\n'
           '            <property name="bottomMargin">\n'
           '             <number>0</number>\n'
           '            </property>\n'
           + NICK_TIP + NICK_LB + (SPACER % 'horizontalSpacer_6')
           + '           </layout>\n'
           '          </widget>\n')
# 备注行加"修改"按钮（bak_lb item 之后）
BACK_BTN = ('            <item>\n'
            '             <widget class="QPushButton" name="back_edit_btn">\n'
            '              <property name="text">\n'
            '               <string>\u4fee\u6539</string>\n'
            '              </property>\n'
            '              <property name="cursor">\n'
            '               <cursorShape>PointingHandCursor</cursorShape>\n'
            '              </property>\n'
            '             </widget>\n'
            '            </item>\n')

# ---------- 1. ui ----------
p = 'friendinfopage.ui'
b = open(p, 'rb').read()
assert b'\r\n' in b, 'ui should be CRLF'
conv = lambda s: s.replace('\n', '\r\n').encode('utf-8')

old = conv(REMOVE)
assert b.count(old) == 1, 'nick items not found or not unique'
b = b.replace(old, b'')

anchor = conv('          <widget class="QWidget" name="widget_5" native="true">')
assert b.count(anchor) == 1, 'widget_5 anchor not found'
b = b.replace(anchor, conv(WIDGET6) + anchor, 1)

# bak_lb item 之后插按钮：找 bak_lb widget 块的结尾
bak_end = conv('             <widget class="QLabel" name="bak_lb">\n'
               '              <property name="text">\n'
               '               <string>\u7433\u7433</string>\n'
               '              </property>\n'
               '             </widget>\n'
               '            </item>\n')
assert b.count(bak_end) == 1, 'bak_lb block not found'
b = b.replace(bak_end, bak_end + conv(BACK_BTN), 1)
open(p, 'wb').write(b)
print('OK friendinfopage.ui')

# ---------- 2. cpp：大标题去重 ----------
p = 'friendinfopage.cpp'
s = open(p, 'rb').read()
old = b'ui->name_lb->setText(user_info->_name);'
new = b'ui->name_lb->setText(user_info->GetBackName());'
assert s.count(old) == 1
s = s.replace(old, new)
open(p, 'wb').write(s)
print('OK friendinfopage.cpp (name_lb -> GetBackName)')
