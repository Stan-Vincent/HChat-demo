#include "timerbtn.h"
#include <QMouseEvent>
#include <QDebug>

TimerBtn::TimerBtn(QWidget *parent):QPushButton(parent),_counter(10)
{
    _timer = new QTimer(this);

    //等待timeout
    connect(_timer, &QTimer::timeout, [this](){

        //每一秒计数减一
        _counter--;

        //倒计时结束，重置状态，开放获取按钮按键
        if(_counter <= 0){
            _timer->stop();
            _counter = 10;
            this->setText("获取");
            this->setEnabled(true);
            return;
        }
        //没结束，继续倒计时
        this->setText(QString::number(_counter));
    });

    // 关键：把倒计时逻辑挂在 clicked 信号上
    // 只有基类判断"真点击"时，clicked 才会发射
    connect(this, &QPushButton::clicked, this, [this]() {
        this->setEnabled(false);
        this->setText(QString::number(_counter));
        _timer->start(1000);
    });
}

TimerBtn::~TimerBtn()
{
    _timer->stop();
}

// void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
// {
//     if (e->button() == Qt::LeftButton) {
//         // 在这里处理鼠标左键释放事件

//         //qDebug() << "MyButton was released!";
//         this->setEnabled(false);
//         this->setText(QString::number(_counter));
//         // 启动定时器，每秒触发
//         _timer->start(1000);
//         //emit clicked();
//     }
//     // 调用基类的mouseReleaseEvent以确保正常的事件处理（如点击效果）
//     QPushButton::mouseReleaseEvent(e);
// }