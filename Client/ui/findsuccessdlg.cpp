#include "findsuccessdlg.h"
#include "ui_findsuccessdlg.h"
#include <QDir>
#include "applyfriend.h"
#include "imagemanager.h"
#include <QPointer>
#include <memory>
FindSuccessDlg::FindSuccessDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FindSuccessDlg),_parent(parent)
{
    ui->setupUi(this);
    // 设置对话框标题
    setWindowTitle("添加");
    // 隐藏对话框标题栏
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    this->setObjectName("FindSuccessDlg");
    // 获取当前应用程序的路径
    QString app_path = QCoreApplication::applicationDirPath();
    QString pix_path = QDir::toNativeSeparators(app_path +
                             QDir::separator() + "static"+QDir::separator()+"head_1.jpg");
    QPixmap head_pix(pix_path);
    head_pix = head_pix.scaled(ui->head_lb->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->head_lb->setPixmap(head_pix);
    ui->add_friend_btn->SetState("normal","hover","press");
    this->setModal(true);
}

FindSuccessDlg::~FindSuccessDlg()
{
    delete ui;
}

void FindSuccessDlg::SetSearchInfo(std::shared_ptr<SearchInfo> si)
{
    ui->name_lb->setText(si->GetDisplayName());
    // _icon 是服务端 URL，QPixmap(url) 会当本地文件读 → 空白。异步拉取。
    if (!si->_icon.isEmpty()) {
        QPointer<FindSuccessDlg> guard(this);
        ImageManager::instance()->loadAvatar(
            si->_icon, [guard, this](bool ok, const QPixmap& pix, const QString& err) {
                if (!guard) { return; }   // 弹窗已关闭
                if (ok && !pix.isNull()) {
                    ui->head_lb->setPixmap(pix.scaled(ui->head_lb->size(),
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    qDebug() << "FindSuccessDlg load avatar failed:" << err;
                }
            });
        ui->head_lb->setScaledContents(true);
    }
    _si = si;
}

void FindSuccessDlg::on_add_friend_btn_clicked()
{
   this->hide();
   //弹出加好友界面
   auto applyFriend = new ApplyFriend(_parent);
   applyFriend->SetSearchInfo(_si);
   applyFriend->setModal(true);
   applyFriend->show();
}
