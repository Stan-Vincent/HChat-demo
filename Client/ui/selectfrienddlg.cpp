// SelectFriendDlg.cpp
#include "selectfrienddlg.h"
#include "usermgr.h"
#include "userdata.h"
#include "avatarutil.h"
#include "logcategory.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <algorithm>

SelectFriendDlg::SelectFriendDlg(const std::vector<int>& exclude_uids, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("邀请好友入群"));
    setMinimumWidth(300);
    setMinimumHeight(360);

    auto* layout = new QVBoxLayout(this);

    auto* tip = new QLabel(tr("勾选要邀请的好友（已在群里的不会列出）："), this);
    layout->addWidget(tip);

    _list = new QListWidget(this);
    layout->addWidget(_list);

    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();
    auto* ok_btn = new QPushButton(tr("确定"), this);
    auto* cancel_btn = new QPushButton(tr("取消"), this);
    btn_layout->addWidget(ok_btn);
    btn_layout->addWidget(cancel_btn);
    layout->addLayout(btn_layout);

    connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);

    fillFriends(exclude_uids);
}

SelectFriendDlg::~SelectFriendDlg()
{
}

void SelectFriendDlg::fillFriends(const std::vector<int>& exclude_uids)
{
    _list->clear();

    auto friends = UserMgr::GetInstance()->GetAllFriends();
    int shown = 0;
    for (auto& f : friends) {
        if (!f) {
            continue;
        }
        if (std::find(exclude_uids.begin(), exclude_uids.end(), f->_uid)
                != exclude_uids.end()) {
            continue;   // 已在群里
        }
        const QString shown_name = f->_nick.isEmpty() ? f->_name : f->_nick;
        auto* item = new QListWidgetItem(
            QStringLiteral("%1（uid %2）").arg(shown_name).arg(f->_uid));
        item->setData(Qt::UserRole, f->_uid);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setIcon(QIcon(AvatarUtil::MakeLetterAvatar(shown_name, 32, f->_uid)));
        _list->addItem(item);
        ++shown;
    }

    if (shown == 0) {
        auto* item = new QListWidgetItem(tr("（没有可邀请的好友）"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        _list->addItem(item);
    }

    qCDebug(lcUser) << "SelectFriendDlg: 可邀请好友" << shown << "个";
}

std::vector<int> SelectFriendDlg::GetSelectedUids() const
{
    std::vector<int> out;
    for (int i = 0; i < _list->count(); ++i) {
        auto* item = _list->item(i);
        if (item->checkState() == Qt::Checked) {
            out.push_back(item->data(Qt::UserRole).toInt());
        }
    }
    return out;
}
