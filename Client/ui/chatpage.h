#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "userdata.h"
#include <QMap>
#include "chatitembase.h"
#include "uploadmanager.h"
#include "emojipanel.h"
#include "FileBubble.h"
#include <QPixmap>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QProgressBar>

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
    // ★ 登出清理：清掉渲染的消息气泡、未回执表、重发上下文和当前会话数据绑定
    void resetForLogout();
    // 发送失败重发：点击失败图标时由 ChatItemBase::sig_resend 触发
    void slot_resend(const QString& unique_id);
    // 服务端整体回包失败时，把仍在等待回执的消息全部标记成"发送失败"
    void FailAllPending();
    // 本人改完头像后调用：把已渲染气泡里"我"的头像刷成新图。
    // 只刷自己的气泡（对方的头像不归本地改），新气泡会自然用新头像。
    void refreshSelfAvatars();
protected:
    void paintEvent(QPaintEvent *event);
private slots:
    void on_send_btn_clicked();

    void on_receive_btn_clicked();

    // 表情功能：点输入框表情按钮切换面板 / 选中 emoji 插入输入框 / 面板关闭复位按钮
    void slot_toggle_emoji_panel(QString, ClickLbState);
    void slot_insert_emoji(const QString& emoji);
    void slot_emo_panel_hidden();

    // 文件功能：点输入框文件夹图标弹出选择框 / 点击文件气泡下载并打开
    void slot_pick_file(QString, ClickLbState);
    void slot_open_file(const QString& url, const QString& fileName);

private:
    // 给气泡挂上"右键菜单 + 图片点击看大图"（文本=复制，图片=另存为/复制，文件=打开/另存为）
    void attachBubbleActions(QWidget* bubble, const std::shared_ptr<ChatDataBase>& msg);
    // 图片灯箱已由 PictureBubble 自带（openImageLightbox），这里不再重复实现
    // 草稿：按 thread_id 存取输入框纯文本（切会话不串台）
    void saveDraft(int thread_id);
    void restoreDraft(int thread_id);
    // 时间分隔条：消息日期变化时插一条"今天 / 昨天 / yyyy年M月d日"
    void ensureDateSeparator(const QDateTime& when);
    QWidget* makeDateSeparator(const QString& text);
    // 人性化时间（悬停胶囊用）：今天 22:05 / 昨天 14:30 / 10月12日 09:00 / 2025年12月3日 08:00
    static QString friendlyTime(const QDateTime& when);

    void clearItems();
    // 下载文件气泡里的文件到 savePath；openAfter=存完直接打开，askOpen=存完询问是否打开
    void downloadFile(const QString& url, const QString& savePath,
                      bool openAfter, bool askOpen);
    // 按消息类型造气泡：TEXT→TextBubble，PIC→异步下载的 PictureBubble，
    // FILE→FileBubble（content 是 JSON {url,name,size}）
    static QWidget* makeBubble(ChatMsgType type, const QString& content, ChatRole role);
    // 为某条已发出但未收到回执的消息启动超时计时（超时则标记发送失败）
    void armResendTimer(const QString& uuid);

    // ---------- 上传进度 ----------
    // 开始一个上传任务，返回任务序号（用于把进度回调对上号）
    int beginUploadTask(const QString& fileName);
    // 更新某个任务的进度；total 可能是 -1（未知）
    void updateUploadProgress(int taskId, qint64 sent, qint64 total);
    // 任务结束（成功/失败都调）：摘掉它并按剩余任务刷新进度条
    void endUploadTask(int taskId);
    // 按所有在途任务汇总刷新进度条；没有在途任务就隐藏
    void refreshUploadBar();
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
    QHash<int, QString> _drafts;                 // thread_id → 未发送的输入草稿
    QSet<QString> _rendered_ids;                 // 已渲染过的 unique_id，防止同一条消息出两个气泡
    QDate _last_section_date;                    // 上一条消息所属日期（变了才插时间分隔条）
    // 待上传的文件：发送时收集，循环结束后逐个异步上传
    struct PendingFile {
        QString local_path;
        QString file_name;
        qint64  size = 0;
        QPixmap icon;
        QString uuid;
        int other_id = 0;
        int thread_id = 0;
        QPointer<FileBubble> bubble;   // 上传成功后回填服务端 url，供点击下载
    };
    QNetworkAccessManager* _nam = nullptr;   // 用于下载文件消息本体

    // ---------- 上传进度条 ----------
    // 图片/文件是"先上传拿到服务端地址、再发消息"，19MB 的文件 base64 后 26MB，
    // 慢网络下要好几分钟。没有进度条用户分不清是在传还是卡死。
    // 支持多个文件同时上传：按 taskId 记录各自进度，汇总成一条总进度。
    QProgressBar* _upload_bar = nullptr;                 // 输入框上方的细进度条
    int _upload_task_seq = 0;                            // 任务序号自增
    QHash<int, QPair<qint64, qint64>> _upload_tasks;     // taskId -> (已发送, 总计)
    QString _upload_names;                               // 在途文件名（进度条左侧文字用）
};

#endif // CHATPAGE_H
