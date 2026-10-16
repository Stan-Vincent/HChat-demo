#ifndef CHATUSERWID_H
#define CHATUSERWID_H

#include <QWidget>
#include <QLabel>
#include "listitembase.h"
#include "userdata.h"
namespace Ui {
class ChatUserWid;
}

class ChatUserWid : public ListItemBase
{
    Q_OBJECT

public:
    explicit ChatUserWid(QWidget *parent = nullptr);
    ~ChatUserWid();
    QSize sizeHint() const override;
    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);
    std::shared_ptr<ChatThreadData> GetChatData();
    void ShowRedPoint(bool bshow);
    void updateLastMsg(std::vector<std::shared_ptr<TextChatData>> msgs);
    // ★ 未读：后台消息累计 +1，点开会话清零；徽标是数字（>99 显示 99+）
    void addUnread(int n = 1);
    void clearUnread();
    // ★ 列表右侧时间：今天 12:00 / 昨天 / M月d日 / yyyy年M月d日
    void setLastTime(const QDateTime& when);
    static QString listTimeText(const QDateTime& when);
private:
    Ui::ChatUserWid *ui;
    std::shared_ptr<ChatThreadData> _chat_data;
    int _unread = 0;
    QLabel* _unread_lb = nullptr;   // 运行时创建的数字徽标（头像右上角）
};

#endif // CHATUSERWID_H
