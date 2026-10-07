#pragma once
#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"

class AsioIOServicePool:public Singleton<AsioIOServicePool>
{
	friend Singleton<AsioIOServicePool>;
public:
	//IOService == io_context
	using IOService = boost::asio::io_context;
	using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
	using WorkPtr = std::unique_ptr<Work>;

	~AsioIOServicePool();

	AsioIOServicePool(const AsioIOServicePool&) = delete;
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

	// 使用 round-robin 的方式返回一个 io_service(上下文)
	boost::asio::io_context& GetIOService();
	//停止线程，回收资源
	void Stop();

private:
	AsioIOServicePool(std::size_t size = 2/*std::thread::hardware_concurrency()*/);

	//存储上下文
	std::vector<IOService> _ioServices;

	std::vector<WorkPtr> _works;

	std::vector<std::thread> _threads;

	//下一个上下文的索引
	std::size_t _nextIOService;
};

