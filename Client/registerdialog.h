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

    void on_sure_btn_clicked();

private:
    Ui::RegisterDialog *ui;

    //初始化_handles
    void initHttpHandlers();

    //对 验证码信息Label 进行处理展示
    void showTip(QString str, bool b_ok);

    ///注册验证码对应的处理
    //initHttpHandlers()后，_handlers[ID_GET_VARIFY_CODE] --> 调用获取Json验证码数据并分析的函数
    QMap<ReqId ,std::function<void(const QJsonObject&)>> _handlers;
};

#endif // REGISTERDIALOG_H
