/******************************************************************************
 *
 * @file       logindialog.h
 * @brief      注册窗口
 *
 * @author     CEACI_XXL
 * @date       2026/09/21
 * @history
 *****************************************************************************/
#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include "global.h"

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
    void slot_reg_mod_finish(ReqId req_id, QString res ,ErrorCodes err);

private:
    void initHttpHandlers();
    void showTip(QString str, bool b_ok); //验证码是否发送tip信息
    Ui::RegisterDialog *ui;
    QMap<ReqId ,std::function<void(const QJsonObject&)>> _handlers; //对RegisterDialog注册消息处理
};

#endif // REGISTERDIALOG_H
