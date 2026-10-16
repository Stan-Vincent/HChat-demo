#ifndef USERINFOPAGE_H
#define USERINFOPAGE_H

#include <QWidget>
#include <QImage>

class QPixmap;

namespace Ui {
class UserInfoPage;
}

class UserInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit UserInfoPage(QWidget *parent = nullptr);
    ~UserInfoPage();
    // ★ 换账号登录后重新从 UserMgr 拉取并回填。
    //   构造函数只在【首次登录】跑一次，不清/不刷的话换号后这里全是上一个账号的资料。
    void refreshUserInfo();

signals:
    // ★ 点「退出登录」：由 ChatDialog 转给 MainWindow 处理
    void sig_logout();

private slots:
    void on_up_btn_clicked();
    void on_submit_btn_clicked();

private:
    // 把头像控件的图缩放后填进 _pending_icon
    void refreshHeadPreview(const QPixmap& pix);
    // 从服务端 URL 异步拉自己的头像并预览
    void loadSelfAvatar(const QString& iconUrl);
    // 发 POST /update_userinfo，成功后同步 UserMgr
    void submitUserInfo();

    Ui::UserInfoPage *ui;
    // 用户刚裁完、还没上传的头像。空 = 这次没换头像
    QImage _pending_icon;
};

#endif // USERINFOPAGE_H