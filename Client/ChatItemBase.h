#ifndef CHATITEMBASE_H
#define CHATITEMBASE_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include "global.h"
class BubbleFrame;

class ChatItemBase : public QWidget
{
    Q_OBJECT
public:
    explicit ChatItemBase(ChatRole role, QWidget *parent = nullptr);
    void setUserName(const QString &name);
    void setUserIcon(const QPixmap &icon);
    void setWidget(QWidget *w);
    void setStatus(int status);
    // 设置本条消息的时间文本；为空时 hover 不显示时间标签
    void setTime(const QString &timeText);
    // 设置本条消息的客户端唯一id，供发送失败重发时定位气泡
    void setUniqueId(const QString &unique_id);
signals:
    // 点击"发送失败"图标时发出，携带该消息的 unique_id
    void sig_resend(const QString &unique_id);
protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
private:
    ChatRole m_role;
    QLabel *m_pNameLabel;
    QLabel *m_pIconLabel;
    QWidget *m_pBubble;
    QLabel * m_pStatusLabel;
    QLabel *m_pTimeLabel = nullptr;   // hover 时浮出的时间标签
    QString m_timeText;
    QString m_uniqueId;               // 本消息客户端唯一id（失败重发用）
    int m_status = -1;                // 当前状态，用于判断是否处于"发送失败"可重发
    void layoutTimeLabel();
    void installHoverRecursive(QObject *obj);
};

#endif // CHATITEMBASE_H
