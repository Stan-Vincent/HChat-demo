#include "ChatItemBase.h"
#include <QFont>
#include <QVBoxLayout>
#include <QEvent>
#include <QResizeEvent>
#include "BubbleFrame.h"
ChatItemBase::ChatItemBase(ChatRole role, QWidget *parent)
    : QWidget(parent)
    , m_role(role)
{
    m_pNameLabel    = new QLabel();
    m_pNameLabel->setObjectName("chat_user_name");
    QFont font("Microsoft YaHei");
    font.setPointSize(9);
    m_pNameLabel->setFont(font);
    m_pNameLabel->setFixedHeight(20);
    m_pIconLabel    = new QLabel();
    m_pIconLabel->setScaledContents(true);
    m_pIconLabel->setFixedSize(42, 42);
    m_pBubble       = new QWidget();
    QGridLayout *pGLayout = new QGridLayout();
    pGLayout->setVerticalSpacing(3);
    pGLayout->setHorizontalSpacing(3);
    pGLayout->setContentsMargins(3, 3, 3, 3);
    QSpacerItem*pSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    //添加状态图标控件
    m_pStatusLabel = new QLabel();
    m_pStatusLabel->setFixedSize(16,16);
    m_pStatusLabel->setScaledContents(true);

    // hover 时间标签：浮在条目顶部（自己消息在左上、他人在右上，避开头像/昵称），
    // 平时隐藏，鼠标进入条目范围才显示。不加入布局，纯手工定位。
    m_pTimeLabel = new QLabel(this);
    m_pTimeLabel->setObjectName("chat_item_time");
    m_pTimeLabel->hide();
    // 鼠标事件穿透：时间标签不该抢走 hover 判定，也不会挡住点击
    m_pTimeLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    if(m_role == ChatRole::Self)
    {
        m_pNameLabel->setContentsMargins(0,0,8,0);
        m_pNameLabel->setAlignment(Qt::AlignRight);
        pGLayout->addWidget(m_pNameLabel, 0,2, 1,1);
        pGLayout->addWidget(m_pIconLabel, 0, 3, 2,1, Qt::AlignTop);
        pGLayout->addItem(pSpacer, 1, 0, 1, 1);
        pGLayout->addWidget(m_pStatusLabel,1,1,1,1,Qt::AlignCenter);
        pGLayout->addWidget(m_pBubble, 1,2, 1,1);
        pGLayout->setColumnStretch(0, 2);
        pGLayout->setColumnStretch(1,0); //status图标(固定大小)
        pGLayout->setColumnStretch(2,3); //名字+气泡(主要拉伸区域)
        pGLayout->setColumnStretch(3, 0);
    }else{
        m_pNameLabel->setContentsMargins(8,0,0,0);
        m_pNameLabel->setAlignment(Qt::AlignLeft);
        pGLayout->addWidget(m_pIconLabel, 0, 0, 2,1, Qt::AlignTop);
        pGLayout->addWidget(m_pNameLabel, 0,1, 1,1);
        pGLayout->addWidget(m_pBubble, 1,1, 1,1);
        pGLayout->addItem(pSpacer, 2, 2, 1, 1);
        pGLayout->setColumnStretch(1, 3);
        pGLayout->setColumnStretch(2, 2);
    }
    this->setLayout(pGLayout);

    // ★ Enter/Leave 会在鼠标进入子控件时"打断"：父窗口收到 Leave、子控件收到 Enter。
    //   所以给整棵子控件树都装上 eventFilter，任何子孙的 Enter/Leave 都当成整个条目的。
    installHoverRecursive(this);
}

void ChatItemBase::setUserName(const QString &name)
{
    m_pNameLabel->setText(name);
}

void ChatItemBase::setUserIcon(const QPixmap &icon)
{
    m_pIconLabel->setPixmap(icon);
}

void ChatItemBase::setWidget(QWidget *w)
{
   QGridLayout *pGLayout = (qobject_cast<QGridLayout *>)(this->layout());
   pGLayout->replaceWidget(m_pBubble, w);
   delete m_pBubble;
   m_pBubble = w;
   // 新气泡（可能带 QTextEdit 等子孙）也要纳入 hover 监听
   installHoverRecursive(w);
}

void ChatItemBase::setUniqueId(const QString &unique_id)
{
    m_uniqueId = unique_id;
}

void ChatItemBase::setStatus(int status)
{
    m_status = status;
    if(status == MsgStatus::UN_READ){
        m_pStatusLabel->setPixmap(QPixmap(":/res/unread.png"));
        m_pStatusLabel->setCursor(Qt::ArrowCursor);
        m_pStatusLabel->setToolTip("");
        return ;
    }

    if(status == MsgStatus::SEND_FAILED){
        m_pStatusLabel->setPixmap(QPixmap(":/res/send_fail.png"));
        // ★ 失败态：状态图标变成可点击的"重发"入口（手型 + 提示）
        m_pStatusLabel->setCursor(Qt::PointingHandCursor);
        m_pStatusLabel->setToolTip(tr("发送失败，点击重新发送"));
        return ;
    }

    if(status == MsgStatus::READED){
        m_pStatusLabel->setPixmap(QPixmap(":/res/readed.png"));
        m_pStatusLabel->setCursor(Qt::ArrowCursor);
        m_pStatusLabel->setToolTip("");
        return ;
    }
}

void ChatItemBase::setTime(const QString &timeText)
{
    m_timeText = timeText.trimmed();
    if (m_timeText.isEmpty()) {
        m_pTimeLabel->hide();
        return;
    }
    m_pTimeLabel->setText(m_timeText);
}

void ChatItemBase::layoutTimeLabel()
{
    if (m_timeText.isEmpty() || m_pBubble == nullptr) {
        return;
    }
    m_pTimeLabel->setText(m_timeText);
    m_pTimeLabel->adjustSize();
    const QSize sh = m_pTimeLabel->sizeHint();

    // ★ 贴在气泡旁边（垂直居中对齐气泡中线）：
    //   他人消息气泡在左 → 胶囊放气泡右侧；自己消息气泡在右 → 放左侧。
    //   用气泡实时几何定位，永远紧跟消息；不依赖条目宽度——
    //   早期版本用条目宽反推角落位置，布局未完成/宽度变化时会漂移，
    //   表现为"胶囊离消息老远"。
    const QRect bg = m_pBubble->geometry();
    int x;
    if (m_role == ChatRole::Self) {
        // 自己消息左侧有 16px 状态图标（已读/未读/发送失败），多留 24px 避开它
        x = bg.left() - sh.width() - 24;
    } else {
        x = bg.right() + 8;
    }
    // 夹在条目可见范围内，防止极端宽的气泡把胶囊挤出去
    x = qBound(2, x, qMax(2, width() - sh.width() - 2));
    int y = qBound(1, bg.center().y() - sh.height() / 2,
                   qMax(1, height() - sh.height() - 1));
    m_pTimeLabel->setGeometry(x, y, sh.width(), sh.height());
}

bool ChatItemBase::eventFilter(QObject *o, QEvent *e)
{
    // ★ 发送失败态下，点击状态图标（红色"!"）触发重发
    if (o == m_pStatusLabel && e->type() == QEvent::MouseButtonPress) {
        if (m_status == MsgStatus::SEND_FAILED && !m_uniqueId.isEmpty()) {
            emit sig_resend(m_uniqueId);
            return true;   // 吞掉这次按下事件，避免误触发 hover/其他逻辑
        }
    }
    if (m_pTimeLabel != nullptr && !m_timeText.isEmpty()) {
        if (e->type() == QEvent::Enter) {
            layoutTimeLabel();
            m_pTimeLabel->show();
            m_pTimeLabel->raise();
        } else if (e->type() == QEvent::Leave) {
            m_pTimeLabel->hide();
            // 强制整条重绘：子窗口隐藏后 Qt 只补绘它的矩形，
            // 若之前有过半透明合成残留（文字发灰鬼影），整条重绘可彻底清掉。
            update();
        }
    }
    return QWidget::eventFilter(o, e);
}

void ChatItemBase::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    // 宽度变了要重新定位时间标签（它贴在右/左上角，依赖当前宽度）
    if (m_pTimeLabel != nullptr && m_pTimeLabel->isVisible()) {
        layoutTimeLabel();
    }
}

void ChatItemBase::installHoverRecursive(QObject *obj)
{
    if (obj == nullptr) {
        return;
    }
    if (obj == m_pTimeLabel) {
        return;  // 时间标签自身不参与 hover 监听
    }
    if (QWidget *w = qobject_cast<QWidget *>(obj)) {
        w->installEventFilter(this);
        const QObjectList &kids = w->children();
        for (QObject *child : kids) {
            installHoverRecursive(child);
        }
    }
}
