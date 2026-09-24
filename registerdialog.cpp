#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include "httpmgr.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    ui->pass_edit->setEchoMode(QLineEdit::Password);
    ui->confirm_edit->setEchoMode(QLineEdit::Password);
    ui->err_tip->setProperty("state","normal");
    repolish(ui->err_tip);
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_reg_mod_finish,this, &RegisterDialog::slot_reg_mod_finish);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::on_grt_code_clicked()
{
    auto email = ui->email_edit->text();

    //判断验证码格式是否符合规范 （正则表达式）
    QRegularExpression regex(R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
    bool match = regex.match(email.trimmed()).hasMatch();
    if(match){
        //发送验证码
        //待实现...

        showTip(tr("正在发送验证码"),true);
    }
    else{
        showTip(tr("邮箱地址错误"),false);
    }
}

void RegisterDialog::slot_reg_mod_finish(ReqId req_id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return ;
    }
    //解析json字符串 res 转化为 QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"),false);
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    //调用对应的逻辑 待完成...

    return;


}

void RegisterDialog::showTip(QString str,bool b_ok)
{
    if(b_ok){
        ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }
    ui->err_tip->setText(str);
    //刷新err_tip的样式表
    repolish(ui->err_tip);
}

