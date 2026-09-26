#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //初加载 登录/注册 界面
    _login_dlg = new LoginDialog(this);
    _reg_dlg = new RegisterDialog(this);
    //登录窗口嵌入主窗口
    setCentralWidget(_login_dlg);

    //创建 switchRegister（改为注册信号）和 SlotSwitchReg（加载注册窗口）的连接
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);

    //无边框化 （为了嵌入主窗口）
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _reg_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _reg_dlg->hide();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::SlotSwitchReg()
{
    //注册窗口嵌入主窗口
    setCentralWidget(_reg_dlg);
    _login_dlg->hide();
    _reg_dlg->show();
}
