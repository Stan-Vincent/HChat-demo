#include "conuseritem.h"
#include "ui_conuseritem.h"
#include "imagemanager.h"
#include <QPointer>
#include "logcategory.h"

ConUserItem::ConUserItem(QWidget *parent) :
    ListItemBase(parent),
    ui(new Ui::ConUserItem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CONTACT_USER_ITEM);
    ui->red_point->raise();
    ShowRedPoint(false);
}

ConUserItem::~ConUserItem()
{
    delete ui;
}

QSize ConUserItem::sizeHint() const
{
    return QSize(250, 70); // 返回自定义的尺寸
}

// _icon 是服务端 URL（"upload/xxx.png"），QPixmap(url) 会当成本地文件读，必然空白。
// 统一走 ImageManager 异步下载（自带磁盘缓存），QPointer 兜底防 item 提前销毁。
void ConUserItem::LoadIconAsync(const QString& icon)
{
    if (icon.isEmpty()) {
        return;
    }
    QPointer<ConUserItem> guard(this);
    ImageManager::instance()->loadAvatar(
        icon, [guard, this](bool ok, const QPixmap& pix, const QString& err) {
            if (!guard) { return; }
            if (ok && !pix.isNull()) {
                ui->icon_lb->setPixmap(pix.scaled(ui->icon_lb->size(),
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                qCDebug(lcAvatar) << "ConUserItem load avatar failed:" << err;
            }
        });
    ui->icon_lb->setScaledContents(true);
}

void ConUserItem::SetInfo(std::shared_ptr<AuthInfo> auth_info)
{
    _info = std::make_shared<UserInfo>(auth_info);
    LoadIconAsync(_info->_icon);
    ui->user_name_lb->setText(_info->GetDisplayName());
}

void ConUserItem::SetInfo(int uid, QString name, QString icon)
{
     _info = std::make_shared<UserInfo>(uid,name, name, icon, 0);
     LoadIconAsync(_info->_icon);
     ui->user_name_lb->setText(_info->GetDisplayName());
}

void ConUserItem::SetInfo(std::shared_ptr<AuthRsp> auth_rsp){
    _info = std::make_shared<UserInfo>(auth_rsp);
    LoadIconAsync(_info->_icon);
    ui->user_name_lb->setText(_info->GetDisplayName());
}

void ConUserItem::ShowRedPoint(bool show)
{
    if(show){
        ui->red_point->show();
    }else{
        ui->red_point->hide();
    }

}

std::shared_ptr<UserInfo> ConUserItem::GetInfo()
{
    return _info;
}
