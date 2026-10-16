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
    // �ύ�������ϳɹ��󣬰���ֵͬ�����ڴ棨����Ҫ���µ�¼����Ч��
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
    void UpdateContactLoadedCount();
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
    std::shared_ptr<ChatThreadData> GetChatThreadByUid(int uid);

    //��ȡ��ǰ���ڼ��ص��������ݡ�
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
    //�����Ựid�����ݵ�ӳ���ϵ
    QMap<int, std::shared_ptr<ChatThreadData>> _chat_map;
    //����Ựid�б�
    std::vector<int> _chat_thread_ids;
    //��¼�Ѿ����������б��ĻỰ����
    int _cur_load_chat_index;
    //�ϴλỰ��id
    int _last_chat_thread_id;
    //���������û�uid�������thread_id��ӳ���ϵ��
    QMap<int, int> _uid_to_thread_id;

public slots:
    void SlotAddFriendRsp(std::shared_ptr<AuthRsp> rsp);
    void SlotAddFriendAuth(std::shared_ptr<AuthInfo> auth);
};

#endif // USERMGR_H
