#include "AsioIOServicePool.h"
#include <iostream>
using namespace std;

AsioIOServicePool::AsioIOServicePool(std::size_t size):_ioServices(size),_works(size), _nextIOService(0)
{
	for (std::size_t i = 0; i < size; ++i) {
		_works[i] = std::make_unique<Work>(_ioServices[i].get_executor());
	}

	//遍历多个ioservice，创建多个线程，每个线程内部启动ioservice
	for (std::size_t i = 0; i < _ioServices.size(); ++i) {
		_threads.emplace_back([this, i]() {
			_ioServices[i].run();
		});
	}
}

AsioIOServicePool::~AsioIOServicePool() {
	Stop();
	std::cout << "AsioIOServicePool destruct" << endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
	auto& service = _ioServices[_nextIOService++];
	if (_nextIOService == _ioServices.size()) {
		_nextIOService = 0;
	}
	return service;
}

void AsioIOServicePool::Stop() {
	// 因为仅仅执行 work.reset 并不能让 io_context 从 run 的状态中退出
	// 当 io_context 已经绑定了读或写的监听事件后，还需要手动 stop 该服务
	for (std::size_t i = 0; i < _works.size(); ++i) {
		_ioServices[i].stop();   // 直接停对应的 io_context
		_works[i].reset();       // 释放 work，允许 io_context 退出
	}

	for (auto& t : _threads) {
		t.join();
	}
}
