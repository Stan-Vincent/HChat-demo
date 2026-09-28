#include "CServer.h"
#include "HttpConnection.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
	:_ioc(ioc), 
	_acceptor(ioc,tcp::endpoint(tcp::v4(),port)),
	_socket(ioc)
{}

void CServer::Start() {
	auto self = shared_from_this();
	_acceptor.async_accept(_socket, [self](beast::error_code ec) {
		try {
			std::cout << "Async_accept...(Cserver:Start())\n\n";
			//出错就放弃这个连接，接续监听其他连接
			if (ec) {
				std::cout << "Async_wait error.\n\n";
				self->Start();
				return;
			}

			//处理新链接，创建HpptConnection类管理新连接
			std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
			//继续监听
			self->Start();
		}
		catch (std::exception& exp) {

		}
		});
		
}
