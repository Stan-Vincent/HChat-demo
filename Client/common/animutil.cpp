#include "animutil.h"

#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QEvent>

namespace AnimUtil {

// ============================================================
//  窗口构造时：150ms 透明度淡入 + 轻微缩放
// ============================================================
void fadeInWindow(QWidget* w, int duration, double fromScale)
{
    if (!w) return;

    // 1) 透明度淡入
    w->setWindowOpacity(0.0);
    auto* op = new QPropertyAnimation(w, "windowOpacity", w);
    op->setDuration(duration);
    op->setStartValue(0.0);
    op->setEndValue(1.0);
    op->setEasingCurve(QEasingCurve::OutCubic);
    op->start(QAbstractAnimation::DeleteWhenStopped);

    // 2) 轻微缩放：基于当前几何，从"中心略小"放大到原尺寸。
    //    不重设窗口位置，避免与 WM 居中冲突；终点即我们刚 set 的几何，无跳变。
    QRect geo = w->geometry();
    if (geo.isEmpty() || geo.width() <= 0 || geo.height() <= 0)
        return;
    int sw = qRound(geo.width() * fromScale);
    int sh = qRound(geo.height() * fromScale);
    QRect start(geo.center().x() - sw / 2, geo.center().y() - sh / 2, sw, sh);
    w->setGeometry(start);
    auto* sc = new QPropertyAnimation(w, "geometry", w);
    sc->setDuration(duration);
    sc->setStartValue(start);
    sc->setEndValue(geo);
    sc->setEasingCurve(QEasingCurve::OutCubic);
    sc->start(QAbstractAnimation::DeleteWhenStopped);
}

// ============================================================
//  堆叠部件交叉淡入（整窗透明度过渡，不使用任何 GraphicsEffect）
// ============================================================
void crossFadeStacked(QStackedWidget* stack, QWidget* next,
                     std::function<void()> applySize, int duration)
{
    if (!stack) return;
    if (stack->currentWidget() == next) {   // 同页：直接执行尺寸切换，不做动画
        if (applySize) applySize();
        return;
    }

    // ★ 为什么不用 QGraphicsOpacityEffect 挂在 _stacked_widget 上：
    //   它的父 _main_widget 已经挂了 QGraphicsDropShadowEffect（阴影），嵌套 graphics effect
    //   在渲染"聊天页"这种复杂子控件时会触发崩溃（实测登录→聊天直接闪退）。
    //   改用整窗 windowOpacity 做"淡出→切页+改尺寸→淡入"，是原生窗口属性，零嵌套风险，
    //   视觉上仍是平滑的页面过渡。
    QWidget* win = stack->window();
    int half = qMax(1, duration / 2);

    // 两段独立动画（out 淡出 -> in 淡入），各自 DeleteWhenStopped 自动回收，
    // 不在动画自身的 finished 里 delete 自己（否则 use-after-free）。
    auto* out = new QPropertyAnimation(win, "windowOpacity", win);
    out->setDuration(half);
    out->setEasingCurve(QEasingCurve::InOutQuad);
    out->setStartValue(1.0);
    out->setEndValue(0.0);
    QObject::connect(out, &QPropertyAnimation::finished, win, [=]() {
        stack->setCurrentWidget(next);   // 切到新页（此刻整窗透明度为 0，不可见）
        if (applySize) applySize();       // 改窗体尺寸 / 标题栏等
        auto* in = new QPropertyAnimation(win, "windowOpacity", win);
        in->setDuration(half);
        in->setEasingCurve(QEasingCurve::InOutQuad);
        in->setStartValue(0.0);
        in->setEndValue(1.0);
        in->start(QAbstractAnimation::DeleteWhenStopped);
    });
    out->start(QAbstractAnimation::DeleteWhenStopped);
}

// ============================================================
//  按钮 hover 背景色过渡（QVariantAnimation + EventFilter）
// ============================================================
namespace {
// 不加 Q_OBJECT：只重写 eventFilter 虚函数 + 用函数指针式 connect，无需 moc。
class HoverColorFilter : public QObject
{
public:
    HoverColorFilter(QWidget* target, const QColor& normal, const QColor& hover,
                     int duration, const QString& staticStyle, QObject* parent = nullptr)
        : QObject(parent), _target(target), _normal(normal),
          _hover(hover), _static(staticStyle)
    {
        _anim = new QVariantAnimation(this);
        _anim->setDuration(duration);
        _anim->setEasingCurve(QEasingCurve::InOutQuad);

        connect(_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            QColor c = v.value<QColor>();
            _target->setProperty("__curColor", c);
            QString s = _static;
            if (!s.isEmpty() && !s.trimmed().endsWith(";")) s += ";";
            s += QString("background-color:%1;").arg(c.name(QColor::HexArgb));
            _target->setStyleSheet(s);       // 过渡期间用内联样式驱动背景色
        });

        connect(_anim, &QVariantAnimation::finished, this, [this]() {
            // 回到 normal 静止态：清掉内联样式，交还 qss 控制
            // （保留对象名样式，如 login_btn 的 22px 圆角、主按钮的 10px 圆角）。
            if (!_anim->endValue().isNull()
                && _anim->endValue().value<QColor>() == _normal) {
                _target->setStyleSheet("");
            }
        });

        _target->setProperty("__curColor", _normal);
        _target->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == _target) {
            if (e->type() == QEvent::Enter)      startTo(_hover);
            else if (e->type() == QEvent::Leave) startTo(_normal);
        }
        return QObject::eventFilter(o, e);
    }

private:
    void startTo(const QColor& c)
    {
        _anim->stop();
        QColor from = _target->property("__curColor").value<QColor>();
        if (!from.isValid()) from = _normal;
        _anim->setStartValue(from);
        _anim->setEndValue(c);
        _anim->start();
    }

    QWidget* _target;
    QColor _normal, _hover;
    QString _static;
    QVariantAnimation* _anim;
};
}

void installHoverColor(QWidget* w, const QColor& normal, const QColor& hover,
                       int duration, const QString& staticStyle)
{
    if (!w) return;
    // 直接挂在目标上做事件过滤器；重复安装代价低（仅对 Enter/Leave 响应）。
    new HoverColorFilter(w, normal, hover, duration, staticStyle, w);
}

void installHoverColorForRole(QWidget* w, const QString& role, int duration)
{
    if (!w) return;

    if (role == "primary") {
        // 圆角按对象名匹配：login_btn=22px（§8 单独覆盖），其余主按钮=10px（§8 实心组）。
        // 字体用 12pt bold，与 §8 实心组一致，避免 hover 时字号/字重跳变。
        QString radius = (w->objectName() == "login_btn") ? "22px" : "10px";
        QString st = QString("border:none;border-radius:%1;color:#ffffff;"
                             "font-size:12pt;font-weight:bold;").arg(radius);
        installHoverColor(w, QColor("#4C5FD7"), QColor("#6B7CE8"), duration, st);
    }
    else if (role == "ghost") {
        // ghost hover：边框/文字转橙（与 §9 ghost:hover 一致），背景做白→淡橙轻微过渡。
        QString st = "background-color:#ffffff;border:1px solid #4C5FD7;"
                     "color:#4C5FD7;border-radius:6px;padding:0 16px;font-size:14px;";
        installHoverColor(w, QColor("#ffffff"), QColor("#F0F2FD"), duration, st);
    }
    // link：保持透明背景文字，不动背景，跳过。
}

// ============================================================
//  未读红点脉冲
// ============================================================
// 用 geometry 动画做"呼吸缩放"：QLabel 的 border-image 红点没法用透明度
// （child widget 的 windowOpacity 不生效），缩放是最稳的做法。
// 动画对象名固定为 "redDotPulse"，父对象设为红点本身——红点销毁时动画
// 一起销毁，不会野指针；重复启动前也会先停掉旧的，避免叠加。
void startRedDotPulse(QLabel* dot, int periodMs)
{
    if (!dot) return;
    stopRedDotPulse(dot);

    const QRect base = dot->geometry();
    if (base.isEmpty()) return;
    // 记住原始几何，停止时恢复（否则可能停在放大状态）
    dot->setProperty("_red_dot_base", base);

    auto* anim = new QPropertyAnimation(dot, "geometry", dot);
    anim->setObjectName("redDotPulse");
    anim->setDuration(qMax(200, periodMs));

    const qreal k = 1.18;  // 放大倍率：轻微可感但不夸张
    int bw = qRound(base.width() * k);
    int bh = qRound(base.height() * k);
    QRect big(base.center().x() - bw / 2, base.center().y() - bh / 2, bw, bh);

    anim->setStartValue(base);
    anim->setKeyValueAt(0.5, big);
    anim->setEndValue(base);
    anim->setEasingCurve(QEasingCurve::InOutSine);
    anim->setLoopCount(-1);   // 无限脉冲，直到 stopRedDotPulse
    anim->start();
}

void stopRedDotPulse(QLabel* dot)
{
    if (!dot) return;
    if (auto* anim = dot->findChild<QPropertyAnimation*>("redDotPulse")) {
        anim->stop();
        anim->deleteLater();
    }
    // 恢复原始几何（动画可能停在中间帧）
    const QVariant v = dot->property("_red_dot_base");
    if (v.isValid() && v.canConvert<QRect>()) {
        dot->setGeometry(v.toRect());
    }
}

// ============================================================
//  输入框校验失败抖动（水平位移关键帧）
// ============================================================
void shakeWidget(QWidget* w, int duration)
{
    if (!w) return;
    // 停掉上一次未完成的抖动，避免叠加
    if (auto* old = w->findChild<QPropertyAnimation*>("shakeAnim")) {
        old->stop();
        old->deleteLater();
    }

    // 记录布局赋予的真实基准位置（只记一次；窗口缩放导致布局挪动时刷新基准）
    QPoint cur = w->pos();
    QPoint base;
    QVariant vb = w->property("_shake_base");
    if (vb.isValid() && vb.canConvert<QPoint>()) {
        base = vb.toPoint();
        if ((cur - base).manhattanLength() > 50) base = cur;   // 布局已挪动，用新基准
    } else {
        base = cur;
    }
    w->setProperty("_shake_base", base);

    const int amp = 6;
    auto* anim = new QPropertyAnimation(w, "pos", w);
    anim->setObjectName("shakeAnim");
    anim->setDuration(duration);
    anim->setEasingCurve(QEasingCurve::Linear);
    anim->setKeyValueAt(0.00, base);
    anim->setKeyValueAt(0.15, base + QPoint(-amp, 0));
    anim->setKeyValueAt(0.30, base + QPoint( amp, 0));
    anim->setKeyValueAt(0.45, base + QPoint(-amp, 0));
    anim->setKeyValueAt(0.60, base + QPoint( amp, 0));
    anim->setKeyValueAt(0.75, base + QPoint(-amp / 2, 0));
    anim->setKeyValueAt(0.90, base + QPoint( amp / 2, 0));
    anim->setKeyValueAt(1.00, base);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ============================================================
//  按钮悬浮上浮（轻微上移 + 过渡回弹）
// ============================================================
namespace {
// 局部事件过滤器：监听 Enter/Leave 驱动 pos 动画（无需 moc）。
class HoverLiftFilter : public QObject
{
public:
    HoverLiftFilter(QWidget* target, int lift, QObject* parent = nullptr)
        : QObject(parent), _target(target), _lift(lift)
    {
        setObjectName("hoverLiftFilter");
        _target->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == _target) {
            if (e->type() == QEvent::Enter)      lift(true);
            else if (e->type() == QEvent::Leave) lift(false);
        }
        return QObject::eventFilter(o, e);
    }
private:
    void lift(bool up)
    {
        QPoint cur = _target->pos();
        QPoint base;
        QVariant vb = _target->property("_lift_base");
        if (vb.isValid() && vb.canConvert<QPoint>()) {
            base = vb.toPoint();
            if ((cur - base).manhattanLength() > 50) base = cur;
        } else {
            base = cur;
        }
        _target->setProperty("_lift_base", base);

        QPoint to = up ? (base - QPoint(0, _lift)) : base;
        auto* anim = new QPropertyAnimation(_target, "pos", _target);
        anim->setDuration(140);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(_target->pos());
        anim->setEndValue(to);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QWidget* _target;
    int _lift;
};
}

void installHoverLift(QWidget* w, int liftPx)
{
    if (!w) return;
    if (auto* old = w->findChild<QObject*>("hoverLiftFilter"))
        old->deleteLater();
    new HoverLiftFilter(w, liftPx, w);
}

} // namespace AnimUtil
