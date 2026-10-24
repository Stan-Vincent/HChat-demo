# -*- coding: utf-8 -*-
"""方案A：修复备注显示链路（客户端-only）。
1. userdata.h：UserInfo 加 _back 成员 + 主构造加 back 参数 + GetBackName()
   （back 空回退 nick——没设备注就显示对方昵称）
2. usermgr.cpp：AppendFriendList 构造调用修正参数错位
   （原来 desc 被塞进 last_msg、back 被塞进 desc）
3. friendinfopage.cpp：\"备注\"标签改显 GetBackName()（原来错显昵称）
"""
def patch(path, fixes):
    b = open(path, 'rb').read()
    crlf = b'\r\n' in b
    conv = lambda s: s.replace(b'\n', b'\r\n') if crlf else s
    for old, new, cnt in fixes:
        o, n = conv(old), conv(new)
        c = b.count(o)
        assert c == cnt, '%s: expect %d, got %d for %r' % (path, cnt, c, o[:70])
        b = b.replace(o, n)
    open(path, 'wb').write(b)
    print('OK %s (%d fixes, %s)' % (path, len(fixes), 'CRLF' if crlf else 'LF'))

# ---------- 1. userdata.h：构造函数加 back 参数 ----------
patch('userdata.h', [
    (b'UserInfo(int uid, QString name, QString nick, QString icon, int sex, QString last_msg = "", QString desc=""):\n'
     b'        _uid(uid),_name(name),_nick(nick),_icon(icon),_sex(sex),_desc(desc){}',
     b'UserInfo(int uid, QString name, QString nick, QString icon, int sex, QString last_msg = "", QString desc="", QString back=""):\n'
     b'        _uid(uid),_name(name),_nick(nick),_icon(icon),_sex(sex),_desc(desc),_back(back){}', 1),
    # 成员 + GetBackName()（GetDisplayName 已在上一轮加过）
    (b'    QString _desc;\n'
     b'    const QString& GetDisplayName() const { return _nick.isEmpty() ? _name : _nick; }\n'
     b'};',
     b'    QString _desc;\n'
     b'    QString _back;\n'
     b'    const QString& GetDisplayName() const { return _nick.isEmpty() ? _name : _nick; }\n'
     b'    const QString& GetBackName() const { return _back.isEmpty() ? _nick : _back; }\n'
     b'};', 1),
])

# ---------- 2. usermgr.cpp：修正参数错位 ----------
patch('usermgr.cpp', [
    (b'auto info = std::make_shared<UserInfo>(uid, name,\n'
     b'            nick, icon, sex, desc, back);',
     b'auto info = std::make_shared<UserInfo>(uid, name,\n'
     b'            nick, icon, sex, "", desc, back);', 1),
])

# ---------- 3. friendinfopage.cpp：备注标签改用 back ----------
patch('friendinfopage.cpp', [
    (b'ui->bak_lb->setText(user_info->_nick);',
     b'ui->bak_lb->setText(user_info->GetBackName());', 1),
])

print('all done')
