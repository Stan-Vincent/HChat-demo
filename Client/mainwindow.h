#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logindialog.h"
#include "registerdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void SlotSwitchReg();
    void SlotSwitchLogin();

private:
    Ui::MainWindow *ui;
    LoginDialog *_login_dlg;    //登录窗口
    RegisterDialog *_reg_dlg;   //注册窗口
};
#endif // MAINWINDOW_H
