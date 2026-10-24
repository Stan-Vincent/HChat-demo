#pragma once
#include "Singleton.h"
#include <queue>
#include <thread>
#include "CSession.h"
#include <queue>
#include <map>
#include <functional>
#include "const.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <unordered_map>
#include "data.h"

class CServer;
typedef  function<void(shared_ptr<CSession>, const short &msg_id, const string &msg_data)> FunCallBack;
class LogicSystem:public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void PostMsgToQue(shared_ptr < LogicNode> msg);
	void SetServer(std::shared_ptr<CServer> pserver);
private:
	LogicSystem();
	void DealMsg();
	void RegisterCallBacks();
	void LoginHandler(shared_ptr<CSession> session, const short &msg_id, const string &msg_data);
	void SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void UpdateFriendBack(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	bool isPureDigit(const std::string& str);
	void GetUserByUid(std::string uid_str, Json::Value& rtvalue);
	void GetUserByName(std::string name, Json::Value& rtvalue);
	void GetUserByEmail(const std::string& email, Json::Value& rtvalue);
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> & user_list);
	void GetUserThreadsHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	bool GetUserThreads(int64_t userId,
		int64_t lastId,
		int      pageSize,
		std::vector<std::shared_ptr<ChatThreadInfo>>& threads,
		bool& loadMore,
		int& nextLastId);
	void CreatePrivateChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void LoadChatMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	// ---- group chat ----
	void CreateGroupChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void AddGroupMembers(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	// 群成员变更实时通知：把新群推给 targets 里的在线成员（建群/加人后调用）。
	// 离线用户跳过（下次登录 1018 拉会话时自然出现）；本服在线直发，跨服走 gRPC NotifyAddGroup。
	void NotifyGroupAdd(const std::vector<int>& targets, int thread_id,
		const std::string& name, int owner_id,
		const std::vector<int>& members, int operator_uid);
	std::thread _worker_thread;
	std::queue<shared_ptr<LogicNode>> _msg_que;
	std::mutex _mutex;
	std::condition_variable _consume;
	bool _b_stop;
	std::map<short, FunCallBack> _fun_callbacks;
	std::shared_ptr<CServer> _p_server;
};

