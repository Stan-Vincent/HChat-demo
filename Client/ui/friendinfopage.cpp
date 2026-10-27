#include "friendinfopage.h"
#include "SvgIconButton.h"
#include "ui_friendinfopage.h"
#include "imagemanager.h"
#include <QPointer>
#include <QDebug>
#include "logcategory.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include "global.h"
#include <QInputDialog>
#include "toast.h"
#include <QMessageBox>

FriendInfoPage::FriendInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendInfoPage),_user_info(nullptr)
{
    ui->setupUi(this);
    // 按钮体系：role 属性 + reload_qss 统一三类按钮样式（见 stylesheet.qss §9）
    ui->back_edit_btn->setProperty("role", "link");  reload_qss(ui->back_edit_btn);
    // update remark rsp
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_update_friend_back,
            this, &FriendInfoPage::slot_update_back_rsp);
    ui->msg_chat->setSvg(":/res/msg_chat_normal.svg");
    ui->msg_chat->setColors("#6B6F76","#4C5FD7","#4C5FD7");
    ui->msg_chat->setIconSize(QSize(50,50));
    ui->video_chat->setSvg(":/res/video_chat_normal.svg");
    ui->video_chat->setColors("#6B6F76","#4C5FD7","#4C5FD7");
    ui->video_chat->setIconSize(QSize(50,50));
    ui->voice_chat->setSvg(":/res/voice_chat_normal.svg");
    ui->voice_chat->setColors("#6B6F76","#4C5FD7","#4C5FD7");
    ui->voice_chat->setIconSize(QSize(50,50));
    // msg_chat 点击跳转聊天：原靠 connectSlotsByName 自动连 on_msg_chat_clicked，
    // 升级为 SvgIconButton 后信号签名为 clicked(QString, ClickLbState)，改为显式连接
    connect(ui->msg_chat, &SvgIconButton::clicked, this,
            [this](const QString&, ClickLbState) { on_msg_chat_clicked(); });
}

FriendInfoPage::~FriendInfoPage()
{
    delete ui;
}


void FriendInfoPage::on_back_edit_btn_clicked()
{
    if (_user_info == nullptr) {
        return;
    }
    // input dialog, default = current remark
    bool ok = false;
    QString back = QInputDialog::getText(this, tr("修改备注"),
        tr("请输入备注名："),
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
        Toast::showToast(this, tr("修改备注失败，请检查网络。"), Toast::Error);
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
    Toast::showToast(this, tr("备注已修改。"), Toast::Success);
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

    ui->name_lb->setText(user_info->GetBackName());
    ui->user_name_lb->setText(user_info->_name);
    ui->nick_lb->setText(user_info->_nick);
    ui->bak_lb->setText(user_info->GetBackName());
}

void FriendInfoPage::on_msg_chat_clicked()
{
    qCDebug(lcAvatar) << "msg chat btn clicked";
    emit sig_jump_chat_item(_user_info);
}
