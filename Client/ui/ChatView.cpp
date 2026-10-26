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
    // ★ 用户自己滚动时判断是否已回到底部（回到底部就清掉"新消息"浮标）
    connect(pVScrollBar, &QScrollBar::valueChanged, this, &ChatView::onScrollBarValueChanged);
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
   // ★ 插入【之前】先判断用户是否"贴着底部"：
   //   贴底   → 新消息自动跟到底部（自己发的 / 刚收到的，符合预期）
   //   翻历史 → 保持当前位置不打断阅读，改用"N 条新消息"浮标提示。
   //   原来是无条件滚到底，用户正往上翻历史时会被强行拽回底部。
   QScrollBar *bar = m_pScrollArea->verticalScrollBar();
   _auto_scroll = (bar->maximum() - bar->value()) <= kStickBottomPx;

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

    // 切会话：清掉"新消息"浮标与计数，否则会带着上一个会话的计数过来
    _pending_new = 0;
    _auto_scroll = true;
    updateNewMsgButton();
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

void ChatView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateNewMsgButton();   // 浮标重新贴到右下角
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
        if (_auto_scroll) {
            pVScrollBar->setSliderPosition(pVScrollBar->maximum());
        } else {
            // ★ 用户在翻历史：不动滚动条，只累计条数并弹浮标
            ++_pending_new;
            updateNewMsgButton();
            emit sig_new_msg_while_scrolled_up(_pending_new);
        }
        //500毫秒内可能调用多次
        QTimer::singleShot(500, [this]()
        {
            isAppended = false;
        });
    }
}

void ChatView::onScrollBarValueChanged(int value)
{
    Q_UNUSED(value);
    QScrollBar *bar = m_pScrollArea->verticalScrollBar();
    // 用户自己滚回底部 → 视为已读，清掉浮标
    if (_pending_new > 0 && (bar->maximum() - bar->value()) <= kStickBottomPx) {
        _pending_new = 0;
        updateNewMsgButton();
    }
}

void ChatView::scrollToBottom()
{
    QScrollBar *bar = m_pScrollArea->verticalScrollBar();
    bar->setSliderPosition(bar->maximum());
    _pending_new = 0;
    updateNewMsgButton();
}

void ChatView::ensureNewMsgButton()
{
    if (_new_msg_btn != nullptr) {
        return;
    }
    _new_msg_btn = new QPushButton(this);
    _new_msg_btn->setObjectName("new_msg_btn");
    _new_msg_btn->setCursor(Qt::PointingHandCursor);
    _new_msg_btn->setStyleSheet(
        QString("QPushButton{background:#ffffff;color:#4C5FD7;border:1px solid #E4E6EB;"
                "border-radius:14px;padding:6px 12px;font-size:12px;}"
                "QPushButton:hover{background:#F0F2FD;}"));
    _new_msg_btn->hide();
    connect(_new_msg_btn, &QPushButton::clicked, this, &ChatView::scrollToBottom);
}

void ChatView::updateNewMsgButton()
{
    if (_pending_new <= 0) {
        if (_new_msg_btn) {
            _new_msg_btn->hide();
        }
        return;
    }
    ensureNewMsgButton();
    _new_msg_btn->setText(QString("%1 条新消息 ↓").arg(_pending_new));
    _new_msg_btn->adjustSize();
    // 贴在消息区右下角（留 12px 边距）
    _new_msg_btn->move(this->width() - _new_msg_btn->width() - 12,
                       this->height() - _new_msg_btn->height() - 12);
    _new_msg_btn->raise();
    _new_msg_btn->show();
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
