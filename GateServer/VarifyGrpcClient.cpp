#include "VarifyGrpcClient.h"
#include "ConfigMgr.h"

RPConPool::RPConPool(size_t poolsize, std::string host, std::string port)
	:poolsize_(poolsize),host_(host),port_(port),b_stop_(false)
{
	for (size_t i = 0; i < poolsize_; i++) {
		//CreateChannel 建立到 127.0.0.1:50051 的 gRPC 通道（Insecure = 不加密，本地测试用）
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host_+":"+port_, grpc::InsecureChannelCredentials());
		//NewStub(channel) 创建一个 Stub（存根），后续所有 RPC 调用都通过它发出。
		connections_.push(VarifyService::NewStub(channel));
	}
}

RPConPool::~RPConPool()
{
	std::lock_guard<std::mutex> lock(mutex_);
	Close();
	while (!connections_.empty()) {
		connections_.pop();
	}
}

void RPConPool::Close() {
	b_stop_ = true;
	cond_.notify_all();
}

std::unique_ptr<VarifyService::Stub> RPConPool::getConnection()
{
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this]() {
		if (b_stop_) {
			return true;
		}
		return !connections_.empty();
	});

	if (b_stop_) {
		return nullptr;
	}

	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void RPConPool::returnConnection(std::unique_ptr<VarifyService::Stub> context)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}

	connections_.push(std::move(context));
	cond_.notify_one();
}

VerifyGrpcClient::VerifyGrpcClient()
{
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["VarifyServer"]["Host"];
	std::string port = gCfgMgr["VarifyServer"]["Port"];
	pool_.reset(new RPConPool(5,host,port));
}
