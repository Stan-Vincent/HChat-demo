#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "resetdialog.h"
#include "tcpmgr.h"
#include <QLayout>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    _ui_status = LOGIN_UI;

    // 1. 创建唯一的中心部件 QStackedWidget
    _stacked_widget = new QStackedWidget(this);
    setCentralWidget(_stacked_widget); // 只调用这一次 setCentralWidget

    // 2. 预分配所有轻量级窗口
    _login_dlg = new LoginDialog(this);
    _reg_dlg = new RegisterDialog(this);
    _reset_dlg = new ResetDialog(this);

    // 3. 统一设置无边框标志
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    _reg_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    _reset_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);

    // 4. 把窗口加入 QStackedWidget 的页面中
    _stacked_widget->addWidget(_login_dlg);
    _stacked_widget->addWidget(_reg_dlg);
    _stacked_widget->addWidget(_reset_dlg);

    // 5. 连接信号槽
    connect(_login_dlg, &LoginDialog::sig_switch_register, this, &MainWindow::slot_switch_register);
    connect(_login_dlg, &LoginDialog::sig_switch_reset, this, &MainWindow::slot_switch_reset);
    connect(_reg_dlg, &RegisterDialog::sig_switch_login, this, &MainWindow::slot_switch_login);
    connect(_reset_dlg, &ResetDialog::switchLogin, this, &MainWindow::slot_switch_login);

    // 6. 连接 TcpMgr 的三个信号（重构时最容易漏掉的就是这三条）
    //    注意 TcpMgr 侧的拼写是 sig_swich_chatdlg（少一个 s），别写成 switch
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_swich_chatdlg,
        this, &MainWindow::slot_switch_chatdlg);
    // 同账号异地登录被踢
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_notify_offline,
        this, &MainWindow::slot_offline);
    // 心跳超时 / 异常断开
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_connection_closed,
        this, &MainWindow::slot_connection_closed);

    // 7. 默认显示登录界面
    _stacked_widget->setCurrentWidget(_login_dlg);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slot_switch_login()
{
    _stacked_widget->setCurrentWidget(_login_dlg);
    _ui_status = LOGIN_UI;
}

void MainWindow::slot_switch_register()
{
    _stacked_widget->setCurrentWidget(_reg_dlg);
    _ui_status = REGISTER_UI;
}

void MainWindow::slot_switch_reset()
{
    _stacked_widget->setCurrentWidget(_reset_dlg);
    _ui_status = RESET_UI;
}

void MainWindow::slot_switch_chatdlg()
{
    if (_chat_dlg == nullptr) {
        _chat_dlg = new ChatDialog(this);
        // ChatDialog 现在是 QStackedWidget 的子页面，不是顶层窗口，
        // 所以不能再设 FramelessWindowHint —— 对子窗口设置窗口标志
        // 只会触发 Qt 的隐藏/重显，而且没有意义（无边框由 MainWindow 决定）。
        _stacked_widget->addWidget(_chat_dlg); // 第一次创建时加入堆叠部件
    }

    _stacked_widget->setCurrentWidget(_chat_dlg);

    // 聊天界面比登录页大，按屏幕可用区域自适应，避免最小高度超过屏幕被裁掉
    QSize avail = QApplication::primaryScreen()->availableGeometry().size();
    this->setMinimumSize(QSize(1050, qMin(900, avail.height() - 40)));
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    _ui_status = CHAT_UI;
    _chat_dlg->loadChatList();
}

void MainWindow::slot_offline(){
    //使用静态方法直接弹出一个信息框
    QMessageBox::information(this, "提示", "账号于其他设备登录");
    TcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::slot_connection_closed()
{
    // 使用静态方法直接弹出一个信息框
        QMessageBox::information(this, "下线提示", "心跳超时或临界异常，该终端下线！");
        TcpMgr::GetInstance()->CloseConnection();
        offlineLogin();
}


void MainWindow::offlineLogin(){
    if(_ui_status == LOGIN_UI){
        return;
    }

    // ★ 必须用 QStackedWidget 切页。
    //   原来这里写的是 setCentralWidget(_login_dlg)，会把 _stacked_widget 从
    //   中央部件的位置顶掉，之后 _stacked_widget->setCurrentWidget(_chat_dlg)
    //   就再也显示不出来（它已经不是中央部件了）——表现就是"登录过一次之后
    //   再也回不到聊天界面"。
    _stacked_widget->setCurrentWidget(_login_dlg);

    // 恢复登录页的窗口尺寸。注意必须同时改 minimum/maximum，
    // 只 resize 的话最小尺寸还停在聊天页的 1050x900，窗口会一直那么大。
    this->setMinimumSize(QSize(300, 500));
    this->setMaximumSize(QSize(300, 500));
    this->resize(300, 500);
    _ui_status = LOGIN_UI;
}


