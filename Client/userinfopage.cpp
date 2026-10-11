#include "userinfopage.h"
#include "ui_userinfopage.h"
#include "usermgr.h"
#include "uploadmanager.h"
#include "httpmgr.h"
#include "global.h"
#include <QDebug>
#include <QMessageBox>
#include <QNetworkReply>
#include "imagecropperdialog.h"
#include "imagecropperlabel.h"
#include "imagemanager.h"
#include <QPointer>
#include <QFileDialog>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>

UserInfoPage::UserInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UserInfoPage)
{
    ui->setupUi(this);

    // 性别下拉框：0=男 1=女 2=保密
    // ★ 原来 .ui 里根本没有性别控件，而服务端 UpdateUserInfo 是按 uid 更新 sex 的，
    //   不给值就永远写 0。这里补上，让"资料提交"是完整的。
    ui->sex_cbx->addItem("男", 0);
    ui->sex_cbx->addItem("女", 1);
    ui->sex_cbx->addItem("保密", 2);

    auto info = UserMgr::GetInstance()->GetUserInfo();
    // ★ GetIcon()/GetNick() 等内部直接解引用 _user_info，
    //   登录回包没到时会崩。所以先判空。
    if (info == nullptr) {
        qDebug() << "UserInfoPage: user info is null";
    } else {
        // ★ _icon 是服务端返回的 URL（"upload/xxx.png"），不是本地文件路径，
        //   不能用 QPixmap(_icon) 直接构造（那会当成文件去读，必然加载失败 → 头像空白）。
        //   必须走 ImageManager::downloadImage 异步从 gate 服务器拉取，
        //   否则重启程序后头像永远空白（之前只靠上传后的内存预览撑着，一重启就没了）。
        if (!info->_icon.isEmpty()) {
            QPointer<UserInfoPage> guard(this);
            ImageManager::instance()->loadAvatar(
                info->_icon, [guard](bool ok, const QPixmap& pix, const QString& err) {
                    if (!guard) {
                        return;     // 页面已被销毁（比如下载途中退出了登录）
                    }
                    if (ok && !pix.isNull()) {
                        guard->refreshHeadPreview(pix);
                    } else {
                        qDebug() << "UserInfoPage load self avatar failed:" << err;
                    }
                });
        }
        ui->head_lb->setScaledContents(true);

        ui->nick_ed->setText(info->_nick);
        ui->name_ed->setText(info->_name);
        ui->desc_ed->setText(info->_desc);

        int idx = ui->sex_cbx->findData(info->_sex);
        if (idx >= 0) {
            ui->sex_cbx->setCurrentIndex(idx);
        }
    }

    // 用户名（name）是注册时定的，不允许在这里改 —— 置灰避免误操作
    ui->name_ed->setEnabled(false);

    // 「退出登录」：只发信号，断开连接和切页面交给 ChatDialog / MainWindow
    connect(ui->logout_btn, &QPushButton::clicked, this, &UserInfoPage::sig_logout);

    // ★ 提交按钮【不要】再手动 connect：on_submit_btn_clicked 已经是 private slots，
    //   且 .ui 里按钮的 objectName 就是 "submit_btn"，setupUi 会自动把
    //   submit_btn->clicked 连到 on_submit_btn_clicked。之前这里又手动 connect 了一次，
    //   导致一次点击触发两次提交（弹两次"资料已保存"，并发上传头像/提交还会互相覆盖）。
    //   删掉手动 connect，只保留自动连接，点一次就提交一次。
}

UserInfoPage::~UserInfoPage()
{
    delete ui;
}

void UserInfoPage::refreshHeadPreview(const QPixmap& pix)
{
    ui->head_lb->setPixmap(pix.scaled(ui->head_lb->size(), Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
    ui->head_lb->setScaledContents(true);
}

//上传头像：裁剪后先本地预览 + 存进 _pending_icon，真正上传等点「提交」
void UserInfoPage::on_up_btn_clicked()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("选择图片"),
        QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)")
    );
    if (filename.isEmpty()) {
        return;
    }

    // 加载一次只为提前发现"文件损坏/插件缺失"，避免裁剪框里一片空白
    QPixmap probe;
    if (!probe.load(filename)) {
        QMessageBox::critical(this, tr("错误"),
                              tr("加载图片失败，请换一张试试。"),
                              QMessageBox::Ok);
        return;
    }

    QPixmap cropped = ImageCropperDialog::getCroppedImage(filename, 600, 400, CropperShape::CIRCLE);
    if (cropped.isNull()) {
        return;      // 用户点了取消
    }

    // ★ 之前这里还会往 AppData/avatars/head.png 存一份本地文件，
    //   但客户端从来没从这个路径读过，纯属写了个没人用的文件。
    //   删掉，改为存内存里的 _pending_icon，点「提交」时才真正传给服务端。
    _pending_icon = cropped.toImage();
    refreshHeadPreview(cropped);
    qDebug() << "avatar cropped, size =" << _pending_icon.size();
}

void UserInfoPage::on_submit_btn_clicked()
{
    submitUserInfo();
}

void UserInfoPage::submitUserInfo()
{
    auto info = UserMgr::GetInstance()->GetUserInfo();
    if (info == nullptr) {
        QMessageBox::warning(this, tr("错误"), tr("用户信息还没加载好，请稍后再试。"));
        return;
    }

    QString nick = ui->nick_ed->text().trimmed();
    QString desc = ui->desc_ed->text().trimmed();
    int    sex  = ui->sex_cbx->currentData().toInt();

    // 前端先拦一道，服务端也会再截断一次（防绕过）
    if (nick.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("昵称不能为空。"));
        ui->nick_ed->setFocus();
        return;
    }
    if (nick.length() > 21) {
        nick = nick.left(21);
        ui->nick_ed->setText(nick);
    }
    if (desc.length() > 200) {
        desc = desc.left(200);
        ui->desc_ed->setText(desc);
    }

    // 点提交时禁用按钮，防止连点导致重复上传头像 / 重复提交
    ui->submit_btn->setEnabled(false);
    ui->submit_btn->setText("提交中...");

    int uid = info->_uid;

    // ★ 分两步：先（有头像才）上传头像拿到路径，再提交资料。
    //   顺序反过来的话没法把新头像路径写进同一次资料提交。
    //   iconPath 为空 = 这次没换头像，提交时沿用原来的。
    auto doSubmit = [this, uid, nick, desc, sex](const QString& iconPath) {
        // 服务端要的是最终生效的头像路径：换了就用新的，没换就用旧的
        QString finalIcon = iconPath.isEmpty() ? UserMgr::GetInstance()->GetIcon() : iconPath;

        QJsonObject json;
        json["uid"]  = uid;
        json["nick"] = nick;
        json["desc"] = desc;
        json["sex"]  = sex;
        json["icon"] = finalIcon;

        // ★ 用 PostHttpReqRaw 而不是 sig_userinfo_mod_finish 信号：
        //   那个信号对所有提交请求共用、只能靠 ReqId 区分模块，区分不了
        //   「第几次提交」。每点一次提交就永久挂一个 lambda，累积后第 N 次提交
        //   会同时触发前 N-1 个回调 —— 弹出 N-1 个「资料已保存」。
        //   PostHttpReqRaw 一请求一回调，天然对应。
        HttpMgr::GetInstance()->PostHttpReqRaw(
            QUrl(gate_url_prefix + "/update_userinfo"), json,
            [this, nick, desc, sex, finalIcon](bool ok, const QString& res, const QString& errMsg) {
                // 无论成败都要把按钮恢复，否则界面卡在"提交中..."
                ui->submit_btn->setEnabled(true);
                ui->submit_btn->setText("提交");

                if (!ok) {
                    QMessageBox::warning(this, tr("错误"),
                                         tr("网络请求失败：%1\n请检查 GateServer 是否在运行。").arg(errMsg));
                    return;
                }
                QJsonDocument doc = QJsonDocument::fromJson(res.toUtf8());
                if (doc.isNull() || !doc.isObject()) {
                    QMessageBox::warning(this, tr("错误"), tr("服务端返回的数据无法解析。"));
                    return;
                }
                QJsonObject obj = doc.object();
                if (obj.value("error").toInt() != 0) {
                    QMessageBox::warning(this, tr("提交失败"),
                                         obj.value("msg").toString("未知错误"));
                    return;
                }

                // 同步到内存，聊天列表/联系人列表立刻用上新头像昵称
                UserMgr::GetInstance()->UpdateLocalUserInfo(nick, desc, sex, finalIcon);
                _pending_icon = QImage();     // 已提交，清掉待上传的图
                QMessageBox::information(this, tr("提示"), tr("资料已保存。"));
            },
            this);
    };

    // 没有新头像就直接提交
    if (_pending_icon.isNull()) {
        doSubmit(QString());
        return;
    }

    // 有新头像：先上传，成功后拿路径再提交
    ui->submit_btn->setText("上传头像中...");
    UploadManager::instance()->uploadImageData(
        uid, _pending_icon, QString("avatar.png"),
        [this, doSubmit](bool ok, const QString& urlPath, const QString& errMsg) {
            if (!ok) {
                ui->submit_btn->setEnabled(true);
                ui->submit_btn->setText("提交");
                QMessageBox::warning(this, tr("上传失败"), errMsg);
                return;
            }
            ui->submit_btn->setText("提交中...");
            doSubmit(urlPath);
        });
}