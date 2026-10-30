#include "loadingdlg.h"
#include "ui_loadingdlg.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QScreen>
#include "spinner.h"

LoadingDlg::LoadingDlg(QWidget *parent, QString tip):
    QDialog(parent),
    ui(new Ui::LoadingDlg)
{
    ui->setupUi(this);

    // 1. 让这个 Widget 透明背景、无边框、拦截底部事件
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);// 设置背景透明
    // 2. 让它覆盖父窗口整个面积
    if (parent) {
        // 获取屏幕尺寸
        setFixedSize(parent->size()); // 设置对话框为全屏尺寸
    }

    if (parent) {
        QPoint topLeft = parent->mapToGlobal(QPoint(0, 0));
        move(topLeft);
    }

    // 3. 半透明黑色背景（alpha = 128，大约 50% 透明度）
   // setStyleSheet("background-color: rgba(0, 0, 0, 128);");

    // ★ P3 微交互（「加载中」点位）：原来是 :/res/loading.gif + QMovie。
    //   换成自绘 Spinner —— 分辨率无关（gif 放大到 200px 会发虚）、颜色跟主题
    //   静靛走（原来灰阶）、不用解码线程也不占资源。仅在可见时旋转（见 Spinner）。
    //   loading_lb 继续留在 .ui 里当锚点，只是被替换出布局并隐藏，不清洗 .ui 结构。
    auto* spinner = new Spinner(this, 56);
    ui->horizontalLayout->replaceWidget(ui->loading_lb, spinner);
    ui->loading_lb->hide();

    ui->status_lb->setText(tip);
}

LoadingDlg::~LoadingDlg()
{
    delete ui;
}
