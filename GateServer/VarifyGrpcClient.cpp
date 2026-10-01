#include "VarifyGrpcClient.h"
#include "ConfigMgr.h"

RPConPool::RPConPool(size_t poolsize, std::string host, std::string port)
	:poolsize_(poolsize),host_(host),port_(port),b_stop_(false)
{
	for (size_t i = 0; i < poolsize_; i++) {
		//CreateChannel 建立到 VarifyServer 的 gRPC 通道（Insecure = 不加密，本地测试用）
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host_+":"+port_, grpc::InsecureChannelCredentials());
		//NewStub(channel) 创建一个 Stub（存根），后续所有 RPC 调用都通过它发出。
		connections_.push(VarifyService::NewStub(channel));
	}
}

RPConPool::~RPConPool()
{
	//加锁，防止析构时其他线程还在操作
	std::lock_guard<std::mutex> lock(mutex_);
	//Close() 把 b_stop_ 设为 true，并通知所有等待的线程
	Close();
	//清空队列（释放所有 stub）
	while (!connections_.empty()) {
		connections_.pop();
	}
}

void RPConPool::Close() {
	b_stop_ = true;
	//唤醒所有 cond_.wait 里等待的线程，让它们从 getConnection 里退出
	cond_.notify_all();
}

std::unique_ptr<VarifyService::Stub> RPConPool::getConnection()
{
	//加锁保护队列：多线程同时 getConnection 时，不会重复取到同一个元素
	std::unique_lock<std::mutex> lock(mutex_);
	//如果队列为空，线程会阻塞在 wait，直到别人 returnConnection 唤醒它
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
	//加锁保护队列：多线程同时归还时，不会破坏队列内部结构
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}
	if (!context) return;

	//把 stub 塞回队列
	connections_.push(std::move(context));
	//唤醒一个正在 wait 的线程（notify_one）
	cond_.notify_one();
}

GetVarifyRsp VerifyGrpcClient::GetVarifyCode(std::string email)
{
	ClientContext context;  //gRPC 的上下文，可传元数据、超时、取消

	// 设置 3 秒超时
	context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

	GetVarifyRsp reply;     //准备接收响应的对象
	GetVarifyReq request;   //构造请求对象
	request.set_email(email);//填充 email 字段

	//从池里借一个 stub,同步调用远程服务，结果填进 reply
	auto stub = pool_->getConnection();
	//如果借不到（池已关闭），返回错误
	if (!stub) {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}

	// RAII：离开作用域自动归还
	struct Guard {
		RPConPool* pool;
		std::unique_ptr<VarifyService::Stub>* stub;
		~Guard() {
			if (*stub) pool->returnConnection(std::move(*stub));
		}
	} guard{ pool_.get(), &stub };

	Status status = stub->GetVarifyCode(&context, request, &reply);

	if (!status.ok()) {
		std::cout << "gRPC 调用失败: " << status.error_message() << std::endl;
		reply.set_error(ErrorCodes::RPCFailed);
	}
	return reply;    // guard 在这里析构，自动归还 stub
}

//从 config.ini 读 VarifyServer 的地址和端口
VerifyGrpcClient::VerifyGrpcClient()
{
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["VarifyServer"]["Host"];
	std::string port = gCfgMgr["VarifyServer"]["Port"];
	pool_.reset(new RPConPool(5,host,port));
}
