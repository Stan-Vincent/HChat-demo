#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "resetdialog.h"
#include "tcpmgr.h"
#include "usermgr.h"
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
#include <QSettings>
#include <QCloseEvent>
#include <QGuiApplication>
#include "logcategory.h"
#include "animutil.h"
#include "toast.h"
#include <QLabel>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QParallelAnimationGroup>

// ============================================================
//  NetStatusBanner：断线重连提示条
//
//  为什么不用 Toast：Toast 到点自动消失，而重连要持续几秒到几十秒，
//  中间还得反复更新"正在重连 (2/5)"。所以做一个常驻的顶部细条：
//    - showMessage()：滑入并显示（autoHideMs>0 则到时自动滑出）
//    - setText()：原地更新文案（宽度重算 + 重新定位），不重新播动画
//    - dismiss()：滑出并隐藏
//  视觉上与 Toast 同一套配色（左侧 accent 竖条 + 白底圆角），
//  挂在 main_widget 顶部内侧，跟着窗口 resize 重新定位。
// ============================================================
class NetStatusBanner : public QWidget
{
public:
    enum Level { Info = 0, Success = 1, Warning = 2, Error = 3 };

    explicit NetStatusBanner(QWidget *parent)
        : QWidget(parent), _level(Info), _auto_hide_ms(0), _anim_out(false)
    {
        setObjectName("net_status_banner");
        setAttribute(Qt::WA_TranslucentBackground, true);
        // 不抢焦点、不挡交互：用户重连期间照样能点界面
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        hide();

        _label = new QLabel(this);
        _label->setObjectName("net_banner_label");
        _label->setTextFormat(Qt::PlainText);
        _label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        _label->setStyleSheet(
            "QLabel#net_banner_label{color:#1f1f1f;font-size:12px;"
            "background:transparent;border:none;}");

        _anim = new QPropertyAnimation(this, "pos", this);
        _anim->setDuration(200);
        _anim->setEasingCurve(QEasingCurve::OutCubic);
    }

    // 显示/更新一条提示。sameLevel 且已显示时只换文案，不重播动画
    // （重连过程中每次重试都调它，重播动画会闪）。
    void showMessage(const QString &text, Level level, int autoHideMs = 0)
    {
        _level = level;
        _auto_hide_ms = autoHideMs;
        _anim->stop();
        _anim_out = false;

        QString accent;
        switch (level) {
        case Success: accent = "#34c759"; break;
        case Warning: accent = "#C9913B"; break;
        case Error:   accent = "#f14e4e"; break;
        default:      accent = "#4C5FD7"; break;
        }
        setStyleSheet(QString(
            "QWidget#net_status_banner{background:rgba(255,255,255,0.97);"
            "border-radius:8px;border-left:3px solid %1;}").arg(accent));

        _label->setText(text);
        _label->adjustSize();

        const int w = qMin(_label->width() + 26, 460);
        const int h = 30;
        setFixedSize(w, h);
        _label->setGeometry(13, 0, w - 18, h);

        if (!isVisible()) {
            // 从父控件顶部外侧滑入
            QWidget *p = parentWidget();
            if (p) {
                int x = qMax(0, (p->width() - w) / 2);
                move(x, -h - 4);
                show();
                raise();
                _anim->setStartValue(QPoint(qMax(0, (p->width() - w) / 2), -h - 4));
                _anim->setEndValue(QPoint(x, kTopMargin));
                _anim->start();
            }
        } else {
            reposition();
            raise();
        }

        if (_auto_hide_ms > 0) {
            QTimer::singleShot(_auto_hide_ms, this, [this]() {
                // 期间文案可能又被更新过（新的常驻提示），别把新提示收掉
                if (!_anim_out && _auto_hide_ms > 0) {
                    dismiss();
                }
            });
        }
    }

    void dismiss()
    {
        if (!isVisible() || _anim_out) {
            return;
        }
        _anim_out = true;
        _auto_hide_ms = 0;
        _anim->stop();
        QPoint from = pos();
        _anim->setStartValue(from);
        _anim->setEndValue(QPoint(from.x(), -height() - 4));
        connect(_anim, &QPropertyAnimation::finished, this, [this]() {
            hide();
            _anim_out = false;
        }, Qt::UniqueConnection);
        _anim->start();
    }

    // 父控件尺寸变化时重新水平居中 + 贴顶
    void reposition()
    {
        QWidget *p = parentWidget();
        if (!p) {
            return;
        }
        int x = qMax(0, (p->width() - width()) / 2);
        move(x, _anim_out ? -height() - 4 : kTopMargin);
    }

private:
    static constexpr int kTopMargin = 8;   // 距 main_widget 顶部的间距
    QLabel *_label = nullptr;
    Level _level;
    int _auto_hide_ms;
    bool _anim_out;
    QPropertyAnimation *_anim = nullptr;
};


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
    // 断线自动重连：TcpMgr 负责退避重试，这里只更新提示条和善后
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_reconnect_retry,
        this, &MainWindow::slot_reconnect_retry);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_recon_success,
        this, &MainWindow::slot_recon_success);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_reconnect_failed,
        this, &MainWindow::slot_reconnect_failed);

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
    // ★ 恢复上次的位置：登录小窗尺寸是固定的（min=max 约束），这里主要恢复【位置】。
    //   没存过（首次启动）就保持上面的默认尺寸。
    restoreWindowState(LOGIN_UI);
    updateWindowControls();

    // ★ 窗口构造时 150ms 透明度淡入 + 轻微缩放（从 0.95 放大到 1.0）
    AnimUtil::fadeInWindow(this, 150, 0.95);
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
    applyMainWidgetBackground();   // 按初始 _ui_status(=LOGIN_UI) 套登录态渐变背景

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
    tbl->setContentsMargins(10, 0, 4, 0);   // 右边留 4px，避免圆角遮罩裁到按钮
    tbl->setSpacing(0);                      // 按钮紧挨着，像 QQ 一样
    tbl->addStretch();

    auto makeBtn = [&](const QString &name, const QString &txt) {
        QPushButton *b = new QPushButton(txt, _title_bar);
        b->setObjectName(name);
        b->setFixedSize(30, 30);             // 正方形按钮，与 QQ 一致
        tbl->addWidget(b);
        return b;
    };
    _btn_min   = makeBtn("win_min",   "−");   // 最小化（用减号，居中显示）
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

    // 小窗右上角浮动按钮：最小化 + 关闭（不放进 layout，避免占用标题栏高度）
    _btn_min_small = new QPushButton("−", _main_widget);
    _btn_min_small->setObjectName("win_min_small");
    _btn_min_small->setFixedSize(30, 30);
    _btn_min_small->setVisible(false);
    connect(_btn_min_small, &QPushButton::clicked, this, &QWidget::showMinimized);

    _btn_close_small = new QPushButton("×", _main_widget);
    _btn_close_small->setObjectName("win_close_small");
    _btn_close_small->setFixedSize(30, 30);
    _btn_close_small->setVisible(false);
    connect(_btn_close_small, &QPushButton::clicked, this, &QWidget::close);

    // ★ 断线重连提示条：挂在 _main_widget 上（跟着它 resize 重新定位）。
    //   必须在 _main_widget 建好之后创建。
    _net_banner = new NetStatusBanner(_main_widget);

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
    applyMainWidgetBackground();   // 重新套用当前 UI 态背景（最大化时圆角归零）
    applyRoundedMask();
}

void MainWindow::applyMainWidgetBackground()
{
    // 登录/注册/重置小窗：淡橙渐变背景；聊天大窗：纯白（承载聊天界面）。
    // 最大化时圆角归零（铺满屏幕、方角），否则 16px 圆角。
    int r = _maximized ? 0 : kCornerRadius;
    QString bg;
    if (_ui_status == CHAT_UI) {
        bg = "#ffffff";
    } else {
        bg = "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #F0F2FD, stop:1 #E3E7FA)";
    }
    _main_widget->setStyleSheet(
        QString("QWidget#main_widget{background:%1;border-radius:%2px;}")
            .arg(bg).arg(r));
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
    // 聊天大窗：显示标题栏（含最小/最大/关闭）；小窗：隐藏标题栏，右上角浮最小化+关闭
    _title_bar->setVisible(chat);
    _btn_min->setVisible(chat);
    _btn_max->setVisible(chat);
    _btn_close->setVisible(chat);
    _btn_min_small->setVisible(!chat);
    _btn_close_small->setVisible(!chat);

    // 延迟到首帧 layout 完成后再定位，避免 _main_widget 宽度还是 0 时算到左上角
    if (!chat)
        QTimer::singleShot(0, this, &MainWindow::repositionSmallControls);

    // 切页时同步切换 main_widget 背景（登录态渐变 / 聊天态白）
    applyMainWidgetBackground();
}

void MainWindow::repositionSmallControls()
{
    if (!_btn_min_small || !_btn_close_small || !_main_widget)
        return;
    if (!_btn_close_small->isVisible())
        return;

    // 按钮放在 _main_widget 右上角内侧，留出 kShadowMargin 避开圆角遮罩裁剪
    int right = qMax(0, _main_widget->width() - kShadowMargin);
    int y = kShadowMargin + 2;    // 30x30 按钮，y>16 可避开顶部圆角裁剪
    _btn_close_small->move(right - _btn_close_small->width(), y);
    _btn_close_small->raise();
    _btn_min_small->move(right - _btn_close_small->width() - _btn_min_small->width(), y);
    _btn_min_small->raise();
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
    repositionSmallControls(); // 小窗右上角按钮随窗口尺寸重定位
    if (_net_banner) {
        _net_banner->reposition();   // 重连提示条跟着重新居中贴顶
    }
}

// ============================================================
//  页面切换（统一用 windowSizeForContent 加留白，并刷新窗口按钮）
// ============================================================
void MainWindow::slot_switch_login()
{
    // ★ 先用 _stacked_widget 做 200ms 交叉淡入（淡出→切页+改尺寸→淡入）。
    _ui_status = LOGIN_UI;
    QSize s = _login_dlg->minimumSize();
    AnimUtil::crossFadeStacked(_stacked_widget, _login_dlg, [this, s]() {
        // 窗口尺寸对齐 LoginDialog（min 360x500 / max 360x540），保证左右边距对称居中。
        // 登录小窗隐藏标题栏，因此尺寸中不加 kTitleBarHeight。
        this->setMinimumSize(windowSizeForContent(s, false));
        this->setMaximumSize(windowSizeForContent(_login_dlg->maximumSize(), false));
        this->resize(windowSizeForContent(s, false));
        updateWindowControls();
    }, 200);
}

void MainWindow::slot_switch_register()
{
    // ★ 交叉淡入到注册页
    _ui_status = REGISTER_UI;
    QSize s = _reg_dlg->minimumSize();
    AnimUtil::crossFadeStacked(_stacked_widget, _reg_dlg, [this, s]() {
        // 对齐 RegisterDialog（min/max 362x500），避免登录框 360 比注册框窄 2px 被裁
        this->setMinimumSize(windowSizeForContent(s, false));
        this->setMaximumSize(windowSizeForContent(_reg_dlg->maximumSize(), false));
        this->resize(windowSizeForContent(s, false));
        updateWindowControls();
    }, 200);
}

void MainWindow::slot_switch_reset()
{
    // ★ 交叉淡入到重置页
    _ui_status = RESET_UI;
    QSize s = _reset_dlg->minimumSize();
    AnimUtil::crossFadeStacked(_stacked_widget, _reset_dlg, [this, s]() {
        // 对齐 ResetDialog（min/max 360x520）
        this->setMinimumSize(windowSizeForContent(s, false));
        this->setMaximumSize(windowSizeForContent(_reset_dlg->maximumSize(), false));
        this->resize(windowSizeForContent(s, false));
        updateWindowControls();
    }, 200);
}

void MainWindow::slot_switch_chatdlg()
{
    if (_chat_dlg == nullptr) {
        _chat_dlg = new ChatDialog(this);
        // ★ 关键：_chat_dlg 在这里才第一次被创建，信号连接必须放这之后。
        connect(_chat_dlg, &ChatDialog::sig_logout, this, &MainWindow::slot_logout);
        _stacked_widget->addWidget(_chat_dlg); // 第一次创建时加入堆叠部件
    }

    // ★ 把【本次登录】账号的数据灌进常驻页面：ChatDialog 只在首次登录 new 一次，
    //   侧边栏头像 / 个人信息页 / 好友申请页都在构造时填过值，
    //   不重新灌的话换号登录后显示的还是上一个账号。
    //   此时 TcpMgr 的登录回包已经把 UserMgr 填好了，可以安全取用。
    _chat_dlg->applyLoginSession();

    // ★ 交叉淡入到聊天页（聊天页首次创建时尺寸跳变大，恰好在淡出→淡入的瞬间完成，看不出突兀）
    _ui_status = CHAT_UI;
    QSize avail = QApplication::primaryScreen()->availableGeometry().size();
    // ★ 窗口改为可自由调整大小 / 最大化：
    //   - 只保留一个"最小可用尺寸"
    //   - 不再设 maximumSize，Qt 默认就能放大和最大化
    //   - 初始尺寸比最小值略大一点，进去时不会顶满屏
    const int min_w = 900;
    const int min_h = 600;
    AnimUtil::crossFadeStacked(_stacked_widget, _chat_dlg, [this, avail, min_w, min_h]() {
        // 聊天大窗显示标题栏，尺寸中额外加上 kTitleBarHeight
        this->setMinimumSize(windowSizeForContent(QSize(min_w, qMin(min_h, avail.height() - 40)), true));
        // ★ 解除登录/注册/重置页给 MainWindow 设的 maximumSize 上限，否则退出登录再登录
        //   后聊天窗口会被锁在 360 宽（maximumSize 仍残留登录框的 360）。
        this->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        // ★ 优先恢复上次聊天窗口的尺寸+位置（含"上次是最大化"），
        //   没存过（首次登录）才用默认的 1080x680 居中尺寸。
        if (!restoreWindowState(CHAT_UI)) {
            this->resize(windowSizeForContent(QSize(qMin(1080, avail.width() - 80),
                                                    qMin(680, avail.height() - 120)), true));
        }
        updateWindowControls();
        if (_chat_dlg) _chat_dlg->loadChatList();
    }, 200);
}

void MainWindow::slot_logout()
{
	qCDebug(lcChat) << "MainWindow::slot_logout";
	// 主动退出登录的用户不该再被自动登录拉回来
	_login_dlg->SetAutoLogin(false);
	offlineLogin();
}

void MainWindow::slot_offline(){
    // 同账号异地登录被踢：右上角 Toast 提示（自动消失、不阻塞），随后回到登录页
    Toast::showToast(this, "账号于其他设备登录", Toast::Warning);
    TcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::slot_connection_closed()
{
    // 被动断开（服务端掐线 / 网络抖动 / 心跳超时）。
    //
    // ★ 行为变更（2026-10-16）：以前这里直接 Toast + 回登录页，
    //   用户网络抖一下就被踢回登录页、要重新输密码，体验很差。
    //   现在 TcpMgr 已经启动自动重连（见它的 disconnected 处理器），
    //   这里只负责提示：能重连就挂着"正在重连"横幅等结果，
    //   不能重连（还没登录成功、手里没有 token）才走原来的回登录页流程。
    if (TcpMgr::GetInstance()->IsReconnecting()) {
        qCDebug(lcChat) << "connection closed, waiting for auto reconnect";
        _net_banner->showMessage(tr("网络连接已断开，正在重连…"),
                                 NetStatusBanner::Warning);
        return;
    }

    // 登录页阶段就断了（还没拿到 token）：保持原有行为
    Toast::showToast(this, "心跳超时或临界异常，该终端下线！", Toast::Error);
    TcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::slot_reconnect_retry(int attempt, int total)
{
    // attempt 从 1 开始；0 表示"即将开始第 1 次"
    const int shown = (attempt <= 0) ? 1 : attempt;
    _net_banner->showMessage(tr("网络连接已断开，正在重连（%1/%2）…")
                                 .arg(shown).arg(total),
                             NetStatusBanner::Warning);
}

void MainWindow::slot_recon_success()
{
    qCDebug(lcChat) << "reconnect success, refresh chat data";
    _net_banner->showMessage(tr("连接已恢复"), NetStatusBanner::Success, 2500);
    Toast::showToast(this, "网络已恢复", Toast::Success, 2000);

    // 断线期间服务端往旧 session 推的消息全丢了，好友/申请也可能变了：
    // 让 ChatDialog 把会话列表整体重拉一遍（它内部带去重，不会渲染两遍）。
    if (_chat_dlg) {
        _chat_dlg->refreshAfterReconnect();
    }
}

void MainWindow::slot_reconnect_failed()
{
    qCDebug(lcChat) << "reconnect failed, back to login page";
    _net_banner->showMessage(tr("重连失败，请重新登录"), NetStatusBanner::Error);
    Toast::showToast(this, "网络恢复失败，请重新登录", Toast::Error);
    offlineLogin();
}


// ============================================================
//  窗口几何持久化（QSettings，存到用户目录 Hcat/Hcat.ini）
// ============================================================
static QSettings windowSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QString("Hcat"), QString("Hcat"));
}

void MainWindow::saveWindowState()
{
    QSettings st = windowSettings();
    st.beginGroup(QString("Window"));
    if (_ui_status == CHAT_UI) {
        // ★ saveGeometry() 本身已经编码了"是否最大化"，restoreGeometry() 会一并还原
        st.setValue(QString("chatGeometry"), saveGeometry());
        st.setValue(QString("chatMaximized"), isMaximized());
    } else {
        // 登录/注册/重置小窗：尺寸被 min=max 约束锁死，记位置即可
        st.setValue(QString("loginGeometry"), saveGeometry());
    }
    st.endGroup();
    st.sync();
}

bool MainWindow::restoreWindowState(UIStatus status)
{
    QSettings st = windowSettings();
    st.beginGroup(QString("Window"));
    QByteArray geo = st.value(status == CHAT_UI ? QString("chatGeometry")
                                                : QString("loginGeometry")).toByteArray();
    bool maximized = status == CHAT_UI ? st.value(QString("chatMaximized"), false).toBool() : false;
    st.endGroup();

    if (geo.isEmpty()) {
        return false;                     // 首次启动，没存过
    }
    if (!restoreGeometry(geo)) {
        return false;                     // 数据损坏/版本不兼容
    }
    // ★ 换过显示器 / 改过分辨率：窗口可能整个跑到屏幕外，拉回可见区域
    if (!isOnAnyScreen(this->frameGeometry())) {
        QRect avail = QApplication::primaryScreen()->availableGeometry();
        this->move(avail.x() + (avail.width()  - this->width())  / 2,
                   avail.y() + (avail.height() - this->height()) / 2);
    }
    // 聊天大窗：上次是最大化就恢复最大化（resizeEvent 里会自动 refreshChrome 去圆角）
    if (status == CHAT_UI && maximized && !isMaximized()) {
        showMaximized();
    }
    return true;
}

bool MainWindow::isOnAnyScreen(const QRect &r) const
{
    // 至少要和某块屏幕有可见重叠，否则认为"跑飞了"
    for (const QScreen *s : QGuiApplication::screens()) {
        if (s == nullptr) {
            continue;
        }
        if (s->availableGeometry().intersects(r)) {
            return true;
        }
    }
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // ★ 退出前存一次：下次启动窗口出现在上次的位置、上次的尺寸
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow::offlineLogin(){
	// ★ 退出登录前先把【聊天大窗】的几何存下来：
	//   接下来窗口会缩回登录小窗，等关闭时再存就只剩小窗的位置了。
	if (_ui_status == CHAT_UI) {
		saveWindowState();
	}

	// ★ 回登录页时收掉重连提示条：要么已经失败（该条已显示"重连失败"），
	//   要么是用户主动登出，都不该再把"正在重连"挂在屏幕上。
	if (_net_banner) {
		_net_banner->dismiss();
	}

	// ★ 每次回到登录页都先复位它的 UI 状态：清掉"正在登录..."提示、恢复按钮可用。
	_login_dlg->resetUIState();

	// ★ 关掉聊天页可能残留的模态 LoadingDlg 并把加载计数归零，
	//   否则被中断的聊天列表/消息加载会把模态框挂在屏幕上、阻塞整个 ChatDialog
	//   （连备注修改按钮都点不动），下次登录再 +1 又无限阻塞。
	// ★ 主动登出也要断开 TCP：连接绑定的是上一个账号的 token，不断开的话
	//   下一个账号会复用旧连接，服务端仍认为上一账号在线。
	//   CloseConnection 内部会标记"主动关闭"，断开后不会再弹心跳超时 Toast。
	TcpMgr::GetInstance()->CloseConnection();

	// ★ 清掉单例里上一个账号的全部数据（UserMgr 跨登录存活，不清则换号全是旧信息）。
	UserMgr::GetInstance()->Clear();

	// ★ 清掉 ChatDialog 的 UI 状态（内部会顺带调 resetLoadingState）。
	if (_chat_dlg) {
		_chat_dlg->clearSessionState();
	}

    if(_ui_status == LOGIN_UI){
        return;
    }

    // ★ 必须用 QStackedWidget 切页，并做交叉淡入。
    _ui_status = LOGIN_UI;
    QSize s = _login_dlg->minimumSize();
    AnimUtil::crossFadeStacked(_stacked_widget, _login_dlg, [this, s]() {
        // 恢复登录页的窗口尺寸。注意必须同时改 minimum/maximum，
        // 只 resize 的话最小尺寸还停在聊天页的 900x600，窗口会一直那么大。
        // ★ 尺寸必须匹配 LoginDialog 的真实约束（min 360x500 / max 360x540）。
        //   登录小窗隐藏标题栏，不加 kTitleBarHeight。
        this->setMinimumSize(windowSizeForContent(s, false));
        this->setMaximumSize(windowSizeForContent(_login_dlg->maximumSize(), false));
        this->resize(windowSizeForContent(s, false));
        updateWindowControls();
    }, 200);
}
