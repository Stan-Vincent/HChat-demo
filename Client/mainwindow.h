#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGraphicsDropShadowEffect>
#include <QPushButton>
#include <QWidget>

#include "logindialog.h"
#include "registerdialog.h"
#include "resetdialog.h"
#include "chatdialog.h"
#include <QStackedWidget>

namespace Ui {
class MainWindow;
}

//UI状态
enum UIStatus{
    LOGIN_UI,
    REGISTER_UI,
    RESET_UI,
    CHAT_UI
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void offlineLogin();

public slots:
    void slot_switch_register();
    void slot_switch_login();
    void slot_switch_reset();
    void slot_switch_chatdlg();
    void slot_offline();
    void slot_connection_closed();
    // ★ 聊天窗口发来的「退出登录」
    void slot_logout();

protected:
    // ★ 无边框窗口：自己接管鼠标拖动 / 边缘缩放 / 最大化重绘
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::MainWindow *ui;

    QStackedWidget *_stacked_widget = nullptr;
    LoginDialog *_login_dlg = nullptr;
    RegisterDialog *_reg_dlg = nullptr;
    ResetDialog *_reset_dlg = nullptr;
    ChatDialog *_chat_dlg = nullptr;

    // ★ 无边框相关成员
    QWidget *_main_widget = nullptr;        // 承载圆角+阴影+内容的容器（白底圆角）
    QWidget *_title_bar = nullptr;          // 顶部标题栏（放窗口按钮，空白区可作拖拽热区）
    QPushButton *_btn_min = nullptr;        // 最小化（标题栏，聊天窗使用）
    QPushButton *_btn_max = nullptr;        // 最大化/还原（标题栏，聊天窗使用）
    QPushButton *_btn_close = nullptr;      // 关闭（标题栏，聊天窗使用）
    QPushButton *_btn_min_small = nullptr;  // 最小化（小窗右上角浮动）
    QPushButton *_btn_close_small = nullptr;// 关闭（小窗右上角浮动，避免标题栏）
    QGraphicsDropShadowEffect *_shadow = nullptr;

    QPoint _drag_start;                     // 拖动偏移（空点 = 当前未拖动）
    bool _resizing = false;                 // 是否处于边缘缩放中
    Qt::Edges _resize_edges;                 // 正在缩放哪些边
    QPoint _press_pos;                       // 按下时全局坐标（缩放用）
    QRect _press_geom;                       // 按下时窗口几何（缩放用）
    bool _maximized = false;                // 是否已最大化（最大化时去圆角/阴影/留白）

    UIStatus _ui_status;

    // 圆角半径 / 四周留白（给阴影显示）/ 标题栏高度
    static constexpr int kCornerRadius = 16;
    static constexpr int kShadowMargin = 15;
    static constexpr int kTitleBarHeight = 34;

    void setupFrameless();                  // 去系统边框 + 透明 + 圆角容器 + 阴影 + 标题栏
    void applyRoundedMask();                 // 给 _main_widget 做圆角遮罩（裁掉方角锯齿）
    void refreshChrome(bool maximized);      // 最大化/还原时切换圆角/阴影/留白
    void applyMainWidgetBackground();        // 按 UI 状态切换 main_widget 背景（登录态渐变 / 聊天态白）
    void toggleMaximize();                   // 最大化 <-> 还原
    void updateWindowControls();             // 按当前页决定显示哪些窗口按钮
    void repositionSmallControls();            // 小窗右上角最小化/关闭按钮定位
    QSize windowSizeForContent(const QSize &content, bool withTitleBar) const; // 内容尺寸 -> 窗口尺寸
    bool isResizable() const;                // 当前窗口是否可自由缩放（仅聊天大窗）
    static bool isInteractive(QWidget *w);   // 命中交互控件则不启动拖动
    void updateCursor(const QPoint &pos);    // 悬停边缘时切换缩放光标
};

#endif // MAINWINDOW_H
