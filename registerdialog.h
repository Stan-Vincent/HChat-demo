#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

/*
 *      registerdialog.h
 *
 *      注册窗口
 */

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void on_grt_code_clicked(); //点击获取验证码按钮（自动连接信号与槽）

private:
    void showTip(QString str, bool b_ok); //验证码是否发送tip信息
    Ui::RegisterDialog *ui;
};

#endif // REGISTERDIALOG_H
