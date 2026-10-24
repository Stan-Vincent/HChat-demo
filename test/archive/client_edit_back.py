# -*- coding: utf-8 -*-
"""Task18: 客户端修改好友备注全流程。
1. tcpmgr.h: 新信号 sig_update_friend_back(ok, friend_uid, back)
2. tcpmgr.cpp: ID_UPDATE_FRIEND_BACK_RSP handler
3. friendinfopage.h: on_back_edit_btn_clicked + slot_update_back_rsp
4. friendinfopage.cpp: include、构造连接、QInputDialog 编辑、发送、回包刷新
"""
def patch(path, fixes):
    b = open(path, 'rb').read()
    crlf = b'\r\n' in b
    conv = lambda s: s.replace(b'\n', b'\r\n') if crlf else s
    for old, new, cnt in fixes:
        o, n = conv(old), conv(new)
        if n in b:
            print('SKIP (already applied) %s' % path)
            continue
        c = b.count(o)
        assert c == cnt, '%s: expect %d, got %d for %r' % (path, cnt, c, o[:70])
        b = b.replace(o, n)
    open(path, 'wb').write(b)
    print('OK %s (%d fixes, %s)' % (path, len(fixes), 'CRLF' if crlf else 'LF'))

# ---------- 1. tcpmgr.h 信号 ----------
patch('tcpmgr.h', [
    (b'    void sig_create_private_chat(int uid, int other_id, int thread_id);',
     b'    void sig_create_private_chat(int uid, int other_id, int thread_id);\n'
     b'    void sig_update_friend_back(bool ok, int friend_uid, QString back);', 1),
])

# ---------- 2. tcpmgr.cpp handler（ID_ADD_FRIEND_RSP 之后） ----------
HANDLER = b'''
    _handlers.insert(ID_UPDATE_FRIEND_BACK_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "UpdateFriendBack: invalid json";
            emit sig_update_friend_back(false, 0, QString());
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "UpdateFriendBack failed, err is " << err;
            emit sig_update_friend_back(false, 0, QString());
            return;
        }

        int friend_uid = jsonObj["friend_uid"].toInt();
        QString back = jsonObj["back"].toString();
        qCDebug(lcNetwork) << "UpdateFriendBack success, uid =" << friend_uid << "back =" << back;
        emit sig_update_friend_back(true, friend_uid, back);
    });
'''
patch('tcpmgr.cpp', [
    # 锚点：ID_ADD_FRIEND_RSP handler 的收尾（"Add Friend Success" 那行 + 右括号）
    (b'''         qCDebug(lcNetwork) << "Add Friend Success " ;
      });
''',
     b'''         qCDebug(lcNetwork) << "Add Friend Success " ;
      });
''' + HANDLER, 1),
])

# ---------- 3. friendinfopage.h ----------
patch('friendinfopage.h', [
    (b'private slots:\n    void on_msg_chat_clicked();',
     b'private slots:\n'
     b'    void on_msg_chat_clicked();\n'
     b'    void on_back_edit_btn_clicked();\n'
     b'    void slot_update_back_rsp(bool ok, int friend_uid, QString back);', 1),
])

# ---------- 4. friendinfopage.cpp ----------
CLICK_IMPL = b'''
void FriendInfoPage::on_back_edit_btn_clicked()
{
    if (_user_info == nullptr) {
        return;
    }
    // input dialog, default = current remark
    bool ok = false;
    QString back = QInputDialog::getText(this, tr("\xe4\xbf\xae\xe6\x94\xb9\xe5\xa4\x87\xe6\xb3\xa8"),
        tr("\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5\xe5\xa4\x87\xe6\xb3\xa8\xe5\x90\x8d\xef\xbc\x9a"),
        QLineEdit::Normal, _user_info->GetBackName(), &ok);
    if (!ok) {
        return;  // cancelled
    }
    back = back.trimmed().left(50);
    if (back.isEmpty()) {
        back = _user_info->_nick;  // empty -> nick
    }
    if (back == _user_info->_back) {
        return;  // unchanged
    }

    QJsonObject json;
    json["uid"] = UserMgr::GetInstance()->GetUid();
    json["friend_uid"] = _user_info->_uid;
    json["back"] = back;
    QJsonDocument doc(json);
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_UPDATE_FRIEND_BACK_REQ, doc.toJson());
    ui->back_edit_btn->setEnabled(false);  // prevent double click
}

void FriendInfoPage::slot_update_back_rsp(bool ok, int friend_uid, QString back)
{
    ui->back_edit_btn->setEnabled(true);
    if (!ok) {
        QMessageBox::warning(this, tr("\xe9\x94\x99\xe8\xaf\xaf"), tr("\xe4\xbf\xae\xe6\x94\xb9\xe5\xa4\x87\xe6\xb3\xa8\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8c\xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5\xe7\xbd\x91\xe7\xbb\x9c\xe3\x80\x82"));
        return;
    }
    if (_user_info == nullptr || friend_uid != _user_info->_uid) {
        return;  // not current friend
    }
    // sync local: page + UserMgr friend list
    _user_info->_back = back;
    auto fi = UserMgr::GetInstance()->GetFriendById(friend_uid);
    if (fi) {
        fi->_back = back;
    }
    ui->bak_lb->setText(_user_info->GetBackName());
    ui->name_lb->setText(_user_info->GetBackName());
    QMessageBox::information(this, tr("\xe6\x8f\x90\xe7\xa4\xba"), tr("\xe5\xa4\x87\xe6\xb3\xa8\xe5\xb7\xb2\xe4\xbf\xae\xe6\x94\xb9\xe3\x80\x82"));
}
'''
patch('friendinfopage.cpp', [
    # include tcpmgr.h（logcategory.h 后）
    (b'#include "logcategory.h"\n',
     b'#include "logcategory.h"\n#include "tcpmgr.h"\n#include <QInputDialog>\n#include <QMessageBox>\n', 1),
    # 构造函数里连接回包信号（setupUi 后）
    (b'    ui->setupUi(this);\n'
     b'    ui->msg_chat->SetState("normal","hover","press");',
     b'    ui->setupUi(this);\n'
     b'    // update remark rsp\n'
     b'    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_update_friend_back,\n'
     b'            this, &FriendInfoPage::slot_update_back_rsp);\n'
     b'    ui->msg_chat->SetState("normal","hover","press");', 1),
    # 两个 slot 实现插在 SetInfo 实现之前
    (b'void FriendInfoPage::SetInfo(std::shared_ptr<UserInfo> user_info)',
     CLICK_IMPL + b'\nvoid FriendInfoPage::SetInfo(std::shared_ptr<UserInfo> user_info)', 1),
])

print('Task18 done')
