#include "RedisMgr.h"
#include "logger.h"

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

// build a command with redisCommandArgv instead of the %s formatter.
// hiredis' %s does NOT quote or escape: a key or value containing a space
// would be split into two arguments, and a '%' would be read as a format
// specifier. argv form is binary safe.
redisReply* RunCommand(redisContext* c, const std::vector<std::string>& args) {
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.reserve(args.size());
    argvlen.reserve(args.size());
    for (const auto& a : args) {
        argv.push_back(a.data());
        argvlen.push_back(a.size());
    }
    return (redisReply*)redisCommandArgv(
        c, static_cast<int>(argv.size()), argv.data(), argvlen.data());
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

    auto reply = RunCommand(connect, { "GET", key });
    if (reply == nullptr) {
        LOG_INFO("GET {} 失败（回复为空）", key);
        return false;
    }
    //key does not exist (expired / never requested) -> hiredis replies with NIL,
    //that is a normal case, not a type error, so tell the two apart
    if (reply->type == REDIS_REPLY_NIL) {
        LOG_INFO("GET {} 未命中（已过期或未写入）", key);
        freeReplyObject(reply);
        return false;
    }
    if (reply->type != REDIS_REPLY_STRING) {
        LOG_INFO("GET {} 失败（意外类型: {}）", key, reply->type);
        freeReplyObject(reply);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    LOG_INFO("GET {} 成功", key);
    return true;
}

bool RedisMgr::Set(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();// 借连接

    if (connect == nullptr) 
        return false;

    ConnGuard guard(_con_pool.get(), connect);// RAII 保证归还

    auto reply = RunCommand(connect, { "SET", key, value });
    if (reply == nullptr) {
        LOG_INFO("Redis SET {} {} 失败", key, value);
        return false;
    }
    if (reply->type != REDIS_REPLY_STATUS ||
        (strcmp(reply->str, "OK") != 0 && strcmp(reply->str, "ok") != 0)) {
        LOG_INFO("Redis SET {} {} 失败", key, value);
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("Redis SET {} {} 成功", key, value);
    return true;
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "LPUSH", key, value });
    if (reply == nullptr) {
        LOG_INFO("Redis LPUSH {} {} 失败", key, value);
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        LOG_INFO("Redis LPUSH {} {} 失败", key, value);
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("Redis LPUSH {} {} 成功", key, value);
    return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "LPOP", key });
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        LOG_INFO("Redis LPOP {} 失败", key);
        if (reply) freeReplyObject(reply);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    LOG_INFO("Redis LPOP {} 成功", key);
    return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "RPUSH", key, value });
    if (reply == nullptr) {
        LOG_INFO("Redis RPUSH {} {} 失败", key, value);
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        LOG_INFO("Redis RPUSH {} {} 失败", key, value);
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("Redis RPUSH {} {} 成功", key, value);
    return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "RPOP", key });
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        LOG_INFO("Redis RPOP {} 失败", key);
        if (reply) freeReplyObject(reply);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    LOG_INFO("Redis RPOP {} 成功", key);
    return true;
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "HSET", key, hkey, value });
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        LOG_INFO("Redis HSET {} {} {} 失败", key, hkey, value);
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("Redis HSET {} {} {} 成功", key, hkey, value);
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
        LOG_INFO("Redis HSET {} {} 失败", key, hkey);
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("Redis HSET {} {} 成功", key, hkey);
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
        LOG_INFO("Redis HGET {} {} 失败", key, hkey);
        if (reply) freeReplyObject(reply);
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    LOG_INFO("Redis HGET {} {} 成功", key, hkey);
    return value;
}

bool RedisMgr::Del(const std::string& key)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "DEL", key });
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        LOG_INFO("Redis DEL {} 失败", key);
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("Redis DEL {} 成功", key);
    return true;
}

bool RedisMgr::ExistsKey(const std::string& key)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) return false;
    ConnGuard guard(_con_pool.get(), connect);

    auto reply = RunCommand(connect, { "EXISTS", key });
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        LOG_ERROR("键 {} 未找到", key);
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_INFO("键 {} 已存在", key);
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
                LOG_INFO("连接 Redis 失败:  {}", context->errstr);
                redisFree(context);
            }
            continue;
        }

        auto reply = RunCommand(context, { "AUTH", std::string(pwd) });
        if (reply == nullptr) {
            LOG_INFO("认证失败（reply 为空）");
            redisFree(context);
            continue;
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            LOG_INFO("认证失败:  {}", reply->str);
            freeReplyObject(reply);
            redisFree(context);
            continue;
        }

        freeReplyObject(reply);
        LOG_INFO("认证成功");
        connections_.push(context);
    }

    LOG_INFO("连接池初始化完成，成功连接数:  {}", connections_.size());
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
    bool got = cond_.wait_for(lock, std::chrono::seconds(POOL_WAIT_TIMEOUT_SEC), [this] {
        return b_stop_ || !connections_.empty();
        });

    if (!got) {
        LOG_ERROR("Redis 连接池耗尽，等待连接超时（{}s）", POOL_WAIT_TIMEOUT_SEC);
        return nullptr;
    }

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