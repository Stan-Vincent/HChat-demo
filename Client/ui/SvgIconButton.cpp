#include "SvgIconButton.h"

#include <QFile>
#include <QSvgRenderer>
#include <QPainter>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QRegularExpression>

SvgIconButton::SvgIconButton(QWidget *parent)
    : QLabel(parent)
{
    setCursor(Qt::PointingHandCursor);
    setScaledContents(false);
}

void SvgIconButton::setSvg(const QString &pathInQrc)
{
    _svgNormal = loadSvg(pathInQrc);
    reload();
}

void SvgIconButton::setCheckedSvg(const QString &pathInQrc)
{
    _svgChecked = loadSvg(pathInQrc);
    reload();
}

void SvgIconButton::setColors(const QString &normal,
                              const QString &hover,
                              const QString &press)
{
    _colorNormal = normal;
    _colorHover  = hover;
    _colorPress  = press;
    reload();
}

void SvgIconButton::setChecked(bool on)
{
    _checked = on;
    reload();
}

void SvgIconButton::setSelected(bool on)
{
    _selected = on;
    reload();
}

QSize SvgIconButton::sizeHint() const
{
    if (_iconSize.isValid()) return _iconSize;
    return QLabel::sizeHint();
}

QString SvgIconButton::loadSvg(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}

/* 把 SVG 里的单色描边（及非 none 的填充）替换成目标色。
 * 项目图标都是「单色描边 + fill="none"」，所以一次全局替换即可完成换色。 */
QString SvgIconButton::recolor(const QString &svg, const QString &color)
{
    QString out = svg;
    const QRegularExpression reStroke(R"(stroke="#[0-9A-Fa-f]{3,8}")");
    out.replace(reStroke, QString("stroke=\"%1\"").arg(color));
    const QRegularExpression reFill(R"(fill="#[0-9A-Fa-f]{3,8}")");
    out.replace(reFill, QString("fill=\"%1\"").arg(color));
    return out;
}

void SvgIconButton::reload()
{
    const QString src = (!_svgChecked.isEmpty() && _checked) ? _svgChecked : _svgNormal;
    if (src.isEmpty()) return;

    const QString color = _pressed ? _colorPress
                         : (_hovered ? _colorHover : _colorNormal);
    const QString svg = recolor(src, color);

    const qreal dpr = devicePixelRatioF();
    const QSize sz = _iconSize.isValid()
                         ? _iconSize
                         : (this->width() > 0 ? this->size() : QSize(24, 24));

    QPixmap pm(qRound(sz.width() * dpr), qRound(sz.height() * dpr));
    pm.fill(Qt::transparent);
    {
        QSvgRenderer r;
        if (r.load(svg.toUtf8())) {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            if (_selected) {
                // 选中底色圆角矩形（侧边栏等），图标描边保持亮色
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(_selectedColor));
                const qreal m = pm.width() * 0.08;
                const QRectF rc(m, m, pm.width() - 2 * m, pm.height() - 2 * m);
                p.drawRoundedRect(rc, rc.width() * 0.32, rc.height() * 0.32);
            }
            r.render(&p, QRectF(0, 0, pm.width(), pm.height()));
        }
    }
    pm.setDevicePixelRatio(dpr);
    setPixmap(pm);
}

void SvgIconButton::enterEvent(QEnterEvent *event)
{
    _hovered = true;
    reload();
    QLabel::enterEvent(event);
}

void SvgIconButton::leaveEvent(QEvent *event)
{
    _hovered = false;
    _pressed = false;
    reload();
    QLabel::leaveEvent(event);
}

void SvgIconButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _pressed = true;
        reload();
    }
    QLabel::mousePressEvent(event);
}

void SvgIconButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const bool wasPressed = _pressed;
        _pressed = false;
        reload();
        if (wasPressed) {
            emit clicked(QString(), ClickLbState::Normal);
        }
    }
    QLabel::mouseReleaseEvent(event);
}
