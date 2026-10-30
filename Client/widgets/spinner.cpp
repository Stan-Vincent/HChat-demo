#include "spinner.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QtMath>

Spinner::Spinner(QWidget* parent, int diameter)
    : QWidget(parent)
    , _color(0x4C, 0x5F, 0xD7)   // 主题静靛
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);  // 纯指示器，不吞点击
    setFixedSize(diameter, diameter);
}

void Spinner::setColor(const QColor& c)
{
    if (!c.isValid() || c == _color) return;
    _color = c;
    update();
}

void Spinner::setDotCount(int n)
{
    n = qBound(4, n, 12);
    if (n == _dots) return;
    _dots = n;
    update();
}

void Spinner::setAngle(int a)
{
    _angle = a % 360;
    update();
}

QSize Spinner::sizeHint() const
{
    return size();
}

void Spinner::start()
{
    if (_running) return;
    _running = true;
    if (!_anim) {
        // 900ms 一圈：比 loading.gif 略快一点，看起来更"在干活"
        _anim = new QPropertyAnimation(this, "angle", this);
        _anim->setDuration(900);
        _anim->setStartValue(0);
        _anim->setEndValue(360);
        _anim->setEasingCurve(QEasingCurve::Linear);
        _anim->setLoopCount(-1);
    }
    _anim->start();
}

void Spinner::stop()
{
    _running = false;
    if (_anim) _anim->stop();
    _angle = 0;
    update();
}

void Spinner::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    start();          // 只在可见时转，隐藏后不烧 CPU
}

void Spinner::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    if (_anim) _anim->stop();
    _running = false;
}

void Spinner::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    const qreal side   = qMin(width(), height());
    const qreal center = side / 2.0;
    const qreal orbit  = side * 0.34;    // 圆点圆心所在半径
    const qreal dotR   = side * 0.078;   // 单个圆点半径

    for (int i = 0; i < _dots; ++i) {
        // i=0 是"头部"（最亮），往后逐级变淡形成拖尾；
        // 整体再叠加当前旋转角度，形成连续转动。
        const qreal deg = 360.0 / _dots * i + _angle;
        const qreal rad = qDegreesToRadians(deg - 90.0);   // -90：从 12 点方向起转
        const QPointF c(center + orbit * qCos(rad), center + orbit * qSin(rad));

        QColor col = _color;
        const qreal tail = qreal(i) / _dots;               // 0(头) -> 1(尾)
        col.setAlphaF(qBound(0.12, 1.0 - 0.88 * tail, 1.0));
        p.setBrush(col);
        p.drawEllipse(c, dotR, dotR);
    }
}
