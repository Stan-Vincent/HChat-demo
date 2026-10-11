#include "friendinfopage.h"
#include "ui_friendinfopage.h"
#include "imagemanager.h"
#include <QPointer>
#include <QDebug>
#include "logcategory.h"

FriendInfoPage::FriendInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendInfoPage),_user_info(nullptr)
{
    ui->setupUi(this);
    ui->msg_chat->SetState("normal","hover","press");
    ui->video_chat->SetState("normal","hover","press");
    ui->voice_chat->SetState("normal","hover","press");
}

FriendInfoPage::~FriendInfoPage()
{
    delete ui;
}

void FriendInfoPage::SetInfo(std::shared_ptr<UserInfo> user_info)
{
    _user_info = user_info;
    // _icon 是服务端 URL，QPixmap(url) 会当本地文件读 → 空白。异步拉取。
    if (!user_info->_icon.isEmpty()) {
        QPointer<FriendInfoPage> guard(this);
        ImageManager::instance()->loadAvatar(
            user_info->_icon, [guard, this](bool ok, const QPixmap& pix, const QString& err) {
                if (!guard) { return; }
                if (ok && !pix.isNull()) {
                    ui->icon_lb->setPixmap(pix.scaled(ui->icon_lb->size(),
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    qCDebug(lcAvatar) << "FriendInfoPage load avatar failed:" << err;
                }
            });
        ui->icon_lb->setScaledContents(true);
    }

    ui->name_lb->setText(user_info->_name);
    ui->nick_lb->setText(user_info->_nick);
    ui->bak_lb->setText(user_info->_nick);
}

void FriendInfoPage::on_msg_chat_clicked()
{
    qCDebug(lcAvatar) << "msg chat btn clicked";
    emit sig_jump_chat_item(_user_info);
}
