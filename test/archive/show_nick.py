# -*- coding: utf-8 -*-
"""让别人视角显示昵称（nick），而不是用户名（name）。
策略：userdata.h 的 6 个信息结构统一加 GetDisplayName()
      （nick 空则回退 name，服务端注册时 nick=name，兜底不会空），
所有"别人视角"的显示点改用它；SetInfo(uid, name, icon) 重载的
调用方改传 _nick（重载内部 name/nick 同值，仅用于列表显示）。
全部 ASCII 级二进制替换，与文件编码无关。
"""
import io

def patch(path, fixes):
    b = open(path, 'rb').read()
    # 自动适配文件换行风格（本项目 Client 源码 CRLF/LF 混用）
    crlf = b'\r\n' in b

    def conv(s):
        return s.replace(b'\n', b'\r\n') if crlf else s

    for old, new, cnt in fixes:
        o, n = conv(old), conv(new)
        c = b.count(o)
        assert c == cnt, '%s: expect %d, got %d for %r' % (path, cnt, c, o[:60])
        b = b.replace(o, n)
    open(path, 'wb').write(b)
    print('OK %s (%d fixes, %s)' % (path, len(fixes), 'CRLF' if crlf else 'LF'))

DISP = b'const QString& GetDisplayName() const { return _nick.isEmpty() ? _name : _nick; }'

# ---------- 1. userdata.h：6 个结构加 GetDisplayName() ----------
patch('userdata.h', [
    # SearchInfo（_icon 后）
    (b'    QString _icon;\n};',
     b'    QString _icon;\n    ' + DISP + b'\n};', 1),
    # AddFriendApply（int     _sex; 后，多空格是原文件格式）
    (b'    int     _sex;\n};',
     b'    int     _sex;\n    ' + DISP + b'\n};', 1),
    # ApplyInfo（int _status; 后）
    (b'    int _status;\n};',
     b'    int _status;\n    ' + DISP + b'\n};', 1),
    # AuthInfo（_chat_datas; 后，后跟 struct AuthRsp）
    (b'_chat_datas;\n};\n\nstruct AuthRsp',
     b'_chat_datas;\n    ' + DISP + b'\n};\n\nstruct AuthRsp', 1),
    # AuthRsp（_chat_datas; 后，后跟 struct UserInfo）
    (b'_chat_datas;\n};\n\nstruct UserInfo',
     b'_chat_datas;\n    ' + DISP + b'\n};\n\nstruct UserInfo', 1),
    # UserInfo（QString _desc; 后）
    (b'    QString _desc;\n};',
     b'    QString _desc;\n    ' + DISP + b'\n};', 1),
])

# ---------- 2. conuseritem.cpp：3 处 ----------
patch('conuseritem.cpp', [
    (b'ui->user_name_lb->setText(_info->_name);',
     b'ui->user_name_lb->setText(_info->GetDisplayName());', 3),
])

# ---------- 3. chatuserwid.cpp：会话项 ----------
patch('chatuserwid.cpp', [
    (b'ui->user_name_lb->setText(other_info->_name);',
     b'ui->user_name_lb->setText(other_info->GetDisplayName());', 1),
])

# ---------- 4. applyfrienditem.cpp：好友申请项 ----------
patch('applyfrienditem.cpp', [
    (b'ui->user_name_lb->setText(_apply_info->_name);',
     b'ui->user_name_lb->setText(_apply_info->GetDisplayName());', 1),
])

# ---------- 5. findsuccessdlg.cpp：搜索成功弹窗 ----------
patch('findsuccessdlg.cpp', [
    (b'ui->name_lb->setText(si->_name);',
     b'ui->name_lb->setText(si->GetDisplayName());', 1),
])

# ---------- 6. SetInfo(uid, name, icon) 调用方改传 _nick ----------
patch('contactuserlist.cpp', [
    (b'con_user_wid->SetInfo(con_ele->_uid,con_ele->_name, con_ele->_icon);',
     b'con_user_wid->SetInfo(con_ele->_uid,con_ele->_nick, con_ele->_icon);', 1),
    (b'con_user_wid->SetInfo(auth_rsp->_uid ,auth_rsp->_name, auth_rsp->_icon);',
     b'con_user_wid->SetInfo(auth_rsp->_uid ,auth_rsp->_nick, auth_rsp->_icon);', 1),
])
patch('chatdialog.cpp', [
    (b'chat_user_wid->SetInfo(friend_ele->_uid, friend_ele->_name,\n',
     b'chat_user_wid->SetInfo(friend_ele->_uid, friend_ele->_nick,\n', 1),
])

print('all done')
