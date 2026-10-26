#include "BubbleFrame.h"
#include <QPainter>
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

    if(m_role == ChatRole::Other)
    {
        // 对方气泡：白底 + 1px 浅灰描边（P1 §3：白底 + 1px border + 深字）
        QColor bk_color(Qt::white);
        QColor border_color(0xE4, 0xE6, 0xEB);   // #E4E6EB 分隔线/border
        QRect bk_rect = QRect(WIDTH_SANJIAO, 0, this->width()-WIDTH_SANJIAO, this->height());
        painter.setPen(QPen(border_color, 1));
        painter.setBrush(QBrush(bk_color));
        painter.drawRoundedRect(bk_rect, 10, 10);
        // 小三角：白填充衔接气泡（无描边，落在浅灰聊天底上自成一脉）
        painter.setPen(Qt::NoPen);
        QPointF points[3] = {
            QPointF(bk_rect.x(), 12),
            QPointF(bk_rect.x(), 12 + WIDTH_SANJIAO),
            QPointF(bk_rect.x()-WIDTH_SANJIAO, 12 + WIDTH_SANJIAO/2),
        };
        painter.drawPolygon(points, 3);
    }
    else
    {
        // 自己消息：静靛浅调 #EEF0FB（238,240,251），无描边（P1 §3：浅底深字）
        QColor bk_color(238,240,251);
        QRect bk_rect = QRect(0, 0, this->width()-WIDTH_SANJIAO, this->height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(bk_color));
        painter.drawRoundedRect(bk_rect, 10, 10);
        // 小三角
        QPointF points[3] = {
            QPointF(bk_rect.x()+bk_rect.width(), 12),
            QPointF(bk_rect.x()+bk_rect.width(), 12 + WIDTH_SANJIAO),
            QPointF(bk_rect.x()+bk_rect.width()+WIDTH_SANJIAO, 12 + WIDTH_SANJIAO/2),
        };
        painter.drawPolygon(points, 3);
    }

    return QFrame::paintEvent(e);
}
