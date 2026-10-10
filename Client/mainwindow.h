#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logindialog.h"
#include "registerdialog.h"
#include "resetdialog.h"
#include "chatdialog.h"
#include <QStackedWidget>

namespace Ui {
class MainWindow;
}

//UI状态
enum UIStatus{
    LOGIN_UI,
    REGISTER_UI,
    RESET_UI,
    CHAT_UI
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void offlineLogin();

public slots:
    void slot_switch_register();
    void slot_switch_login();
    void slot_switch_reset();
    void slot_switch_chatdlg();
    void slot_offline();
    void slot_connection_closed();
    // ★ 聊天窗口发来的「退出登录」
    void slot_logout();

private:
    Ui::MainWindow *ui;

    QStackedWidget *_stacked_widget = nullptr;
    LoginDialog *_login_dlg = nullptr;
    RegisterDialog *_reg_dlg = nullptr;
    ResetDialog *_reset_dlg = nullptr;
    ChatDialog *_chat_dlg = nullptr;


    UIStatus _ui_status;
};

#endif // MAINWINDOW_H
