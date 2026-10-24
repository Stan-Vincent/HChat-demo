#pragma once
#include "const.h"
#include <atomic>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "ConfigMgr.h"

#include "RedisConPool.h"   // 共享连接池（common/，替代原本地实现）
class RedisMgr : public Singleton<RedisMgr>
{
    friend class Singleton<RedisMgr>;
public:
    ~RedisMgr();

    bool Get(const std::string& key, std::string& value);
    bool Set(const std::string& key, const std::string& value);
    bool LPush(const std::string& key, const std::string& value);
    bool LPop(const std::string& key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    std::string HGet(const std::string& key, const std::string& hkey);
    bool Del(const std::string& key);
    bool ExistsKey(const std::string& key);
	// 探活：PING 一下 Redis。失败时坏连接会被丢弃（不还池），交给后台巡检补齐。
	bool Ping();
    void Close();

private:
    RedisMgr();
    std::unique_ptr<RedisConPool> _con_pool; // 持有连接池
};