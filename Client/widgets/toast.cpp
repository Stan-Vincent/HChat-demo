#include "toast.h"

#include <QApplication>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <algorithm>

QList<QPointer<Toast>> Toast::s_active;

Toast::Toast(QWidget *parent, const QString &text, Type type, int durationMs)
    : QWidget(parent), _durationMs(durationMs)
{
    setObjectName("toast");
    // 背景透明：圆角外的区域透出父控件（main_widget 渐变），不显方角。
    setAttribute(Qt::WA_TranslucentBackground, true);
    // 不阻塞交互：鼠标事件直接穿透到下层，用户可继续操作界面。
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // 左侧 accent 颜色按类型区分（与主色系一致）
    QString accent;
    switch (type) {
    case Success: accent = "#34c759"; break;
    case Warning: accent = "#ff9a3c"; break;
    case Error:   accent = "#f14e4e"; break;
    default:      accent = "#ff7a45"; break;
    }
    setStyleSheet(QString(
        "QWidget#toast{background:rgba(255,255,255,0.97);border-radius:10px;"
        "border-left:4px solid %1;}").arg(accent));

    QLabel *label = new QLabel(text, this);
    label->setObjectName("toast_label");
    label->setWordWrap(true);
    label->setMaximumWidth(260);
    label->setStyleSheet("QLabel#toast_label{color:#1f1f1f;font-size:13px;"
                         "background:transparent;border:none;padding:10px 14px;}");
    label->adjustSize();

    // 给左 accent + 内边距预留空间
    int w = qMax(label->width() + 28, 80);
    int h = label->height() + 20;
    setFixedSize(w, h);
}

void Toast::showToast(QWidget *parent, const QString &text, Type type, int durationMs)
{
    if (parent == nullptr) {
        return;
    }
    Toast *t = new Toast(parent, text, type, durationMs);
    t->raise();   // 浮在最上层

    // 淡入：用 QGraphicsOpacityEffect（子控件上的单一效果，无嵌套风险）
    auto *eff = new QGraphicsOpacityEffect(t);
    eff->setOpacity(0.0);
    t->setGraphicsEffect(eff);

    s_active.append(QPointer<Toast>(t));
    relayout();   // 先把位置定好，再淡入

    auto *fadeIn = new QPropertyAnimation(eff, "opacity", t);
    fadeIn->setDuration(220);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    // 停留到时触发淡出 + 自销毁
    QTimer::singleShot(durationMs, t, &Toast::startDismiss);
}

void Toast::startDismiss()
{
    // 先从活跃列表移除自身，其余上移补位
    for (int i = 0; i < s_active.size(); ++i) {
        if (s_active[i] == this) {
            s_active.removeAt(i);
            break;
        }
    }
    relayout();

    auto *eff = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
    if (eff == nullptr) {
        deleteLater();
        return;
    }
    auto *fadeOut = new QPropertyAnimation(eff, "opacity", this);
    fadeOut->setDuration(260);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InCubic);
    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
        deleteLater();
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::relayout()
{
    // 顺手清掉已销毁的空指针，避免列表无限增长
    s_active.erase(std::remove_if(s_active.begin(), s_active.end(),
                                  [](const QPointer<Toast> &p) { return p.isNull(); }),
                  s_active.end());
    if (s_active.isEmpty()) {
        return;
    }

    QWidget *parent = s_active.first()->parentWidget();
    if (parent == nullptr) {
        return;
    }
    const int margin = 20;   // 距窗口右/上边距（避开圆角与阴影留白）
    const int gap = 10;
    int y = margin;
    for (const QPointer<Toast> &p : s_active) {
        if (p.isNull()) {
            continue;
        }
        int x = parent->width() - p->width() - margin;
        p->setGeometry(x, y, p->width(), p->height());
        y += p->height() + gap;
    }
}
