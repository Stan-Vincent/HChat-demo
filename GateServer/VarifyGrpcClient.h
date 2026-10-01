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

class RPConPool {
public:
    RPConPool(size_t poolsize, std::string host, std::string port);
    ~RPConPool();
    void Close();
    std::unique_ptr<VarifyService::Stub> getConnection();
    void returnConnection(std::unique_ptr<VarifyService::Stub> context);

private:
    std::atomic<bool> b_stop_;
    size_t poolsize_;
    std::string host_;
    std::string port_;
    std::queue<std::unique_ptr<VarifyService::Stub>> connections_;
    std::condition_variable cond_;
    std::mutex mutex_;
};

//封装的一个gRPC客户端，负责调用VarifyServer的GetVarifyCode服务。它用单例模式保证全局只有一个实例，避免每次都重新建立连接
class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;
public:

    GetVarifyRsp GetVarifyCode(std::string email) {
        ClientContext context;  //gRPC 的上下文，可传元数据、超时、取消

        // 设置 3 秒超时
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

        GetVarifyRsp reply;     //准备接收响应的对象
        GetVarifyReq request;   //构造请求对象
        request.set_email(email);//填充 email 字段

        //同步调用远程服务，结果填进 reply
        auto stub = pool_->getConnection();
        Status status = stub->GetVarifyCode(&context, request, &reply);

        //判断 RPC 本身是否成功
        if (status.ok()) {
            pool_->returnConnection(std::move(stub));
            return reply;
        }
        else {
            std::cout << "gRPC 调用失败: " << status.error_message() << std::endl;
            reply.set_error(ErrorCodes::RPCFailed);
            pool_->returnConnection(std::move(stub));
            return reply;
        }
    }

private:
    VerifyGrpcClient();
    std::unique_ptr<RPConPool> pool_;
};

