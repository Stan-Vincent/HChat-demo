#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "resetdialog.h"
#include "tcpmgr.h"
#include <QLayout>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include "logcategory.h"

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

    // ★ 聊天窗口的「退出登录」信号不在这里连：构造期 _chat_dlg 还是 nullptr，
    //   连了也是空操作（Qt 报 "cannot connect to a null pointer" 且不生效），
    //   会导致登出切不回登录页。真正的连接放到 slot_switch_chatdlg 里
    //   _chat_dlg 第一次被 new 出来之后。

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
        // ★ 关键：_chat_dlg 在这里才第一次被创建，信号连接必须放这之后。
        //   构造期连（当时为 nullptr）是空操作，登出就会"什么都没发生"。
        connect(_chat_dlg, &ChatDialog::sig_logout, this, &MainWindow::slot_logout);
        // ChatDialog 现在是 QStackedWidget 的子页面，不是顶层窗口，
        // 所以不能再设 FramelessWindowHint —— 对子窗口设置窗口标志
        // 只会触发 Qt 的隐藏/重显，而且没有意义（无边框由 MainWindow 决定）。
        _stacked_widget->addWidget(_chat_dlg); // 第一次创建时加入堆叠部件
    }

    _stacked_widget->setCurrentWidget(_chat_dlg);

    QSize avail = QApplication::primaryScreen()->availableGeometry().size();
    // ★ 窗口改为可自由调整大小 / 最大化：
    //   - 只保留一个"最小可用尺寸"，防止控件被压到看不见
    //   - 不再设 maximumSize，Qt 默认就能放大和最大化
    //   - 初始尺寸比最小值略大一点，进去时不会顶满屏
    const int min_w = 900;
    const int min_h = 600;
    this->setMinimumSize(QSize(min_w, qMin(min_h, avail.height() - 40)));
    this->resize(qMin(1080, avail.width() - 80), qMin(680, avail.height() - 120));
    _ui_status = CHAT_UI;
    _chat_dlg->loadChatList();
}

void MainWindow::slot_logout()
{
	qCDebug(lcChat) << "MainWindow::slot_logout";
	// 主动退出登录的用户不该再被自动登录拉回来
	_login_dlg->SetAutoLogin(false);
	offlineLogin();
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
    // ★ 每次回到登录页都先复位它的 UI 状态：清掉"正在登录..."提示、恢复按钮可用。
    //   否则会停在"聊天服务连接成功，正在登录..."且按钮被禁用，界面无法使用。
    //   必须放在 LOGIN_UI 守卫之前——因为心跳超时那条路径可能先一步把状态切成
    //   LOGIN_UI，等 sig_logout 再到 offlineLogin 时如果先看守卫就直接 return，
    //   复位就跳过了。
    _login_dlg->resetUIState();

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
    // ★ 尺寸必须匹配 LoginDialog 的真实约束（minimumSize 340x490、
    //   maximumSize 340x532，见 logindialog.ui）。原来这里写死 300x500，
    //   比登录框需要的 340 宽还小 40px，退出登录后登录框右侧被裁掉，
    //   表现为"登录窗口显示不完全"。
    this->setMinimumSize(QSize(340, 490));
    this->setMaximumSize(QSize(340, 532));
    this->resize(340, 500);
    _ui_status = LOGIN_UI;
}


