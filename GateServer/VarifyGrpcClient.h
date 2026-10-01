#pragma once
#include "const.h"
#include "message.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "Singleton.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVarifyReq;
using message::GetVarifyRsp;
using message::VarifyService;


/*
*grpc连接池
*内部维护一个队列，队列里是 gRPC 的 stub
*getConnection 从队列取一个，returnConnection 还回去
*mutex + cond_ 保证多线程安全
*/
class RPConPool {
public:
    RPConPool(size_t poolsize, std::string host, std::string port);
    ~RPConPool();
    void Close();
    std::unique_ptr<VarifyService::Stub> getConnection();
    void returnConnection(std::unique_ptr<VarifyService::Stub> context);

private:
    std::atomic<bool> b_stop_;                                  // 是否已关闭
    size_t poolsize_;                                           // 池大小
    std::string host_;                                          // VarifyServer 地址
    std::string port_;                                          // 端口
    std::queue<std::unique_ptr<VarifyService::Stub>> connections_; // 连接队列
    std::condition_variable cond_;                              // 条件变量
    std::mutex mutex_;                                          // 互斥锁
};

//封装的一个gRPC客户端，负责调用VarifyServer的GetVarifyCode服务。它用单例模式保证全局只有一个实例，避免每次都重新建立连接
class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;
public:

    GetVarifyRsp GetVarifyCode(std::string email);

private:
    VerifyGrpcClient();
    std::unique_ptr<RPConPool> pool_;
};

