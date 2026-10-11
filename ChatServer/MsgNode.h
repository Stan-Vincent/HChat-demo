#pragma once
#include <string>
#include "const.h"
#include <iostream>
#include <boost/asio.hpp>
using namespace std;
using boost::asio::ip::tcp;
class LogicSystem;
class MsgNode
{
public:
	// ★ 长度字段必须用 int，不能用 short：
	//   协议头的长度是 2 字节，单个响应体上限 65535，
	//   而 short 是有符号 16 位，超过 32767 就溢出成负数，
	//   导致 new char[负数] / memcpy(负长度) / 发半截数据。
	MsgNode(int max_len) :_total_len(max_len), _cur_len(0) {
		_data = new char[_total_len + 1]();
		_data[_total_len] = '\0';
	}

	~MsgNode() {
		std::cout << "destruct MsgNode" << endl;
		delete[] _data;
	}

	void Clear() {
		::memset(_data, 0, _total_len);
		_cur_len = 0;
	}

	int _cur_len;
	int _total_len;
	char* _data;
};

class RecvNode :public MsgNode {
	friend class LogicSystem;
public:
	RecvNode(int max_len, short msg_id);
private:
	short _msg_id;
};

class SendNode:public MsgNode {
	friend class LogicSystem;
public:
	SendNode(const char* msg, int max_len, short msg_id);
private:
	short _msg_id;
};

