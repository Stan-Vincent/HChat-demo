// GroupMemberDlg.cpp
#include "groupmemberdlg.h"
#include "selectfrienddlg.h"
#include "usermgr.h"
#include "userdata.h"
#include "tcpmgr.h"
#include "global.h"
#include "avatarutil.h"
#include "logcategory.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QIcon>

GroupMemberDlg::GroupMemberDlg(int thread_id, const QString& group_name,
                               const std::vector<int>& members, QWidget* parent)
    : QDialog(parent), _thread_id(thread_id), _members(members)
{
    setWindowTitle(tr("群成员"));
    setMinimumWidth(300);
    buildUi(group_name);
    fillMembers(members);

    // 加成员回包（1036）：成功就把新 uid 并进本地列表并重绘。
    // 这里**不重发 1018 刷新**——老服务端没有 members 字段，重刷反而会把列表冲掉；
    // 新服务端下次加载会话时也会带全量成员，本地追加不会重复（下面按 uid 去重）。
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_group_member_rsp,
            this, [this](int error, int thread_id) {
        if (thread_id != _thread_id) {
            return;   // 别的群的回包
        }
        if (error != ErrorCodes::SUCCESS) {
            QMessageBox::warning(this, tr("邀请好友"),
                tr("邀请失败（错误码 %1）。").arg(error));
            return;
        }
        // 用发送时记下的 uid 并进列表（服务端不回"加了谁"），按 uid 去重
        for (int uid : _pending_added) {
            if (std::find(_members.begin(), _members.end(), uid) == _members.end()) {
                _members.push_back(uid);
            }
        }
        _pending_added.clear();
        fillMembers(_members);
    });
}

GroupMemberDlg::~GroupMemberDlg()
{
}

void GroupMemberDlg::buildUi(const QString& group_name)
{
    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(tr("「%1」的成员").arg(group_name), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    _member_list = new QListWidget(this);
    _member_list->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(_member_list);

    _invite_btn = new QPushButton(tr("邀请好友入群"), this);
    layout->addWidget(_invite_btn);
    connect(_invite_btn, &QPushButton::clicked, this, [this]() {
        SelectFriendDlg dlg(_members, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        auto picked = dlg.GetSelectedUids();
        if (picked.empty()) {
            return;
        }
        sendAddMembersReq(picked);
    });
}

void GroupMemberDlg::fillMembers(const std::vector<int>& members)
{
    _members = members;
    _member_list->clear();
    if (members.empty()) {
        auto* item = new QListWidgetItem(tr("（暂无成员信息）"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        _member_list->addItem(item);
        return;
    }

    std::shared_ptr<UserMgr> user_mgr = UserMgr::GetInstance();
    int self_uid = user_mgr->GetUid();
    for (int uid : members) {
        QString text;
        QString icon_path;

        auto ui_ptr = user_mgr->GetFriendById(uid);
        if (ui_ptr) {
            text = ui_ptr->_nick.isEmpty() ? ui_ptr->_name : ui_ptr->_nick;
            icon_path = ui_ptr->_icon;
        } else {
            // 非好友成员：本地只有 uid，昵称要等服务端补下
            text = tr("用户 %1").arg(uid);
        }
        if (uid == self_uid) {
            text += tr("（我）");
        }

        auto* item = new QListWidgetItem(text);
        if (!icon_path.isEmpty()) {
            item->setIcon(QIcon(icon_path));
        } else {
            item->setIcon(QIcon(AvatarUtil::MakeLetterAvatar(text, 32, uid)));
        }
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        _member_list->addItem(item);
    }

    qCDebug(lcUser) << "group member dlg: thread_id =" << _thread_id
                    << "members =" << (int)members.size();
}

void GroupMemberDlg::sendAddMembersReq(const std::vector<int>& uids)
{
    if (!TcpMgr::GetInstance()->IsConnected()) {
        QMessageBox::warning(this, tr("邀请好友"), tr("网络未连接。"));
        return;
    }

    QJsonObject obj;
    obj["uid"] = UserMgr::GetInstance()->GetUid();
    obj["thread_id"] = _thread_id;
    QJsonArray arr;
    for (int uid : uids) {
        arr.append(uid);
    }
    obj["members"] = arr;

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    _pending_added = uids;   // 记住邀请了谁，回包成功时并进列表
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_ADD_GROUP_MEMBER_REQ, data);
    qCDebug(lcUser) << "send add group member req, thread_id =" << _thread_id
                    << "count =" << (int)uids.size();
}
