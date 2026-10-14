#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "userdata.h"
#include <QMap>
#include "chatitembase.h"
#include "uploadmanager.h"
#include "emojipanel.h"
#include <QPixmap>

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();
    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg);
    void UpdateChatStatus(QString unique_id, int status);
    // 发送失败重发：点击失败图标时由 ChatItemBase::sig_resend 触发
    void slot_resend(const QString& unique_id);
    // 服务端整体回包失败时，把仍在等待回执的消息全部标记成"发送失败"
    void FailAllPending();
protected:
    void paintEvent(QPaintEvent *event);
private slots:
    void on_send_btn_clicked();

    void on_receive_btn_clicked();

    // 表情功能：点输入框表情按钮切换面板 / 选中 emoji 插入输入框 / 面板关闭复位按钮
    void slot_toggle_emoji_panel(QString, ClickLbState);
    void slot_insert_emoji(const QString& emoji);
    void slot_emo_panel_hidden();

private:
    void clearItems();
    // 按消息类型造气泡：TEXT→TextBubble，PIC→异步下载的 PictureBubble，FILE→nullptr
    static QWidget* makeBubble(ChatMsgType type, const QString& content, ChatRole role);
    // 为某条已发出但未收到回执的消息启动超时计时（超时则标记发送失败）
    void armResendTimer(const QString& uuid);
    // 重发所需的上下文：文本存内容，图片存本地路径+预览
    struct ResendContext {
        QString content;            // 文本：文本内容；图片：本地路径（重发时重新上传）
        ChatMsgType type = ChatMsgType::TEXT;
        int other_id = 0;
        int thread_id = 0;
        QPixmap preview;            // 图片重发时复用的本地预览
    };
    Ui::ChatPage *ui;
    EmojiPanel* _emo_panel = nullptr;
    std::shared_ptr<ChatThreadData> _chat_data;
    QMap<QString, QWidget*>  _bubble_map;
    QHash<QString, ChatItemBase*> _unrsp_item_map;
    QHash<QString, ResendContext> _resend_ctx;   // 仅"本次会话发出、可重发"的消息
};

#endif // CHATPAGE_H
