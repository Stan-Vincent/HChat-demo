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
        Status status = stub_->GetVarifyCode(&context, request, &reply);

        //判断 RPC 本身是否成功
        if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
            std::cout << "gRPC 调用超时" << std::endl;
        }
        else {
            std::cout << "gRPC 调用失败: " << status.error_message() << std::endl;
        }
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }

private:
    VerifyGrpcClient() {
        //CreateChannel 建立到 127.0.0.1:50051 的 gRPC 通道（Insecure = 不加密，本地测试用）
        std::shared_ptr<Channel> channel = grpc::CreateChannel("127.0.0.1:50051", grpc::InsecureChannelCredentials());
        //NewStub(channel) 创建一个 Stub（存根），后续所有 RPC 调用都通过它发出。
        stub_ = VarifyService::NewStub(channel);
    }
    std::shared_ptr<Channel> channel_;
    std::unique_ptr<VarifyService::Stub> stub_;
};

