#include "chatuserlist.h"
#include<QScrollBar>
#include "usermgr.h"
#include <QTimer>
#include <QCoreApplication>

ChatUserList::ChatUserList(QWidget *parent):QListWidget(parent), _load_pending(false)
{
    Q_UNUSED(parent);
     this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
     this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);
}

bool ChatUserList::eventFilter(QObject *watched, QEvent *event)
{
    // 检查事件是否是鼠标悬浮进入或离开
    if (watched == this->viewport()) {
        if (event->type() == QEvent::Enter) {
            // 鼠标悬浮，显示滚动条
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        } else if (event->type() == QEvent::Leave) {
            // 鼠标离开，隐藏滚动条
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

    // 检查事件是否是鼠标滚轮事件
    if (watched == this->viewport() && event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        // ★ 触摸板惯性滚动时 angleDelta 可能是几十上百，必须夹一下，
        //   否则一次滚动的步长会失控（value - numSteps 变成很大的负数）。
        int numDegrees = qBound(-120, wheelEvent->angleDelta().y(), 120) / 8;
        int numSteps = numDegrees / 15; // 计算滚动步数

        // 设置滚动幅度
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - numSteps);

        // 检查是否滚动到底部
        QScrollBar *scrollBar = this->verticalScrollBar();
        int maxScrollValue = scrollBar->maximum();
        int currentValue = scrollBar->value();
        //int pageSize = 10; // 每页加载的联系人数量

        // ★ 必须是"接近底部"而不是"currentValue >= max"：
        //   内容不足一屏时 maximum()==0，滚到顶部(currentValue==0)也会满足
        //   max - current <= 0，导致一进列表就开始疯狂加载。
        if (maxScrollValue > 0 && maxScrollValue - currentValue <= 2) {
             auto b_loaded = UserMgr::GetInstance()->IsLoadChatFin();
             if(b_loaded){
                 return true;
             }

             if(_load_pending){
                 return true;
             }
            // 滚动到底部，加载新的联系人
            qDebug()<<"load more chat user";
            _load_pending = true;

            // 只解锁 pending 标志，不要在这里退出程序。
            // 教程原代码这里有一句 QCoreApplication::quit()（调试残留），
            // 结果是"会话列表滚到底部 -> 整个客户端闪退"，非常难查。
            // ★ 这个标志不能在 100ms 就放开：加载慢的时候用户继续滚会重复触发。
            QTimer::singleShot(100, [this](){
                _load_pending = false;
                });
            //发送信号通知聊天界面加载更多聊天内容
            emit sig_loading_chat_user();
         }

        return true; // 停止事件传递
    }

    return QListWidget::eventFilter(watched, event);
}
