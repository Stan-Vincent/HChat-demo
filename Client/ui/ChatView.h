#ifndef CHATVIEW_H
#define CHATVIEW_H
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <QPushButton>
#include "global.h"   // ChatRole 用于按发送者分组控制间距

class ChatView : public QWidget
{
    Q_OBJECT
public:
    ChatView(QWidget *parent = Q_NULLPTR);
    void appendChatItem(QWidget *item);                 //尾插
    void prependChatItem(QWidget *item);                //头插
    void insertChatItem(QWidget *before, QWidget *item);//中间插
    void removeAllItem();
    // 距底部多少像素内算"贴底"（贴底才自动跟随新消息）
    static constexpr int kStickBottomPx = 40;
signals:
    // 用户在翻历史时来了新消息 → 通知外部（外部可不理会）
    void sig_new_msg_while_scrolled_up(int count);
protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;   // 浮标跟随右下角
private slots:
    void onVScrollBarMoved(int min, int max);
    void onScrollBarValueChanged(int value);   // 用户自己滚回底部 → 清掉"新消息"提示

private:
    void initStyleSheet();
    // "N 条新消息"浮标：创建 / 文案更新 / 点它回到底部
    void ensureNewMsgButton();
    void updateNewMsgButton();
    void scrollToBottom();
    // 落点是否落在消息列表（本 ChatView）范围内
    bool isPosInsideChatView(const QPoint &globalPos) const;
    // 清除消息列表内所有 TextBubble 的文字选区
    void clearAllTextSelections();
private:
    //QWidget *m_pCenterWidget;
    QVBoxLayout *m_pVl;
    QScrollArea *m_pScrollArea;
    bool isAppended;
    // ★ 本次 append 是否该自动滚到底：在插入【之前】判断用户是否贴底
    bool _auto_scroll = true;
    // 用户翻历史期间累计的新消息条数（浮标文案）
    int _pending_new = 0;
    QPushButton* _new_msg_btn = nullptr;
    // P1 §3 间距：记录上一条"消息"的发送者，决定本条上方空隙（同人 4px / 换人 12px）
    ChatRole _last_msg_role = ChatRole::Other;
    bool _has_last = false;
};

#endif // CHATVIEW_H
