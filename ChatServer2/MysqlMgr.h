#pragma once
#include "const.h"
#include "MysqlDao.h"
#include "Singleton.h"
#include <vector>
#include "message.pb.h"

class MysqlMgr: public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	int RegUser(const std::string& name, const std::string& email,  const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string & email);
	bool UpdatePwd(const std::string& name, const std::string& email);
	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
	bool AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name);
	bool UpdateFriendBack(int self_uid, int friend_uid, const std::string& back);
	bool AuthFriendApply(const int& from, const int& to);
	bool AddFriend(const int& from, const int& to, std::string back_name, std::vector<std::shared_ptr<AddFriendMsg>>& msg_list);
	std::shared_ptr<UserInfo> GetUser(int uid);

	std::shared_ptr<UserInfo> GetUserByEmail(const std::string& email);
	std::shared_ptr<UserInfo> GetUser(std::string name);
	bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit=10);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info);
	bool GetUserThreads(int64_t userId,
		int64_t lastId,
		int      pageSize,
		std::vector<std::shared_ptr<ChatThreadInfo>>& threads,
		bool& loadMore,
		int& nextLastId);

	bool CreatePrivateChat(int user1_id, int user2_id, int &thread_id);
	// See MysqlDao::GetPrivateChatUsers: verify a sender really belongs to the
	// thread it claims to write into.
	bool GetPrivateChatUsers(int thread_id, int& user1_id, int& user2_id);
	// ---- group chat (see MysqlDao for the contracts) ---------------------
	bool GetThreadType(int thread_id, std::string& type);
	bool GetGroupMembers(int thread_id, std::vector<int>& members);
	bool CreateGroupChat(const std::string& name, int owner_id,
		const std::vector<int>& members, int& thread_id);
	bool AddGroupMembers(int thread_id, const std::vector<int>& new_members);
	std::shared_ptr<PageResult> LoadChatMsg(int threadId, int lastId, int pageSize);
	bool AddChatMsg(std::vector<std::shared_ptr<ChatMessage>>& chat_datas);
private:
	MysqlMgr();
	MysqlDao  _dao;
};

