#include "clickedbtn.h"
#include <QVariant>
#include "global.h"
#include <QKeyEvent>

ClickedBtn::ClickedBtn(QWidget *parent):QPushButton (parent)
{
    setCursor(Qt::PointingHandCursor); // 设置光标为小手
    setFocusPolicy(Qt::NoFocus);
}

ClickedBtn::~ClickedBtn(){

}


void ClickedBtn::SetState(QString normal, QString hover, QString press)
{
    _hover = hover;
    _normal = normal;
    _press = press;
    setProperty("state",normal);
    reload_qss(this);
    update();
}

void ClickedBtn::enterEvent(QEnterEvent* event)
{
    setProperty("state",_hover);
    reload_qss(this);
    update();
    QPushButton::enterEvent(event);
}

void ClickedBtn::leaveEvent(QEvent *event)
{
    setProperty("state",_normal);
    reload_qss(this);
    update();
    QPushButton::leaveEvent(event);
}

void ClickedBtn::mousePressEvent(QMouseEvent *event)
{
    setProperty("state",_press);
    reload_qss(this);
    update();
    QPushButton::mousePressEvent(event);
}

void ClickedBtn::mouseReleaseEvent(QMouseEvent *event)
{
    setProperty("state",_hover);
    reload_qss(this);
    update();
    QPushButton::mouseReleaseEvent(event);
}
