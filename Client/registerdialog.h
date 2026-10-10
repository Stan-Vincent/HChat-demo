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
    void slot_reg_mod_finish(ReqId req_id, QString res ,ErrorCodes err);

    void on_sure_btn_clicked();

    void on_return_btn_clicked();

    void on_get_code_clicked();

    void on_cancel_btn_clicked();

private:
    Ui::RegisterDialog *ui;

    bool checkUserValid();
    bool checkEmailValid();
    bool checkPassValid();
    bool checkVarifyValid();
    bool checkConfirmValid();

    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);

    void ChangeTipPage();

    //初始化_handles
    void initHttpHandlers();

    //对 验证码信息Label 进行处理展示
    void showTip(QString str, bool b_ok);

    ///注册验证码对应的处理
    //initHttpHandlers()后，_handlers[ID_GET_VARIFY_CODE] --> 调用获取Json验证码数据并分析的函数
    QMap<ReqId ,std::function<void(const QJsonObject&)>> _handlers;

    QMap<TipErr, QString> _tip_errs;
    QTimer * _countdown_timer;
    int _countdown;
signals:
    void sig_switch_login();
};

#endif // REGISTERDIALOG_H
