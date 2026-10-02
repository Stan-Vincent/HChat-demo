#pragma once
#include "const.h"
#include <atomic>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "ConfigMgr.h"

class RedisConPool {
public:
    RedisConPool(size_t poolSize, const std::string& host, int port, const char* pwd);
    ~RedisConPool();

    redisContext* getConnection();           // 从池里借一个
    void returnConnection(redisContext* context);  // 归还
    void Close();                            // 关闭整个池

private:
    std::atomic<bool> b_stop_;               // 是否已关闭
    size_t poolSize_;                        // 池大小（5）
    std::string host_;
    int port_;
    std::queue<redisContext*> connections_;  // 装连接的队列
    std::mutex mutex_;                       // 保护队列的锁
    std::condition_variable cond_;           // 队列空时用来等待
};

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
    void Close();

private:
    RedisMgr();
    std::unique_ptr<RedisConPool> _con_pool; // 持有连接池
};