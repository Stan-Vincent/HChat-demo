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
