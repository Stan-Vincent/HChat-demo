
/******************************************************************************
 *
 * @file       logindialog.h
 * @brief      登录窗口
 *
 * @author     CEACI_XXL
 * @date       2026/09/21
 * @history
 *****************************************************************************/
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private:
    Ui::LoginDialog *ui;

signals:
    void switchRegister();
};

#endif // LOGINDIALOG_H
