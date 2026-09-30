#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
	:_ioc(ioc), 
	_acceptor(ioc,tcp::endpoint(tcp::v4(),port))
{

}

void CServer::Start() {
	auto self = shared_from_this();
	
	//在iocontext连接池里获取上下文
	auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
	//通过获取的上下文构造HttpConnection类，并利用类中的socket
	std::shared_ptr<HttpConnection> new_con = std::make_shared<HttpConnection>(io_context);

	_acceptor.async_accept(new_con->GetSocket(), [self,new_con](beast::error_code ec) {
		try {
			std::cout << "CServer Async_accept...\n";

			//出错就放弃这个连接，接续监听其他连接
			if (ec) {
				std::cout << "Async_wait error.\n\n";
				self->Start();
				return;
			}

			//如果接收到请求，HpptConnection类调用Start()管理新连接
			new_con->Start();

			//继续监听
			self->Start();
		}
		catch (std::exception& exp) {
			std::cerr << "CServer Error: " << exp.what() << std::endl;
		}
	});
		
}
