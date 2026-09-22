#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    ui->pass_edit->setEchoMode(QLineEdit::Password);
    ui->confirm_edit->setEchoMode(QLineEdit::Password);
    ui->err_tip->setProperty("state","normal");
    repolish(ui->err_tip);
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

