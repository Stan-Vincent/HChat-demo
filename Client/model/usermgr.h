#ifndef USERMGR_H
#define USERMGR_H
#include <QObject>
#include <memory>
#include <singleton.h>
#include "userdata.h"
#include <vector>
class UserMgr:public QObject,public Singleton<UserMgr>,
        public std::enable_shared_from_this<UserMgr>
{
    Q_OBJECT
public:
    friend class Singleton<UserMgr>;
    ~ UserMgr();
    void SetUserInfo(std::shared_ptr<UserInfo> user_info);
    void SetToken(QString token);
    QString GetToken();
    // Reset every per-login state: user info, token, apply list,
    // friends, chat threads (with messages) and paging counters.
    // Must be called on logout, otherwise the next account inherits
    // everything the previous one left behind (stale UI / wrong data).
    void Clear();
    int GetUid();
    QString GetName();
    QString GetNick();
    QString GetIcon();
    QString GetDesc();
    std::shared_ptr<UserInfo> GetUserInfo();
    // 提交资料成功后调用：把改动同步到内存里的 UserInfo。
    // 空串表示该字段这次没改，保留原值；不同步则头像/昵称要重新登录才刷新。
    void UpdateLocalUserInfo(const QString& nick, const QString& desc,
                              int sex, const QString& icon);
    void AppendApplyList(QJsonArray array);
    void AppendFriendList(QJsonArray array);
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList();
    void AddApplyList(std::shared_ptr<ApplyInfo> app);
    bool AlreadyApply(int uid);
    std::vector<std::shared_ptr<UserInfo>> GetChatListPerPage();
    bool IsLoadChatFin();
    void UpdateChatLoadedCount();
    std::vector<std::shared_ptr<UserInfo>> GetConListPerPage();
    // 全部好友（不走分页）：加群成员选人、匹配群成员昵称等场景用
    std::vector<std::shared_ptr<UserInfo>> GetAllFriends() const;
    void UpdateContactLoadedCount();
    // 联系人分页游标复位为 0：reloadFriendItems() 会先清空 UI 再从第 0 页重读，
    // 若不复位，构造时 addContactUserList() 可能已把游标推进到满页，
    // 重读时 begin>=_friend_list.size() 直接返回空 → 联系人列表空白（首次登录必现）。
    void ResetContactLoadedCount();
    bool IsLoadConFin();
    bool CheckFriendById(int uid);
    void AddFriend(std::shared_ptr<AuthRsp> auth_rsp);
    void AddFriend(std::shared_ptr<AuthInfo> auth_info);
    std::shared_ptr<UserInfo> GetFriendById(int uid);
    int GetLastChatThreadId();
    void SetLastChatThreadId(int id);
    void AddChatThreadData(std::shared_ptr<ChatThreadData> chat_thread_data, int other_uid);
    int GetThreadIdByUid(int uid);
    std::shared_ptr<ChatThreadData> GetChatThreadByThreadId(int thread_id);
    std::shared_ptr<ChatThreadData> GetChatThreadByUid(int thread_id);

    // 获取当前正在加载的会话数据
    std::shared_ptr<ChatThreadData> GetCurLoadData();
    std::shared_ptr<ChatThreadData> GetNextLoadData();
private:
    UserMgr();
    std::shared_ptr<UserInfo> _user_info;
    std::vector<std::shared_ptr<ApplyInfo>> _apply_list;
    std::vector<std::shared_ptr<UserInfo>> _friend_list;
    QMap<int, std::shared_ptr<UserInfo>> _friend_map;
    QString _token;
    int _chat_loaded;
    int _contact_loaded;
    // 建立会话id到数据的映射关系
    QMap<int, std::shared_ptr<ChatThreadData>> _chat_map;
    // 存储会话id列表
    std::vector<int> _chat_thread_ids;
    // 记录已经加载了多少列表的会话
    int _cur_load_chat_index;
    // 上一个会话id
    int _last_chat_thread_id;
    // 建立对方用户uid和thread_id的映射关系
    QMap<int, int> _uid_to_thread_id;

public slots:
    void SlotAddFriendRsp(std::shared_ptr<AuthRsp> rsp);
    void SlotAddFriendAuth(std::shared_ptr<AuthInfo> auth_info);
};

#endif // USERMGR_H
