#include "BubbleFrame.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
const int WIDTH_SANJIAO  = 8;  //三角宽
BubbleFrame::BubbleFrame(ChatRole role, QWidget *parent)
    :QFrame(parent)
    ,m_role(role)
    ,m_margin(3)
{
    m_pHLayout = new QHBoxLayout();
    if(m_role == ChatRole::Self)
        m_pHLayout->setContentsMargins(m_margin, m_margin, WIDTH_SANJIAO + m_margin, m_margin);
    else
        m_pHLayout->setContentsMargins(WIDTH_SANJIAO + m_margin, m_margin, m_margin, m_margin);

    this->setLayout(m_pHLayout);
}

void BubbleFrame::setMargin(int margin)
{
    Q_UNUSED(margin);
    //m_margin = margin;
}

void BubbleFrame::setWidget(QWidget *w)
{
    if(m_pHLayout->count() > 0)
        return ;
    else{
        m_pHLayout->addWidget(w);
    }
}

void BubbleFrame::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);   // 描边/圆角更平滑

    // ★ P2 §2：把原来外挂的三角尾巴换成"缺角小圆角"尾指（更现代、更克制）。
    //   气泡铺满整个控件；尾巴侧、靠近头像的那个角用 2px 小圆角，其余三角 10px。
    //   self 尾巴在右（右下角小圆角）/ other 尾巴在左（左下角小圆角）。
    const qreal R  = 10.0;             // 常规圆角
    const qreal RT = 2.0;              // 尾指小圆角
    const bool  self = (m_role == ChatRole::Self);

    QRectF rc(0, 0, this->width(), this->height());
    qreal tl = R, tr = R, br = R, bl = R;
    if (self) br = RT; else bl = RT;   // 尾指角

    QPainterPath path;
    path.moveTo(rc.left() + tl, rc.top());
    path.lineTo(rc.right() - tr, rc.top());
    path.arcTo(rc.right() - 2 * tr, rc.top(), 2 * tr, 2 * tr, 90, -90);
    path.lineTo(rc.right(), rc.bottom() - br);
    path.arcTo(rc.right() - 2 * br, rc.bottom() - 2 * br, 2 * br, 2 * br, 0, -90);
    path.lineTo(rc.left() + bl, rc.bottom());
    path.arcTo(rc.left(), rc.bottom() - 2 * bl, 2 * bl, 2 * bl, 270, -90);
    path.lineTo(rc.left(), rc.top() + tl);
    path.arcTo(rc.left(), rc.top(), 2 * tl, 2 * tl, 180, -90);
    path.closeSubpath();

    if (self) {
        // 自己消息：静靛浅调 #EEF0FB，浅底深字，无描边
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(238, 240, 251));
        painter.drawPath(path);
    } else {
        // 对方气泡：白底 + 1px #E4E6EB 描边（P1 §3）
        painter.setPen(QPen(QColor(0xE4, 0xE6, 0xEB), 1));
        painter.setBrush(QColor(Qt::white));
        painter.drawPath(path);
    }

    return QFrame::paintEvent(e);
}
