#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "DistLock.h"
#include <string>
#include "CServer.h"
#include "utils.h"
#include <vector>
#include "logger.h"

using namespace std;

LogicSystem::LogicSystem():_b_stop(false), _p_server(nullptr){
	RegisterCallBacks();
	_worker_thread = std::thread (&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem(){
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(shared_ptr < LogicNode> msg) {
	std::unique_lock<std::mutex> unique_lk(_mutex);
	_msg_que.push(msg);
	//��0��Ϊ1����֪ͨ�ź�
	if (_msg_que.size() == 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}


void LogicSystem::SetServer(std::shared_ptr<CServer> pserver) {
	_p_server = pserver;
}


void LogicSystem::DealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//�ж϶���Ϊ�������������������ȴ������ͷ���
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}

		//�ж��Ƿ�Ϊ�ر�״̬���������߼�ִ��������˳�ѭ��
		if (_b_stop ) {
			while (!_msg_que.empty()) {
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end()) {
					_msg_que.pop();
					continue;
				}
				try {
					call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
						std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				}
				catch (const std::exception& e) {
					// �� ���붵ס���κ�һ�� handler ���쳣������ std::stoi("") ��
					//   std::invalid_argument������ӳ�ȥ���쳣������������̣߳�
					//   ֱ�Ӵ��� std::terminate �á����� ChatServer ���̱�������
					//   �ͻ����Ǳ߱��־���"һֱתȦ�Ȼذ� -> ������ʱ -> ����"��
					std::cerr << "DealMsg: handler for msg_id "
						<< msg_node->_recvnode->_msg_id << " threw: " << e.what() << std::endl;
				}
				_msg_que.pop();
			}
			break;
		}

		//���û��ͣ������˵��������������
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			LOG_INFO("��ϢID {} �޶�Ӧ������", msg_node->_recvnode->_msg_id);
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id, 
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks() {
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_UPDATE_FRIEND_BACK_REQ] = std::bind(&LogicSystem::UpdateFriendBack, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_HEART_BEAT_REQ] = std::bind(&LogicSystem::HeartBeatHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_LOAD_CHAT_THREAD_REQ] = std::bind(&LogicSystem::GetUserThreadsHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	
	_fun_callbacks[ID_CREATE_PRIVATE_CHAT_REQ] = std::bind(&LogicSystem::CreatePrivateChat, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_LOAD_CHAT_MSG_REQ] = std::bind(&LogicSystem::LoadChatMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CREATE_GROUP_CHAT_REQ] = std::bind(&LogicSystem::CreateGroupChat, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_GROUP_MEMBER_REQ] = std::bind(&LogicSystem::AddGroupMembers, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short &msg_id, const string &msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	LOG_INFO("�����¼����: uid={}, token=[������]", uid);

	Json::Value  rtvalue;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, MSG_CHAT_LOGIN_RSP);
		});


	//��redis��ȡ�û�token�Ƿ���ȷ
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return ;
	}

	if (token_value != token) {
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return ;
	}

	rtvalue["error"] = ErrorCodes::Success;


	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;

	//�����ݿ��ȡ�����б�
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply) {
		for (auto& apply : apply_list) {
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	//��ȡ�����б�
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto& friend_ele : friend_list) {
		Json::Value obj;
		obj["name"] = friend_ele->name;
		obj["uid"] = friend_ele->uid;
		obj["icon"] = friend_ele->icon;
		obj["nick"] = friend_ele->nick;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		rtvalue["friend_list"].append(obj);
	}

	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	{
		//�˴����ӷֲ�ʽ�����ø��̶߳�ռ��¼
		//ƴ���û�ip��Ӧ��key
		auto lock_key = LOCK_PREFIX + uid_str;
		auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
		//����defer����
		Defer defer2([this, identifier, lock_key]() {
			RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
			});
		//�˴��жϸ��û��Ƿ��ڱ𴦻��߱���������¼

		std::string uid_ip_value = "";
		auto uid_ip_key = USERIPPREFIX + uid_str;
		bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
		//˵���û��Ѿ���¼�ˣ��˴�Ӧ���ߵ�֮ǰ���û���¼״̬
		if (b_ip) {
			//��ȡ��ǰ������ip��Ϣ
			auto& cfg = ConfigMgr::Inst();
			auto self_name = cfg["SelfServer"]["Name"];
			//���֮ǰ��¼�ķ������͵�ǰ��ͬ����ֱ���ڱ��������ߵ�
			if (uid_ip_value == self_name) {
				//���Ҿ��е�����
				auto old_session = UserMgr::GetInstance()->GetSession(uid);

				//�˴�Ӧ�÷���������Ϣ
				if (old_session) {
					old_session->NotifyOffline(uid);
					//����ɵ�����
					_p_server->ClearSession(old_session->GetSessionId());
				}

			}
			else {
				//������Ǳ�����������֪ͨgrpc֪ͨ�����������ߵ�
				//����֪ͨ
				KickUserReq kick_req;
				kick_req.set_uid(uid);
				ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
			}
		}

		//session���û�uid
		session->SetUserId(uid);
		//Ϊ�û����õ�¼ip server������
		std::string  ipkey = USERIPPREFIX + uid_str;
		RedisMgr::GetInstance()->Set(ipkey, server_name);
		//uid��session�󶨹���,�����Ժ����˲���
		UserMgr::GetInstance()->SetUserSession(uid, session);
		std::string  uid_session_key = USER_SESSION_PREFIX + uid_str;
		RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());

	}

	return;
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	LOG_INFO("�����û���Ϣ");

	Json::Value  rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP);
		});

	// �������򣺴����� -> �� uid �飻�� @ -> ������飻�����û����顣
	// ����"�Ӻ���"֧�� uid / �ǳ� / �����������뷽ʽ��
	// �� ������ֱ�ӻش�����������²� ���� ��ʡһ�β�ѯ��Ҳ���������ת���Ŀ�
	if (uid_str.empty()) {
	    rtvalue["error"] = ErrorCodes::UidInvalid;   // ö����û�� ErrParams���� UidInvalid
	    LOG_ERROR("�����ַ���Ϊ��");
	    return;
	}

	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		GetUserByUid(uid_str, rtvalue);
	}
	else if (uid_str.find('@') != std::string::npos) {
		GetUserByEmail(uid_str, rtvalue);
	}
	else {
		GetUserByName(uid_str, rtvalue);
	}
	return;
}

void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto desc = root["applyname"].asString();
	auto bakname = root["bakname"].asString();
	auto touid = root["touid"].asInt();
	LOG_INFO("���Ӻ�������: uid={}, ��ע={}, �ǳ�={}, Ŀ��uid={}", uid, desc, bakname, touid);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_FRIEND_RSP);
		});

	//�ȸ������ݿ�
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid, desc, bakname);

	//��ѯredis ����touid��Ӧ��server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}


	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];


	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	//ֱ��֪ͨ�Է���������Ϣ
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//���ڴ�����ֱ�ӷ���֪ͨ�Է�
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = apply_info->name;
			notify["desc"] = desc;
			if (b_info) {
				notify["icon"] = apply_info->icon;
				notify["sex"] = apply_info->sex;
				notify["nick"] = apply_info->nick;
			}
			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
		}

		return ;
	}

	
	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);
	add_req.set_name(apply_info->name);
	add_req.set_desc(desc);
	if (b_info) {
		add_req.set_icon(apply_info->icon);
		add_req.set_sex(apply_info->sex);
		add_req.set_nick(apply_info->nick);
	}

	//����֪ͨ
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value,add_req);

}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto back_name = root["back"].asString();
	LOG_INFO("�û� {} ��֤���� {}", uid, touid);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid);
	bool b_info = GetBaseInfo(base_key, touid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
		rtvalue["uid"] = touid;
		// �� ����ش� thread_id���ͻ��� slot_auth_rsp ������ ChatThreadData��
		//   ȱ�˻��� auth_rsp->_thread_id ��Ϊ 0���� _uid_to_thread_id[�Զ�] ���ǳ� 0
		//   ����¼ʱ��д����ʵֵ����ͬһ�Ự�� UserMgr �� _chat_thread_items �����һ�ݡ�
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}


	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP);
		});

	//�ȸ������ݿ⣬ �ŵ������У��˴����ٴ���
	//MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	std::vector<std::shared_ptr<AddFriendMsg>> chat_datas;

	//�������ݿ����Ӻ���
	MysqlMgr::GetInstance()->AddFriend(uid, touid,back_name, chat_datas);

    // �� �� AddFriend �������Ϣ��ȡ thread_id�����ڲ����� chat_thread + private_chat��
    int chat_thread_id = 0;
    if (!chat_datas.empty()) {
        chat_thread_id = chat_datas[0]->thread_id();
    }
	// �ش� thread_id���ͻ��� slot_auth_rsp ������ ChatThreadData��
	//   ȱ�˻��� auth_rsp->_thread_id ��Ϊ 0���� _uid_to_thread_id[�Զ�] ���ǳ� 0��
	rtvalue["thread_id"] = chat_thread_id;

	//��ѯredis ����touid��Ӧ��server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//ֱ��֪ͨ�Է�����֤ͨ����Ϣ
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//���ڴ�����ֱ�ӷ���֪ͨ�Է�
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info) {
				notify["name"] = user_info->name;
				notify["nick"] = user_info->nick;
				notify["icon"] = user_info->icon;
				notify["sex"] = user_info->sex;
			}
			else {
				notify["error"] = ErrorCodes::UidInvalid;
			}

			auto chat_time = getCurrentTimestamp();
			for(auto & chat_data : chat_datas)
			{
				Json::Value  chat;
				chat["sender"] = chat_data->sender_id();
				chat["msg_id"] = chat_data->msg_id();
				chat["thread_id"] = chat_data->thread_id();
				chat["unique_id"] = chat_data->unique_id();
				chat["msg_content"] = chat_data->msgcontent();
				chat["msg_type"] = chat_data->msg_type();
				chat["total_size"] = (Json::Int64)chat_data->total_size();
				chat["md5"] = chat_data->md5();
				chat["chat_time"] = chat_time;
				chat["status"] = chat_data->status();
				notify["chat_datas"].append(chat);
				rtvalue["chat_datas"].append(chat);
			}

			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
		}

		return ;
	}


	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);
	auto chat_time = getCurrentTimestamp();
	for(auto& chat_data : chat_datas)
	{
		auto text_msg = auth_req.add_textmsgs();
		text_msg->CopyFrom(*chat_data);
		Json::Value  chat;
		chat["sender"] = chat_data->sender_id();
		chat["msg_id"] = chat_data->msg_id();
		chat["thread_id"] = chat_data->thread_id();
		chat["unique_id"] = chat_data->unique_id();
		chat["msg_content"] = chat_data->msgcontent();
		chat["msg_type"] = chat_data->msg_type();
		chat["total_size"] = (Json::Int64)chat_data->total_size();
		chat["md5"] = chat_data->md5();
		chat["chat_time"] = chat_time;
		chat["status"] = chat_data->status();
		rtvalue["chat_datas"].append(chat);
	}
	//����֪ͨ
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}


void LogicSystem::UpdateFriendBack(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto friend_uid = root["friend_uid"].asInt();
	auto back = root["back"].asString();
	LOG_INFO("modify friend remark: uid={}, friend_uid={}, back={}", uid, friend_uid, back);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["friend_uid"] = friend_uid;
	rtvalue["back"] = back;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_UPDATE_FRIEND_BACK_RSP);
	});

	if (!MysqlMgr::GetInstance()->UpdateFriendBack(uid, friend_uid, back)) {
		rtvalue["error"] = ErrorCodes::Error_MySql;
		return;
	}
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	const Json::Value  arrays = root["text_array"];
	
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	auto thread_id = root["thread_id"].asInt();
	rtvalue["thread_id"] = thread_id;

	// ==== Never trust thread_id / touid coming from the client ====
	// A forged or stale thread_id would write this message into somebody else's
	// conversation -- exactly how cross-user rows once got into chat_message
	// ("three people talking in one chat"). So: look the thread up, require the
	// sender to be a member, and derive the receiver(s) from the thread instead
	// of believing the client.
	// NOTE: Defer is declared here (before any early return) so every exit path
	// still answers the client.
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
		});

	// Threads come in two flavours and the membership rule differs:
	//   private -> exactly two members, the receiver is the other one
	//   group   -> N members, every member except the sender gets a copy
	std::string thread_type;
	if (!MysqlMgr::GetInstance()->GetThreadType(thread_id, thread_type)) {
		LOG_ERROR("DealChatTextMsg: thread {} not found (uid={}, touid={})", thread_id, uid, touid);
		rtvalue["error"] = ErrorCodes::Error_MySql;
		return;
	}
	rtvalue["thread_type"] = thread_type;

	// Everyone who must receive this message.  The sender is never in here: it
	// already gets the very same chat_datas back in the response below.
	std::vector<int> recipients;

	if (thread_type == "group") {
		std::vector<int> members;
		if (!MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members)) {
			LOG_ERROR("DealChatTextMsg: group thread {} has no member rows (uid={})",
				thread_id, uid);
			rtvalue["error"] = ErrorCodes::Error_MySql;
			return;
		}
		bool in_group = false;
		for (int m : members) {
			if (m == uid) {
				in_group = true;
				continue;
			}
			recipients.push_back(m);
		}
		if (!in_group) {
			LOG_ERROR("DealChatTextMsg: uid {} is not a member of group thread {}", uid, thread_id);
			rtvalue["error"] = ErrorCodes::UidInvalid;
			return;
		}
		// A group message has no single receiver: chat_message.recv_id stores 0
		// and every member is addressed by thread_id instead.
		touid = 0;
		rtvalue["touid"] = 0;
	}
	else if (thread_type == "private") {
		int thread_u1 = 0, thread_u2 = 0;
		if (!MysqlMgr::GetInstance()->GetPrivateChatUsers(thread_id, thread_u1, thread_u2)) {
			LOG_ERROR("DealChatTextMsg: thread {} not found (uid={}, touid={})", thread_id, uid, touid);
			rtvalue["error"] = ErrorCodes::Error_MySql;
			return;
		}
		if (uid != thread_u1 && uid != thread_u2) {
			LOG_ERROR("DealChatTextMsg: uid {} is not a member of thread {}", uid, thread_id);
			rtvalue["error"] = ErrorCodes::UidInvalid;
			return;
		}
		const int peer_uid = (uid == thread_u1) ? thread_u2 : thread_u1;
		if (touid != peer_uid) {
			LOG_WARN("DealChatTextMsg: touid {} != peer {} of thread {}, fixed",
				touid, peer_uid, thread_id);
			touid = peer_uid;
			rtvalue["touid"] = touid;
		}
		recipients.push_back(peer_uid);
	}
	else {
		LOG_ERROR("DealChatTextMsg: unknown thread type '{}' for thread {}", thread_type, thread_id);
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	std::vector<std::shared_ptr<ChatMessage>> chat_datas;
	auto timestamp = getCurrentTimestamp();
	for (const auto& txt_obj : arrays) {
		auto content = txt_obj["content"].asString();
		auto unique_id = txt_obj["unique_id"].asString();
		// �� ͼ����Ϣ���ͻ��˲��� msg_type ʱ���ı�(0)������
		//   content �ĺ��������ͱ仯���ı�=���ģ�ͼƬ/�ļ�=�������ϵ����·����
		auto msg_type = txt_obj.get("msg_type", 0).asInt();
		auto total_size = txt_obj.get("total_size", 0).asInt64();
		auto md5 = txt_obj.get("md5", "").asString();
		LOG_INFO("��Ϣ����: {}", content);
		LOG_INFO("unique_id: {}", unique_id);
		LOG_INFO("��Ϣ����: {}", msg_type);
		// �Ƿ�����һ�ɵ��ı�
		if (msg_type < 0 || msg_type > 2) {
			msg_type = 0;
		}
		// ͼƬ/�ļ�������·��������ͻ���ֻ����ʾ��һ��·���ı�
		if (msg_type != 0 && content.empty()) {
			msg_type = 0;
		}
		auto chat_msg = std::make_shared<ChatMessage>();
		chat_msg->chat_time = timestamp;
		chat_msg->sender_id = uid;
		chat_msg->recv_id = touid;
		chat_msg->unique_id = unique_id;
		chat_msg->thread_id = thread_id;
		chat_msg->content = content;
		chat_msg->msg_type = msg_type;
		chat_msg->total_size = (uint64_t)total_size;
		chat_msg->md5 = md5;
		chat_msg->status = 2;
		chat_datas.push_back(chat_msg);
	}


	//�������ݿ�
	MysqlMgr::GetInstance()->AddChatMsg(chat_datas);


	for (const auto& chat_data : chat_datas) {
		Json::Value  chat_msg;
		chat_msg["message_id"] = chat_data->message_id;
		chat_msg["unique_id"] = chat_data->unique_id;
		chat_msg["content"] = chat_data->content;
		chat_msg["status"] = chat_data->status;
		chat_msg["chat_time"] = chat_data->chat_time;
		// �� ͼ����Ϣ�ֶΣ��ͻ��˿� msg_type ������Ⱦ���ı����ݻ���ͼƬ
		chat_msg["msg_type"] = chat_data->msg_type;
		chat_msg["total_size"] = (Json::Int64)chat_data->total_size;
		chat_msg["md5"] = chat_data->md5;
		rtvalue["chat_datas"].append(chat_msg);
	}

	// ---- deliver to every recipient --------------------------------------
	// A private chat has exactly one recipient, a group has one per member.
	// They may live on different ChatServers, so the routing below
	// (redis USERIPPREFIX -> local session, else gRPC) runs once per uid.
	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	for (int ruid : recipients) {
		auto to_str = std::to_string(ruid);
		auto to_ip_key = USERIPPREFIX + to_str;
		std::string to_ip_value = "";
		bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
		if (!b_ip) {
			// Offline: the row is already in MySQL, the client pulls history later.
			LOG_INFO("DealChatTextMsg: uid {} offline, message kept in db", ruid);
			continue;
		}

		// Same payload for everyone; only touid differs, and that is what the
		// receiving client matches against itself.
		Json::Value notify = rtvalue;
		notify["touid"] = ruid;

		if (to_ip_value == self_name) {
			auto to_session = UserMgr::GetInstance()->GetSession(ruid);
			if (to_session) {
				to_session->Send(notify.toStyledString(), ID_NOTIFY_TEXT_CHAT_MSG_REQ);
			}
			continue;
		}

		TextChatMsgReq text_msg_req;
		text_msg_req.set_fromuid(uid);
		text_msg_req.set_touid(ruid);
		text_msg_req.set_thread_id(thread_id);
		for (const auto& chat_data : chat_datas) {
			auto *text_msg = text_msg_req.add_textmsgs();
			text_msg->set_unique_id(chat_data->unique_id);
			text_msg->set_msgcontent(chat_data->content);
			text_msg->set_msg_id(chat_data->message_id);
			text_msg->set_chat_time(chat_data->chat_time);
			// Image/file fields.  Without them the other ChatServer rebuilds the
			// payload as plain text and the receiver shows a raw server path.
			text_msg->set_msg_type(chat_data->msg_type);
			text_msg->set_total_size((int64_t)chat_data->total_size);
			text_msg->set_md5(chat_data->md5);
		}

		ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, notify);
	}


}
void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["fromuid"].asInt();
	LOG_INFO("�յ�������uid: {}", uid);
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	session->Send(rtvalue.toStyledString(), ID_HEARTBEAT_RSP);
}

bool LogicSystem::isPureDigit(const std::string& str)
{
	for (char c : str) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	//���Ȳ�redis�в�ѯ�û���Ϣ
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		LOG_INFO("�û���Ϣ: uid={}, name={}, email={}, icon={}", uid, name, email, icon);

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = safeToInt(uid_str, 0);
	// �� ԭ�������� std::stoi(uid_str)���մ�/�����ֻ��� std::invalid_argument��
	//   �� DealMsg �Ĺ����߳�һ�����쳣�ӳ�ȥ���� std::terminate -> �������̱���
	//redis��û�����ѯmysql
	//��ѯ���ݿ�
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	//�����ݿ�����д��redis����
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString(), USER_BASE_INFO_TTL_SEC);

	//��������
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

void LogicSystem::GetUserByName(std::string name, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	//���Ȳ�redis�в�ѯ�û���Ϣ
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		LOG_INFO("�û���Ϣ: uid={}, name={}, email={}", uid, name, email);

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	//redis��û�����ѯmysql
	//��ѯ���ݿ�
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	//�����ݿ�����д��redis����
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString(), USER_BASE_INFO_TTL_SEC);
	
	//��������
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

//��������û����Ӻ���ʱ֧��������������
//���ⲻ�� Redis����������Ƶ�ʵͣ���û�� emailcache_* ���໺�����
void LogicSystem::GetUserByEmail(const std::string& email, Json::Value& rtvalue)
{
	auto user_info = MysqlMgr::GetInstance()->GetUserByEmail(email);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_ERROR("�������ѯδ�ҵ�: {}", email);
		return;
	}

	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = user_info->uid;
	rtvalue["name"] = user_info->name;
	rtvalue["nick"] = user_info->nick;
	rtvalue["email"] = user_info->email;
	rtvalue["icon"] = user_info->icon;
	rtvalue["sex"] = user_info->sex;
	rtvalue["desc"] = user_info->desc;
	LOG_INFO("�������ѯ����: uid={}, name={}", user_info->uid, user_info->name);
}


bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	//���Ȳ�redis�в�ѯ�û���Ϣ
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		LOG_INFO("�û���¼�ɹ�: uid={}, name={}, email={}", userinfo->uid, userinfo->name, userinfo->email);
	}
	else {
		//redis��û�����ѯmysql
		//��ѯ���ݿ�
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			return false;
		}

		userinfo = user_info;

		//�����ݿ�����д��redis����
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		// �� �� TTL д�룺��ǰ�����û��棬�������ݿ� user.icon �󱾻��û����õ���ֵ
		//   ���Է�ȴ��������Ϊ�����б�����·��ֱ�� MySQL �ƹ��˻��棩���ǳ����Ų顣
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString(), USER_BASE_INFO_TTL_SEC);
	}

	return true;
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list) {
	//��mysql��ȡ���������б�
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	//��mysql��ȡ�����б�
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}

void LogicSystem::GetUserThreadsHandler(std::shared_ptr<CSession> session, 
	const short& msg_id, const string& msg_data)
{
	//�����ݿ��chat_threads��¼
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	int last_id = root["thread_id"].asInt();
	LOG_INFO("��ȡuid: {}", uid);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_THREAD_RSP);
		});
	
	std::vector<std::shared_ptr<ChatThreadInfo>> threads;
	
	int page_size = 10;
	bool load_more = false;
	int next_last_id = 0;
	bool res = GetUserThreads(uid, last_id, page_size, threads, load_more, next_last_id);
	if (!res) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}


	rtvalue["load_more"] = load_more;
	rtvalue["next_last_id"] = (int)next_last_id;
	//����threads����д��json����
	for (auto& thread : threads) {
		Json::Value thread_value;
		thread_value["thread_id"] = int(thread->_thread_id);
		thread_value["type"] = thread->_type;
		thread_value["user1_id"] = thread->_user1_id;
		thread_value["user2_id"] = thread->_user2_id;
		// Group threads carry a display name; private ones send an empty string.
		thread_value["name"] = thread->_name;
		// Group threads also carry their member list (empty array for private).
		// Older clients ignore unknown fields, so this stays compatible.
		for (int m_uid : thread->_members) {
			thread_value["members"].append(m_uid);
		}
		rtvalue["threads"].append(thread_value);
	}
}

bool LogicSystem::GetUserThreads(int64_t userId,
	int64_t lastId,
	int      pageSize,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads,
	bool& loadMore,
	int& nextLastId)
{
	return MysqlMgr::GetInstance()->GetUserThreads(userId, lastId, pageSize, 
		threads, loadMore, nextLastId);
}

void LogicSystem::CreatePrivateChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto other_id = root["other_id"].asInt();
	
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["other_id"] = other_id;

	// �� У��˫�� uid��CreatePrivateChat �������� uid1 = min(uid, other_id)��
	//   ֻҪ��һ���� 0���ͻ��� private_chat д��һ�� user1_id=0 ����Ự��
	//   ֮�����пͻ��˼��ػỰ�б�������Ϊ GetFriendById(0) �õ� nullptr �����ˡ�
	//   �ͻ�����ʾ����ʱ����� uid=0 �ļٺ��ѣ��Ѿ����������������ݡ�
	if (uid <= 0 || other_id <= 0 || uid == other_id) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CREATE_PRIVATE_CHAT_RSP);
		});

	int thread_id = 0;
	bool res = MysqlMgr::GetInstance()->CreatePrivateChat(uid, other_id, thread_id);
	if (!res) {
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		return;
	}

	rtvalue["thread_id"] = thread_id;
}

void LogicSystem::LoadChatMsg(std::shared_ptr<CSession> session, 
	const short& msg_id, const string& msg_data) {

	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto thread_id = root["thread_id"].asInt();
	auto message_id = root["message_id"].asInt();


	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["thread_id"] = thread_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_MSG_RSP);
		});

	int page_size = 10;
	std::shared_ptr<PageResult> res = MysqlMgr::GetInstance()->LoadChatMsg(thread_id, message_id, page_size);
	if (!res) {
		rtvalue["error"] = ErrorCodes::LOAD_CHAT_FAILED;
		return;
	}

	rtvalue["last_message_id"] = res->next_cursor;
	rtvalue["load_more"] = res->load_more;
	for (auto& chat : res->messages) {
		Json::Value  chat_data;
		chat_data["sender"] = chat.sender_id;
		chat_data["msg_id"] = chat.message_id;
		chat_data["thread_id"] = chat.thread_id;
		chat_data["unique_id"] = 0;
		chat_data["msg_content"] = chat.content;
		chat_data["msg_type"] = chat.msg_type;
		chat_data["total_size"] = (Json::Int64)chat.total_size;
		chat_data["md5"] = chat.md5;
		chat_data["chat_time"] = chat.chat_time;
		chat_data["status"] = chat.status;
		rtvalue["chat_datas"].append(chat_data);
	}

	// �� ���ֽ�Ԥ��ü���װ���¾��ٷż��������� load_more �ÿͻ��˼�������
	//   Ӳ�ػ�� JSON ���ϣ�ʵ����� \u4e2d ��·�����ͻ��˽�����ʧ�ܡ�
	while (rtvalue["chat_datas"].size() > 0) {
		std::string tmp = rtvalue.toStyledString();
		if ((int)tmp.size() <= MSG_BODY_SAFE) {
			break;
		}
		Json::Value arr = rtvalue["chat_datas"];
		int lastIdx = arr.size() - 1;
		int dropped_id = arr[lastIdx]["msg_id"].asInt();
		rtvalue["chat_datas"].removeIndex(lastIdx, nullptr);
		rtvalue["load_more"] = true;
		// �α���˵����������һ�������´δ���֮��ʼ
		if (rtvalue["chat_datas"].size() > 0) {
			int n = rtvalue["chat_datas"].size() - 1;
			rtvalue["last_message_id"] = rtvalue["chat_datas"][n]["msg_id"].asInt();
		} else {
			// һ����װ���£����������ͳ��ޣ����� ����ԭ�α꣬������ѭ��
			rtvalue["last_message_id"] = res->next_cursor;
		}
		std::cerr << "LoadChatMsg: ��Ӧ���� " << MSG_BODY_SAFE
			<< " �ֽڣ����� msg_id=" << dropped_id << std::endl;
	}
}

// ---------------------------------------------------------------------------
// Ⱥ�ģ���Ⱥ / ���ˡ�
// ��һ��Ⱥ�ĵĴ�������� SQL���������ӿ��Ǹ��ͻ���/���Խű��õ�����ͨ����
// ��Ⱥ = chat_thread + group_chat + group_chat_member ����д�루һ�����񣩣�
// ���� = ֻ�������г�Ա���𣬱��⿿�� thread_id ���Լ��������˵�Ⱥ��
// ---------------------------------------------------------------------------
void LogicSystem::NotifyGroupAdd(const std::vector<int>& targets, int thread_id,
	const std::string& name, int owner_id,
	const std::vector<int>& members, int operator_uid) {
	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	for (int ruid : targets) {
		if (ruid == operator_uid) {
			continue;   // 触发者已在 1034/1036 拿到结果，无需再推
		}
		auto to_ip_key = USERIPPREFIX + std::to_string(ruid);
		std::string to_ip_value;
		if (!RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value)) {
			continue;   // 离线：下次登录 1018 拉会话时自然出现
		}

		if (to_ip_value == self_name) {
			auto session = UserMgr::GetInstance()->GetSession(ruid);
			if (session) {
				Json::Value notify;
				notify["error"] = ErrorCodes::Success;
				notify["thread_id"] = thread_id;
				notify["name"] = name;
				notify["owner_id"] = owner_id;
				notify["operator_uid"] = operator_uid;
				for (int m : members) {
					notify["members"].append(m);
				}
				session->Send(notify.toStyledString(), ID_NOTIFY_ADD_GROUP_REQ);
			}
			continue;
		}

		AddGroupNotifyReq req;
		req.set_touid(ruid);
		req.set_thread_id(thread_id);
		req.set_name(name);
		req.set_owner_id(owner_id);
		req.set_operator_uid(operator_uid);
		for (int m : members) {
			req.add_members(m);
		}
		ChatGrpcClient::GetInstance()->NotifyAddGroup(to_ip_value, req);
	}
}

void LogicSystem::CreateGroupChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto name = root["name"].asString();

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["name"] = name;

	// The creator becomes the owner AND the first member row, so it has to be
	// a real uid -- otherwise group_chat_member ends up holding uid 0 and every
	// fan-out in DealChatTextMsg would try to deliver to a ghost.
	if (uid <= 0) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CREATE_GROUP_CHAT_RSP);
		});

	std::vector<int> members;
	for (const auto& m : root["members"]) {
		int muid = m.asInt();
		if (muid <= 0 || muid == uid) {
			continue;
		}
		members.push_back(muid);
	}

	// Hard cap: DealChatTextMsg loops over every member per message, and the DAO
	// inserts one row per member.  200 keeps both bounded.
	const size_t kMaxGroupMembers = 200;
	if (members.size() + 1 > kMaxGroupMembers) {
		LOG_ERROR("CreateGroupChat: uid {} tried to create a group of {} members",
			uid, members.size() + 1);
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	int thread_id = 0;
	if (!MysqlMgr::GetInstance()->CreateGroupChat(name, uid, members, thread_id)) {
		rtvalue["error"] = ErrorCodes::CREATE_GROUP_FAILED;
		return;
	}

	rtvalue["thread_id"] = thread_id;

	// 实时把新群推给被邀请的成员：他们无需重登就能在会话列表看到群
	std::vector<int> all_members = members;
	all_members.push_back(uid);
	NotifyGroupAdd(members, thread_id, name, uid, all_members, uid);
	LOG_INFO("����Ⱥ��: thread={}, owner={}, ��Ա��={}", thread_id, uid, members.size() + 1);
}

void LogicSystem::AddGroupMembers(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto thread_id = root["thread_id"].asInt();
	auto name = root["name"].asString();

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["thread_id"] = thread_id;

	if (uid <= 0 || thread_id <= 0) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_GROUP_MEMBER_RSP);
		});

	// Only somebody already in the group may invite.  Without this check a
	// client could push itself (or a victim) into any group by guessing ids.
	std::vector<int> members;
	if (!MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members)) {
		rtvalue["error"] = ErrorCodes::Error_MySql;
		return;
	}
	bool in_group = false;
	for (int m : members) {
		if (m == uid) {
			in_group = true;
			break;
		}
	}
	if (!in_group) {
		LOG_ERROR("AddGroupMembers: uid {} is not in thread {}", uid, thread_id);
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	std::vector<int> new_members;
	for (const auto& m : root["members"]) {
		int muid = m.asInt();
		if (muid <= 0) {
			continue;
		}
		bool dup = false;
		for (int exist : members) {
			if (exist == muid) {
				dup = true;
				break;
			}
		}
		if (!dup) {
			new_members.push_back(muid);
		}
	}

	// Nothing new to add: still Success, just a no-op.
	if (new_members.empty()) {
		return;
	}

	if (!MysqlMgr::GetInstance()->AddGroupMembers(thread_id, new_members)) {
		rtvalue["error"] = ErrorCodes::ADD_GROUP_MEMBER_FAILED;
		return;
	}

	rtvalue["added"] = (int)new_members.size();

	// 实时通知：新成员建会话、老成员刷新成员列表（无需重登）
	std::vector<int> all_members = members;
	all_members.insert(all_members.end(), new_members.begin(), new_members.end());
	NotifyGroupAdd(all_members, thread_id, name, 0, all_members, uid);
	LOG_INFO("Ⱥ�ļ���: thread={}, ����={}", thread_id, new_members.size());
}
