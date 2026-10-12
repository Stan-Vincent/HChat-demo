#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    // ★ 主动退出登录时调用：取消勾选「记住密码 / 自动登录」并清掉已存的密码，
    //   否则下次启动会被自动登录又拉回去。
    void SetAutoLogin(bool on);

    // ★ 登出后回到登录页时调用：清掉登录过程中显示的"聊天服务连接成功，正在登录..."
    //   提示、恢复按钮可用状态。否则界面会停在"正在登录"且按钮被禁用，无法使用。
    void resetUIState();

    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    void initHead();
    void initHttpHandlers();
    void showTip(QString str,bool b_ok);
    bool checkUserValid();
    bool checkPwdValid();
    bool enableBtn(bool);

private:
    //「记住密码 / 自动登录」的读写（login.ini，密码用 Windows DPAPI 加密）
    void loadSettings();
    void saveSettings();
    // 按"密码是否来自自动填入"更新眼睛按钮的可用状态。
    // 自动填入的密码没有"核对输错"的需求，点眼睛纯展示，只增加泄露面
    // （共享屏幕 / 旁人窥视），故禁用；用户一旦手动编辑密码立即恢复。
    void updateEyeButtonState();
    bool eventFilter(QObject *obj, QEvent *ev) override;
    Ui::LoginDialog *ui;

    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
    QMap<TipErr, QString> _tip_errs;
    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);
    int _uid;
    QString _token;

private slots:
    void slot_forget_pwd();
    void on_login_btn_clicked();
    void slot_login_mod_finish(ReqId id, QString res, ErrorCodes err);
    void slot_tcp_con_finish(bool bsuccess);
    void slot_login_failed(int);

signals:
    void sig_switch_register();
    void sig_switch_reset();
    void sig_connect_tcp(ServerInfo);
};

#endif // LOGINDIALOG_H
