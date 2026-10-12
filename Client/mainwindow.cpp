#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "resetdialog.h"
#include "tcpmgr.h"
#include <QLayout>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QPainterPath>
#include <QRegion>
#include <QCursor>
#include <QAbstractButton>
#include <QComboBox>
#include <QAbstractSlider>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "logcategory.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    _ui_status = LOGIN_UI;

    // 1. 先建堆叠部件与各页（setupFrameless 需要把 _stacked_widget 装进圆角容器）
    _stacked_widget = new QStackedWidget(this);

    _login_dlg = new LoginDialog(this);
    _reg_dlg = new RegisterDialog(this);
    _reset_dlg = new ResetDialog(this);

    // ★ 注意：这三个对话框是 QStackedWidget 的子页，不是顶层窗口，
    //   对子部件设 FramelessWindowHint 没有任何效果（聊天页那段注释同理），故不再设置。

    _stacked_widget->addWidget(_login_dlg);
    _stacked_widget->addWidget(_reg_dlg);
    _stacked_widget->addWidget(_reset_dlg);

    // 2. 连接信号槽
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

    // 3. 无边框初始化（去系统边框 + 透明背景 + 圆角容器 + 阴影 + 标题栏 + 窗口按钮）
    setupFrameless();

    // 4. 默认显示登录界面
    _stacked_widget->setCurrentWidget(_login_dlg);

    // ★ 初始尺寸直接对齐 LoginDialog 的真实约束（见 logindialog.ui：
    //   min 360x500 / max 360x540），并加上圆角容器四周留白(kShadowMargin)。
    //   登录小窗不显示标题栏，所以不用额外加 kTitleBarHeight。
    QSize s = _login_dlg->minimumSize();
    this->setMinimumSize(windowSizeForContent(s, false));
    this->setMaximumSize(windowSizeForContent(_login_dlg->maximumSize(), false));
    this->resize(windowSizeForContent(s, false));
    updateWindowControls();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ============================================================
//  无边框窗口：去边框 + 透明背景 + 圆角容器 + 阴影 + 标题栏
// ============================================================
void MainWindow::setupFrameless()
{
    setWindowFlag(Qt::FramelessWindowHint);      // 去掉系统标题栏
    setAttribute(Qt::WA_TranslucentBackground);  // 背景透明，才能透出圆角+阴影

    // 中央透明留白层：四周留 kShadowMargin 给阴影显示
    QWidget *center = new QWidget(this);
    QVBoxLayout *cl = new QVBoxLayout(center);
    cl->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);
    cl->setSpacing(0);

    // 圆角白底容器：圆角遮罩 + 阴影都落在这层
    _main_widget = new QWidget(center);
    _main_widget->setObjectName("main_widget");
    _main_widget->setStyleSheet(
        QString("QWidget#main_widget{background:#ffffff;border-radius:%1px;}").arg(kCornerRadius));

    _shadow = new QGraphicsDropShadowEffect(_main_widget);
    _shadow->setBlurRadius(30);
    _shadow->setOffset(0, 0);
    _shadow->setColor(QColor(0, 0, 0, 55));
    _main_widget->setGraphicsEffect(_shadow);

    QVBoxLayout *ml = new QVBoxLayout(_main_widget);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);

    // 标题栏：顶部一条，承载窗口按钮；其空白区域可作拖拽热区
    _title_bar = new QWidget(_main_widget);
    _title_bar->setObjectName("title_bar");
    _title_bar->setFixedHeight(34);
    QHBoxLayout *tbl = new QHBoxLayout(_title_bar);
    tbl->setContentsMargins(10, 0, 10, 0);
    tbl->setSpacing(4);
    tbl->addStretch();

    auto makeBtn = [&](const QString &name, const QString &txt) {
        QPushButton *b = new QPushButton(txt, _title_bar);
        b->setObjectName(name);
        b->setFixedSize(34, 26);
        tbl->addWidget(b);
        return b;
    };
    _btn_min   = makeBtn("win_min",   "—");   // 最小化
    _btn_max   = makeBtn("win_max",   "□");   // 最大化/还原
    _btn_close = makeBtn("win_close", "×");   // 关闭

    connect(_btn_min,   &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(_btn_close, &QPushButton::clicked, this, &QWidget::close);
    connect(_btn_max,   &QPushButton::clicked, this, &MainWindow::toggleMaximize);

    // 默认先隐藏标题栏：登录/注册/重置小窗不需要顶部条，
    // 只在页面右上角浮一个关闭按钮（像 QQ 登录页）。
    _title_bar->setVisible(false);

    ml->addWidget(_title_bar);
    ml->addWidget(_stacked_widget, 1);        // 内容区占满剩余空间

    // 小窗右上角浮动关闭按钮（不放进 layout，避免占用标题栏高度）
    _btn_close_small = new QPushButton("×", _main_widget);
    _btn_close_small->setObjectName("win_close_small");
    _btn_close_small->setFixedSize(34, 26);
    _btn_close_small->setVisible(false);
    connect(_btn_close_small, &QPushButton::clicked, this, &QWidget::close);

    cl->addWidget(_main_widget);
    setCentralWidget(center);

    // 首次圆角遮罩延迟到首帧 geometry 就绪后再做（构造期 rect 可能还没算好）
    QTimer::singleShot(0, this, &MainWindow::applyRoundedMask);
}

void MainWindow::applyRoundedMask()
{
    if (_maximized) {
        _main_widget->clearMask();
        return;
    }
    QPainterPath path;
    path.addRoundedRect(_main_widget->rect(), kCornerRadius, kCornerRadius);
    _main_widget->setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void MainWindow::refreshChrome(bool maximized)
{
    _maximized = maximized;
    // 中央留白：最大化时归零（铺满屏幕、方角）
    if (auto *cl = qobject_cast<QVBoxLayout *>(centralWidget()->layout())) {
        int m = maximized ? 0 : kShadowMargin;
        cl->setContentsMargins(m, m, m, m);
    }
    _shadow->setEnabled(!maximized);
    _main_widget->setStyleSheet(
        QString("QWidget#main_widget{background:#ffffff;border-radius:%1px;}")
            .arg(maximized ? 0 : kCornerRadius));
    applyRoundedMask();
}

void MainWindow::toggleMaximize()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void MainWindow::updateWindowControls()
{
    bool chat = (_ui_status == CHAT_UI);
    // 聊天大窗：显示标题栏（含最小/最大/关闭）；小窗：隐藏标题栏，右上角浮一个关闭按钮
    _title_bar->setVisible(chat);
    _btn_min->setVisible(chat);
    _btn_max->setVisible(chat);
    _btn_close->setVisible(chat);
    _btn_close_small->setVisible(!chat);

    if (!chat && _btn_close_small && _main_widget) {
        // 定位到小窗页面右上角内侧（避开圆角遮罩裁剪）
        int x = qMax(0, _main_widget->width() - kShadowMargin - _btn_close_small->width());
        int y = kShadowMargin + 4;
        _btn_close_small->move(x, y);
        _btn_close_small->raise();
    }
}

QSize MainWindow::windowSizeForContent(const QSize &content, bool withTitleBar) const
{
    int m = _maximized ? 0 : kShadowMargin;
    int h = (withTitleBar && !_maximized) ? kTitleBarHeight : 0;
    return QSize(content.width() + 2 * m, content.height() + 2 * m + h);
}

bool MainWindow::isResizable() const
{
    // 只有聊天大窗可自由缩放；小窗固定尺寸，最大化时也不可拖拽边缘
    return !isMaximized() && _ui_status == CHAT_UI;
}

bool MainWindow::isInteractive(QWidget *w)
{
    // 命中这些交互控件时不启动窗口拖动，交给控件自身处理
    return w != nullptr &&
        (qobject_cast<QAbstractButton *>(w) ||
         qobject_cast<QLineEdit *>(w) ||
         qobject_cast<QComboBox *>(w) ||
         qobject_cast<QAbstractSlider *>(w));
}

void MainWindow::updateCursor(const QPoint &pos)
{
    if (!isResizable()) { unsetCursor(); return; }
    int t = 6;
    QRect r = rect();
    Qt::Edges ed;
    if (pos.x() < t)      ed |= Qt::LeftEdge;
    else if (pos.x() > r.width() - t)  ed |= Qt::RightEdge;
    if (pos.y() < t)      ed |= Qt::TopEdge;
    else if (pos.y() > r.height() - t) ed |= Qt::BottomEdge;

    Qt::Edges eF = Qt::LeftEdge  | Qt::TopEdge;    // 一条对角线
    Qt::Edges eB = Qt::RightEdge | Qt::TopEdge;    // 另一条对角线
    if (ed == Qt::Edges(Qt::LeftEdge) || ed == Qt::Edges(Qt::RightEdge))
        setCursor(Qt::SizeHorCursor);
    else if (ed == Qt::Edges(Qt::TopEdge) || ed == Qt::Edges(Qt::BottomEdge))
        setCursor(Qt::SizeVerCursor);
    else if (ed == eF || ed == (Qt::Edges(Qt::RightEdge) | Qt::BottomEdge))
        setCursor(Qt::SizeFDiagCursor);
    else if (ed == eB || ed == (Qt::Edges(Qt::LeftEdge) | Qt::BottomEdge))
        setCursor(Qt::SizeBDiagCursor);
    else
        unsetCursor();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        int t = 6;
        QRect r = rect();
        Qt::Edges ed;
        if (pos.x() < t)      ed |= Qt::LeftEdge;
        else if (pos.x() > r.width() - t)  ed |= Qt::RightEdge;
        if (pos.y() < t)      ed |= Qt::TopEdge;
        else if (pos.y() > r.height() - t) ed |= Qt::BottomEdge;

        // 可缩放窗口 + 贴近边缘 -> 进入自写边缘缩放（Qt 6.5.3 MinGW 未提供 startSystemResize）
        if (ed != Qt::Edges() && isResizable()) {
            _resizing = true;
            _resize_edges = ed;
            _press_pos = event->globalPosition().toPoint();
            _press_geom = frameGeometry();
            return;
        }

        QWidget *w = childAt(pos);
        if (isInteractive(w)) {
            _drag_start = QPoint();   // 命中交互控件：不拖动
        } else {
            // 记录按下点与窗口左上角的偏移，move 时复用
            _drag_start = event->globalPosition().toPoint() - frameGeometry().topLeft();
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (_resizing) {
        // 手动边缘缩放：按命中的边移动对应边界，同时限制最小尺寸
        QPoint delta = event->globalPosition().toPoint() - _press_pos;
        QRect g = _press_geom;
        if (_resize_edges & Qt::LeftEdge)   g.setLeft(g.left() + delta.x());
        if (_resize_edges & Qt::RightEdge)  g.setRight(g.right() + delta.x());
        if (_resize_edges & Qt::TopEdge)    g.setTop(g.top() + delta.y());
        if (_resize_edges & Qt::BottomEdge) g.setBottom(g.bottom() + delta.y());

        if (g.width() < minimumWidth())   g.setWidth(minimumWidth());
        if (g.height() < minimumHeight()) g.setHeight(minimumHeight());
        setGeometry(g);
        return;
    }
    if (!_drag_start.isNull() && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - _drag_start);
        return;
    }
    updateCursor(event->pos());
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    _drag_start = QPoint();
    _resizing = false;
    _resize_edges = Qt::Edges();
    _press_pos = QPoint();
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    bool max = isMaximized();
    if (max != _maximized)
        refreshChrome(max);   // 进/出最大化时切换圆角/阴影/留白
    applyRoundedMask();       // 窗口尺寸变了，圆角遮罩要重算

    // 小窗关闭按钮随窗口尺寸重定位
    if (_btn_close_small && _btn_close_small->isVisible() && _main_widget) {
        int x = qMax(0, _main_widget->width() - kShadowMargin - _btn_close_small->width());
        int y = kShadowMargin + 4;
        _btn_close_small->move(x, y);
    }
}

// ============================================================
//  页面切换（统一用 windowSizeForContent 加留白，并刷新窗口按钮）
// ============================================================
void MainWindow::slot_switch_login()
{
    _stacked_widget->setCurrentWidget(_login_dlg);
    // 窗口尺寸对齐 LoginDialog（min 360x500 / max 360x540），保证左右边距对称居中。
    // 登录小窗隐藏标题栏，因此尺寸中不加 kTitleBarHeight。
    QSize s = _login_dlg->minimumSize();
    this->setMinimumSize(windowSizeForContent(s, false));
    this->setMaximumSize(windowSizeForContent(_login_dlg->maximumSize(), false));
    this->resize(windowSizeForContent(s, false));
    updateWindowControls();
    _ui_status = LOGIN_UI;
}

void MainWindow::slot_switch_register()
{
    _stacked_widget->setCurrentWidget(_reg_dlg);
    // 对齐 RegisterDialog（min/max 362x500），避免登录框 360 比注册框窄 2px 被裁
    QSize s = _reg_dlg->minimumSize();
    this->setMinimumSize(windowSizeForContent(s, false));
    this->setMaximumSize(windowSizeForContent(_reg_dlg->maximumSize(), false));
    this->resize(windowSizeForContent(s, false));
    updateWindowControls();
    _ui_status = REGISTER_UI;
}

void MainWindow::slot_switch_reset()
{
    _stacked_widget->setCurrentWidget(_reset_dlg);
    // 对齐 ResetDialog（min/max 360x520）
    QSize s = _reset_dlg->minimumSize();
    this->setMinimumSize(windowSizeForContent(s, false));
    this->setMaximumSize(windowSizeForContent(_reset_dlg->maximumSize(), false));
    this->resize(windowSizeForContent(s, false));
    updateWindowControls();
    _ui_status = RESET_UI;
}

void MainWindow::slot_switch_chatdlg()
{
    if (_chat_dlg == nullptr) {
        _chat_dlg = new ChatDialog(this);
        // ★ 关键：_chat_dlg 在这里才第一次被创建，信号连接必须放这之后。
        connect(_chat_dlg, &ChatDialog::sig_logout, this, &MainWindow::slot_logout);
        _stacked_widget->addWidget(_chat_dlg); // 第一次创建时加入堆叠部件
    }

    _stacked_widget->setCurrentWidget(_chat_dlg);

    QSize avail = QApplication::primaryScreen()->availableGeometry().size();
    // ★ 窗口改为可自由调整大小 / 最大化：
    //   - 只保留一个"最小可用尺寸"
    //   - 不再设 maximumSize，Qt 默认就能放大和最大化
    //   - 初始尺寸比最小值略大一点，进去时不会顶满屏
    const int min_w = 900;
    const int min_h = 600;
    // 聊天大窗显示标题栏，尺寸中额外加上 kTitleBarHeight
    this->setMinimumSize(windowSizeForContent(QSize(min_w, qMin(min_h, avail.height() - 40)), true));
    // ★ 解除登录/注册/重置页给 MainWindow 设的 maximumSize 上限，否则退出登录再登录
    //   后聊天窗口会被锁在 360 宽（maximumSize 仍残留登录框的 360）。
    this->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    this->resize(windowSizeForContent(QSize(qMin(1080, avail.width() - 80),
                                            qMin(680, avail.height() - 120)), true));
    updateWindowControls();
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
    _login_dlg->resetUIState();

    if(_ui_status == LOGIN_UI){
        return;
    }

    // ★ 必须用 QStackedWidget 切页。
    _stacked_widget->setCurrentWidget(_login_dlg);

    // 恢复登录页的窗口尺寸。注意必须同时改 minimum/maximum，
    // 只 resize 的话最小尺寸还停在聊天页的 900x600，窗口会一直那么大。
    // ★ 尺寸必须匹配 LoginDialog 的真实约束（min 360x500 / max 360x540）。
    //   登录小窗隐藏标题栏，不加 kTitleBarHeight。
    QSize s = _login_dlg->minimumSize();
    this->setMinimumSize(windowSizeForContent(s, false));
    this->setMaximumSize(windowSizeForContent(_login_dlg->maximumSize(), false));
    this->resize(windowSizeForContent(s, false));
    updateWindowControls();
    _ui_status = LOGIN_UI;
}
