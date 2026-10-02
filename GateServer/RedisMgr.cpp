#include "RedisMgr.h"

// 作用：函数退出时自动把连接归还给连接池，无论正常返回还是提前 return
namespace {
    struct ConnGuard {
        RedisConPool* pool;
        redisContext* ctx;
        ConnGuard(RedisConPool* p, redisContext* c) : pool(p), ctx(c) {}
        ~ConnGuard() {
            if (ctx) 
                pool->returnConnection(ctx);
        }
    };
}

// ==================== RedisMgr ====================

//从 config.ini 读 Redis 的地址、端口、密码
RedisMgr::RedisMgr() {
    auto& gCfgMgr = ConfigMgr::Inst();
    std::string host = gCfgMgr["Redis"]["Host"];
    std::string port = gCfgMgr["Redis"]["Port"];
    std::string pwd = gCfgMgr["Redis"]["Passwd"];
    _con_pool.reset(new RedisConPool(5, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}

RedisMgr::~RedisMgr() {
    Close();
}

void RedisMgr::Close() {
    _con_pool->Close();
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "GET %s", key.c_str());
    if (reply == nullptr) {
        std::cout << "[ GET " << key << " ] failed (reply null)" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_STRING) {
        std::cout << "[ GET " << key << " ] failed (not string)" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    std::cout << "Succeed to execute command [ GET " << key << " ]" << std::endl;
    return true;
}

bool RedisMgr::Set(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();// 借连接

    if (connect == nullptr) 
        return false;

    ConnGuard guard(_con_pool.get(), connect);// RAII 保证归还

    auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ SET " << key << " " << value << " ] failure (reply null)" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_STATUS ||
        (strcmp(reply->str, "OK") != 0 && strcmp(reply->str, "ok") != 0)) {
        std::cout << "Execut command [ SET " << key << " " << value << " ] failure (bad status)" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Execut command [ SET " << key << " " << value << " ] success" << std::endl;
    return true;
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ LPUSH " << key << " " << value << " ] failure (reply null)" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ LPUSH " << key << " " << value << " ] failure" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Execut command [ LPUSH " << key << " " << value << " ] success" << std::endl;
    return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "LPOP %s", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ LPOP " << key << " ] failure" << std::endl;
        if (reply) freeReplyObject(reply);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    std::cout << "Execut command [ LPOP " << key << " ] success" << std::endl;
    return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ RPUSH " << key << " " << value << " ] failure (reply null)" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ RPUSH " << key << " " << value << " ] failure" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Execut command [ RPUSH " << key << " " << value << " ] success" << std::endl;
    return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "RPOP %s", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ RPOP " << key << " ] failure" << std::endl;
        if (reply) freeReplyObject(reply);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    std::cout << "Execut command [ RPOP " << key << " ] success" << std::endl;
    return true;
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s",
        key.c_str(), hkey.c_str(), value.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ HSET " << key << " " << hkey << " " << value << " ] failure" << std::endl;
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Execut command [ HSET " << key << " " << hkey << " " << value << " ] success" << std::endl;
    return true;
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    const char* argv[4];
    size_t argvlen[4];
    argv[0] = "HSET";       argvlen[0] = 4;
    argv[1] = key;          argvlen[1] = strlen(key);
    argv[2] = hkey;         argvlen[2] = strlen(hkey);
    argv[3] = hvalue;       argvlen[3] = hvaluelen;

    auto reply = (redisReply*)redisCommandArgv(connect, 4, argv, argvlen);
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ HSET " << key << " " << hkey << " ] failure" << std::endl;
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Execut command [ HSET " << key << " " << hkey << " ] success" << std::endl;
    return true;
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return "";
    ConnGuard guard(_con_pool.get(), connect);

    const char* argv[3];
    size_t argvlen[3];
    argv[0] = "HGET";       argvlen[0] = 4;
    argv[1] = key.c_str();  argvlen[1] = key.length();
    argv[2] = hkey.c_str(); argvlen[2] = hkey.length();

    auto reply = (redisReply*)redisCommandArgv(connect, 3, argv, argvlen);
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ HGET " << key << " " << hkey << " ] failure" << std::endl;
        if (reply) freeReplyObject(reply);
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    std::cout << "Execut command [ HGET " << key << " " << hkey << " ] success" << std::endl;
    return value;
}

bool RedisMgr::Del(const std::string& key)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ DEL " << key << " ] failure" << std::endl;
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Execut command [ DEL " << key << " ] success" << std::endl;
    return true;
}

bool RedisMgr::ExistsKey(const std::string& key)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = (redisReply*)redisCommand(connect, "EXISTS %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "Not Found [ Key " << key << " ]" << std::endl;
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "Found [ Key " << key << " ] exists" << std::endl;
    return true;
}

// ==================== RedisConPool ====================

RedisConPool::RedisConPool(size_t poolSize, const std::string& host, int port, const char* pwd)
    : poolSize_(poolSize),
    host_(host),
    port_(port),
    b_stop_(false)
{
    for (size_t i = 0; i < poolSize_; ++i) {
        // 建立 TCP 连接
        auto* context = redisConnect(host_.c_str(), port_);

        if (context == nullptr || context->err != 0) {
            if (context != nullptr) {
                std::cout << "连接 Redis 失败: " << context->errstr << std::endl;
                redisFree(context);
            }
            continue;
        }

        auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd);
        if (reply == nullptr) {
            std::cout << "认证失败（reply 为空）" << std::endl;
            redisFree(context);
            continue;
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            std::cout << "认证失败: " << reply->str << std::endl;
            freeReplyObject(reply);
            redisFree(context);
            continue;
        }

        freeReplyObject(reply);
        std::cout << "认证成功" << std::endl;
        connections_.push(context);
    }

    std::cout << "连接池初始化完成，成功连接数: " << connections_.size() << std::endl;
}

RedisConPool::~RedisConPool() 
{
    Close();   // 先唤醒所有等待的线程
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) {
        redisFree(connections_.front());
        connections_.pop();
    }
}

redisContext* RedisConPool::getConnection()
{
    //加锁，防止多个线程同时操作队列
    std::unique_lock<std::mutex> lock(mutex_);
    //条件等待：如果队列为空（没连接可借），就挂起当前线程，并释放锁让别人用。
    //等别人归还连接并调用 notify_one()，才唤醒
    cond_.wait(lock, [this] {
        return b_stop_ || !connections_.empty();
        });

    if (b_stop_) return nullptr;

    auto* context = connections_.front();
    connections_.pop();
    return context;
}

void RedisConPool::returnConnection(redisContext* context)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (b_stop_) {
        redisFree(context);   // 池已关闭，直接释放，避免泄漏
        return;
    }
    connections_.push(context);
    cond_.notify_one();
}

void RedisConPool::Close()
{
    b_stop_ = true;
    cond_.notify_all();
}