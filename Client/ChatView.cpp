#include "ChatView.h"
#include <QScrollBar>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QDebug>
#include "TextBubble.h"

#include <QTimer>
#include <QStyleOption>
#include <QPainter>

ChatView::ChatView(QWidget *parent)
   : QWidget(parent)
   , isAppended(false)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout();
    this->setLayout(pMainLayout);
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pScrollArea = new QScrollArea();
    m_pScrollArea->setObjectName("chat_area");
    pMainLayout->addWidget(m_pScrollArea);

    QWidget *w = new QWidget(this);
    w->setObjectName("chat_bg");
    w->setAutoFillBackground(true);
    QVBoxLayout *pHLayout_1 = new QVBoxLayout();
    pHLayout_1->addWidget(new QWidget(), 100000);
    w->setLayout(pHLayout_1);
    m_pScrollArea->setWidget(w);    //应该时在QSCrollArea构造后执行 才对

    m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScrollBar *pVScrollBar = m_pScrollArea->verticalScrollBar();
    connect(pVScrollBar, &QScrollBar::rangeChanged,this, &ChatView::onVScrollBarMoved);
    //把垂直ScrollBar放到上边 而不是原来的并排
    QHBoxLayout *pHLayout_2 = new QHBoxLayout();
    pHLayout_2->addWidget(pVScrollBar, 0, Qt::AlignRight);
    pHLayout_2->setContentsMargins(0, 0, 0, 0);
    m_pScrollArea->setLayout(pHLayout_2);
    pVScrollBar->setHidden(true);

    m_pScrollArea->setWidgetResizable(true);
    m_pScrollArea->installEventFilter(this);
    initStyleSheet();
    // ★ 安装应用级事件过滤器：捕获"消息列表内任意位置的左键按下"。
    //   只用滚动区/视口的过滤器不够——子 QTextEdit 会吞掉鼠标按下事件
    //   （它要用来定位光标/起选），事件不会冒泡到父级过滤器，导致"点在别的
    //   气泡/背景上"清不掉旧选区。qApp 级过滤器在所有事件最前面触发，任何
    //   按下（包括落在子 QTextEdit 上的）都看得到，从而统一清除所有气泡选区。
    qApp->installEventFilter(this);
}

void ChatView::appendChatItem(QWidget *item)
{
   QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(m_pScrollArea->widget()->layout());
   //qDebug() << "vl->count() is " << vl->count();
   // ★ 这一行之前被误注释掉了：appendChatItem 实际什么都不做，
   //   聊天条目（历史消息 / 自己刚发的消息）构造出来却从没插进滚动区布局，
   //   所以「聊天记录不显示 / 发了消息也不显示」。恢复插入即可。
   vl->insertWidget(vl->count()-1, item);
   isAppended = true;
}

void ChatView::prependChatItem(QWidget *item)
{

}

void ChatView::insertChatItem(QWidget *before, QWidget *item)
{

}

void ChatView::removeAllItem()
{
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(m_pScrollArea->widget()->layout());

   int count = layout->count();

    for (int i = 0; i < count - 1; ++i) {
        QLayoutItem *item = layout->takeAt(0); // 始终从第一个控件开始删除
        if (item) {
            if (QWidget *widget = item->widget()) {
                delete widget;
            }
            delete item;
        }
    }

}

bool ChatView::eventFilter(QObject *o, QEvent *e)
{
    // ★ 任意位置左键按下 → 取消所有消息气泡的文字选中。
    //   先判断落点是否在本消息列表范围内（点会话列表/输入框等其它区域不动），
    //   只对落在消息区（气泡、背景、昵称、头像、滚动条都算）的按下做清除。
    //   注意：必须在 qApp 级拦截且不能 return true，否则会吞掉正常点击（起选/拖选）。
    if (e->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton && isPosInsideChatView(me->globalPos())) {
            clearAllTextSelections();
        }
    }
    /*if(e->type() == QEvent::Resize && o == )
    {

    }
    else */if(e->type() == QEvent::Enter && o == m_pScrollArea)
    {
        m_pScrollArea->verticalScrollBar()->setHidden(m_pScrollArea->verticalScrollBar()->maximum() == 0);
    }
    else if(e->type() == QEvent::Leave && o == m_pScrollArea)
    {
         m_pScrollArea->verticalScrollBar()->setHidden(true);
    }
    return QWidget::eventFilter(o, e);
}

bool ChatView::isPosInsideChatView(const QPoint &globalPos) const
{
    // 取落点处的真实控件，沿父链回找是否归属本 ChatView（消息列表）。
    // 命中容器/视口/某条 ChatItemBase/其任意子控件（含 QTextEdit）都算"在消息区内"。
    QWidget *w = QApplication::widgetAt(globalPos);
    while (w != nullptr) {
        if (w == this) {
            return true;
        }
        w = w->parentWidget();
    }
    return false;
}

void ChatView::clearAllTextSelections()
{
    // 遍历消息列表里的每条 ChatItemBase，清掉其下所有 TextBubble 的文字选区。
    // 用 findChildren 而非缓存指针：条目会随切会话增删，查找更稳，条数也少。
    QWidget *container = m_pScrollArea->widget();
    if (container == nullptr) {
        return;
    }
    QLayout *lay = container->layout();
    if (lay == nullptr) {
        return;
    }
    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem *li = lay->itemAt(i);
        if (li == nullptr) {
            continue;
        }
        QWidget *itemWidget = li->widget();
        if (itemWidget == nullptr) {
            continue;
        }
        QList<TextBubble *> bubbles = itemWidget->findChildren<TextBubble *>();
        for (TextBubble *tb : bubbles) {
            tb->clearSelection();
        }
    }
}

void ChatView::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}


void ChatView::onVScrollBarMoved(int min, int max)
{
    if(isAppended) //添加item可能调用多次
    {
        QScrollBar *pVScrollBar = m_pScrollArea->verticalScrollBar();
        pVScrollBar->setSliderPosition(pVScrollBar->maximum());
        //500毫秒内可能调用多次
        QTimer::singleShot(500, [this]()
        {
            isAppended = false;
        });
    }
}

void ChatView::initStyleSheet()
{
//    QScrollBar *scrollBar = m_pScrollArea->verticalScrollBar();
//    scrollBar->setStyleSheet("QScrollBar{background:transparent;}"
//                             "QScrollBar:vertical{background:transparent;width:8px;}"
//                             "QScrollBar::handle:vertical{background:red; border-radius:4px;min-height:20px;}"
//                             "QScrollBar::add-line:vertical{height:0px}"
//                             "QScrollBar::sub-line:vertical{height:0px}"
//                             "QScrollBar::add-page:vertical {background:transparent;}"
//                             "QScrollBar::sub-page:vertical {background:transparent;}");
}
