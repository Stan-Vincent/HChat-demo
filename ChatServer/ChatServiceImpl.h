#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <mutex>
#include "data.h"
#include "CServer.h"
#include <memory>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::ChatService;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;
using message::KickUserReq;
using message::KickUserRsp;
using message::AddGroupNotifyReq;
using message::AddGroupNotifyRsp;


class ChatServiceImpl final: public ChatService::Service
{
public:
	ChatServiceImpl();
	Status NotifyAddFriend(ServerContext* context, const AddFriendReq* request,
		AddFriendRsp* reply) override;

	Status NotifyAuthFriend(ServerContext* context, 
		const AuthFriendReq* request, AuthFriendRsp* response) override;

	Status NotifyTextChatMsg(::grpc::ServerContext* context, 
		const TextChatMsgReq* request, TextChatMsgRsp* response) override;

	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);

	//����rpc��������
	Status NotifyKickUser(::grpc::ServerContext* context,
		const KickUserReq* request, KickUserRsp* response) override;

	// 群成员变更实时通知（建群/加人后，把新群推给被邀请的在线成员）
	Status NotifyAddGroup(::grpc::ServerContext* context,
		const AddGroupNotifyReq* request, AddGroupNotifyRsp* response) override;

	void RegisterServer(std::shared_ptr<CServer> pServer);
private:
	std::shared_ptr<CServer> _p_server;
};

