#include "applyfrienditem.h"
#include "ui_applyfrienditem.h"
#include "imagemanager.h"
#include <QPointer>
#include "logcategory.h"

ApplyFriendItem::ApplyFriendItem(QWidget *parent) :
    ListItemBase(parent), _added(false),
    ui(new Ui::ApplyFriendItem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::APPLY_FRIEND_ITEM);
    ui->addBtn->SetState("normal","hover", "press");
    ui->addBtn->hide();
    connect(ui->addBtn, &ClickedBtn::clicked,  [this](){
        emit this->sig_auth_friend(_apply_info);
    });
}

ApplyFriendItem::~ApplyFriendItem()
{
    delete ui;
}

void ApplyFriendItem::SetInfo(std::shared_ptr<ApplyInfo> apply_info)
{
    _apply_info = apply_info;
    // _icon 是服务端 URL，QPixmap(url) 会当本地文件读 → 空白。异步拉取。
    if (!_apply_info->_icon.isEmpty()) {
        QPointer<ApplyFriendItem> guard(this);
        ImageManager::instance()->loadAvatar(
            _apply_info->_icon, [guard, this](bool ok, const QPixmap& pix, const QString& err) {
                if (!guard) { return; }
                if (ok && !pix.isNull()) {
                    ui->icon_lb->setPixmap(pix.scaled(ui->icon_lb->size(),
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    qCDebug(lcAvatar) << "ApplyFriendItem load avatar failed:" << err;
                }
            });
        ui->icon_lb->setScaledContents(true);
    }

    ui->user_name_lb->setText(_apply_info->_name);
    ui->user_chat_lb->setText(_apply_info->_desc);
}

void ApplyFriendItem::ShowAddBtn(bool bshow)
{
    if (bshow) {
		ui->addBtn->show();
		ui->already_add_lb->hide();
        _added = false;
    }
    else {
		ui->addBtn->hide();
		ui->already_add_lb->show();
        _added = true;
    }
}

int ApplyFriendItem::GetUid() {
    return _apply_info->_uid;
}


