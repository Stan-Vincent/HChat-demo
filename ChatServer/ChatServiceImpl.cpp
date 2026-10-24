#include "ChatServiceImpl.h"
#include "UserMgr.h"
#include "CSession.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "utils.h"
#include "logger.h"

ChatServiceImpl::ChatServiceImpl()
{

}

Status ChatServiceImpl::NotifyAddFriend(ServerContext* context, const AddFriendReq* request, AddFriendRsp* reply)
{
	//�����û��Ƿ��ڱ�������
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_applyuid(request->applyuid());
		reply->set_touid(request->touid());
		});

	//�û������ڴ�����ֱ�ӷ���
	if (session == nullptr) {
		return Status::OK;
	}
	
	//���ڴ�����ֱ�ӷ���֪ͨ�Է�
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["applyuid"] = request->applyuid();
	rtvalue["name"] = request->name();
	rtvalue["desc"] = request->desc();
	rtvalue["icon"] = request->icon();
	rtvalue["sex"] = request->sex();
	rtvalue["nick"] = request->nick();

	std::string return_str = rtvalue.toStyledString();

	session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyAddGroup(ServerContext* context, const AddGroupNotifyReq* request, AddGroupNotifyRsp* reply)
{
	// 目标用户是否在本 ChatServer 在线
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_thread_id(request->thread_id());
		});

	if (session == nullptr) {
		// 对方不在线：新群会在他下次登录（1018 拉会话）时自然出现，这里无需处理
		return Status::OK;
	}

	// 在本服，直接把群信息推过去。客户端据此建/刷新本地群会话。
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["thread_id"] = request->thread_id();
	rtvalue["name"] = request->name();
	rtvalue["owner_id"] = request->owner_id();
	rtvalue["operator_uid"] = request->operator_uid();
	for (int m : request->members()) {
		rtvalue["members"].append(m);
	}

	std::string return_str = rtvalue.toStyledString();
	session->Send(return_str, ID_NOTIFY_ADD_GROUP_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context, const AuthFriendReq* request,
	AuthFriendRsp* reply) {
	//�����û��Ƿ��ڱ�������
	auto touid = request->touid();
	auto fromuid = request->fromuid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_fromuid(request->fromuid());
		reply->set_touid(request->touid());
		});

	//�û������ڴ�����ֱ�ӷ���
	if (session == nullptr) {
		return Status::OK;
	}

	//���ڴ�����ֱ�ӷ���֪ͨ�Է�
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = request->fromuid();
	rtvalue["touid"] = request->touid();
	// �� ���������������Ϣ��ȡ thread_id��AuthFriendApply ���ѽ��ûỰ��
	int chat_thread_id = 0;
	if (request->textmsgs_size() > 0) {
		chat_thread_id = request->textmsgs(0).thread_id();
	}
	rtvalue["thread_id"] = chat_thread_id;

	std::string base_key = USER_BASE_INFO + std::to_string(fromuid);
	auto user_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, fromuid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	auto chat_time = getCurrentTimestamp();
	for(auto& msg : request->textmsgs()) {
		Json::Value  chat;
		chat["sender"] = msg.sender_id();
		chat["msg_id"] = msg.msg_id();
		chat["thread_id"] = msg.thread_id();
		chat["unique_id"] = msg.unique_id();
		chat["msg_content"] = msg.msgcontent();
		chat["chat_time"] = chat_time;
		chat["status"] = msg.status();
		rtvalue["chat_datas"].append(chat);
	}

	std::string return_str = rtvalue.toStyledString();

	session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
	return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context,
	const TextChatMsgReq* request, TextChatMsgRsp* reply) {
	//�����û��Ƿ��ڱ�������
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);
	reply->set_error(ErrorCodes::Success);

	//�û������ڴ�����ֱ�ӷ���
	if (session == nullptr) {
		return Status::OK;
	}

	//���ڴ�����ֱ�ӷ���֪ͨ�Է�
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = request->fromuid();
	rtvalue["touid"] = request->touid();
	rtvalue["thread_id"] = request->thread_id();
	//������������֯Ϊ����
	Json::Value text_array;
	for (auto& msg : request->textmsgs()) {
		Json::Value element;
		element["content"] = msg.msgcontent();
		element["unique_id"] = msg.unique_id();
		element["message_id"] = msg.msg_id();
		element["chat_time"] = msg.chat_time();
		// Image/file fields: mirror of LogicSystem::DealChatTextMsg, so a
		// message relayed from another ChatServer keeps its real type.
		element["msg_type"] = msg.msg_type();
		element["total_size"] = (Json::Int64)msg.total_size();
		element["md5"] = msg.md5();
		text_array.append(element);
	}
	rtvalue["chat_datas"] = text_array;

	std::string return_str = rtvalue.toStyledString();

	session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
	return Status::OK;
}


bool ChatServiceImpl::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
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
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString(), USER_BASE_INFO_TTL_SEC);
	}
	
	return true;
}

Status ChatServiceImpl::NotifyKickUser(::grpc::ServerContext* context, 
	const KickUserReq* request, KickUserRsp* reply)
{
	//�����û��Ƿ��ڱ�������
	auto uid = request->uid();
	auto session = UserMgr::GetInstance()->GetSession(uid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_uid(request->uid());
		});

	//�û������ڴ�����ֱ�ӷ���
	if (session == nullptr) {
		return Status::OK;
	}

	//���ڴ�����ֱ�ӷ���֪ͨ�Է�
	session->NotifyOffline(uid);
	//����ɵ�����
	_p_server->ClearSession(session->GetSessionId());

	return Status::OK;
}

void ChatServiceImpl::RegisterServer(std::shared_ptr<CServer> pServer)
{
	_p_server = pServer;
}
