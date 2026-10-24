#pragma once
// 共享的 Redis 连接池：ChatServer / GateServer 共用一份实现。
//
// 背景：原来是两份不同的实现——ChatServer 版有 PING 巡检 + 补齐但无退避、
// 巡检间隔 60 秒；GateServer 版什么都没有（Redis 一抖动，登录/验证码/聊天全失败
// 且不自愈）。已统一到这里，并修了两个问题：
//   1) reconnect() 里 `reply->type` 未判 NULL（redisCommand 失败时 reply 为 NULL，
//      直接解引用会崩）→ 先判空再判类型；
//   2) 巡检间隔从 60s 降到可配置（默认 5s），Redis 抖动后最多 5s 自愈；
//   3) 补齐连接改为指数退避（1s→2s→4s…上限 30s），避免 Redis 重启时
//      所有服务高频重连把它再打爆。
#include "hiredis.h"
#include "logger.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class RedisConPool {
public:
    // check_interval_sec：后台 PING 巡检间隔（秒）。Redis 抖动后最长这么久开始自愈。
    RedisConPool(size_t poolSize, const char* host, int port, const char* pwd,
                 int check_interval_sec = 5)
        : poolSize_(poolSize), host_(host), port_(port), pwd_(pwd),
          b_stop_(false), fail_count_(0), check_interval_(check_interval_sec),
          backoff_sec_(1), backoff_skip_(0) {
        // 初次建连：失败的先跳过，由后台巡检补齐（进程可能在 Redis 之前起来）
        for (size_t i = 0; i < poolSize_; ++i) {
            tryConnect();
        }
        size_t ok = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ok = connections_.size();
        }
        if (ok < poolSize_) {
            LOG_WARN("redis pool: {}/{} connections up, rest retried by background check",
                     ok, poolSize_);
        }
        check_thread_ = std::thread([this]() { checkLoop(); });
    }

    ~RedisConPool() {
        Close();
    }

    redisContext* getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] {
            if (b_stop_) {
                return true;
            }
            return !connections_.empty();
        });
        if (b_stop_) {
            return nullptr;
        }
        auto* context = connections_.front();
        connections_.pop();
        return context;
    }

    redisContext* getConNonBlock() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_ || connections_.empty()) {
            return nullptr;
        }
        auto* context = connections_.front();
        connections_.pop();
        return context;
    }

    void returnConnection(redisContext* context) {
        if (context == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_) {
            redisFree(context);
            return;
        }
        connections_.push(context);
        cond_.notify_one();
    }

    void ClearConnections() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!connections_.empty()) {
            redisFree(connections_.front());
            connections_.pop();
        }
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (b_stop_) {
                return;
            }
            b_stop_ = true;
        }
        cond_.notify_all();
        if (check_thread_.joinable()) {
            check_thread_.join();
        }
        ClearConnections();
    }

private:
    // 建一条新连接（含 AUTH）并入池。成功返回 true。
    bool tryConnect() {
        auto* context = redisConnect(host_.c_str(), port_);
        if (context == nullptr || context->err != 0) {
            if (context != nullptr) {
                redisFree(context);
            }
            return false;
        }
        auto* reply = (redisReply*)redisCommand(context, "AUTH %s", pwd_.c_str());
        if (reply == nullptr) {                 // ★ 先判空（旧实现直接解引用会崩）
            redisFree(context);
            return false;
        }
        const bool ok = (reply->type != REDIS_REPLY_ERROR);
        freeReplyObject(reply);
        if (!ok) {
            redisFree(context);
            return false;
        }
        returnConnection(context);
        return true;
    }

    // PING 探活：true = 连接健康
    bool ping(redisContext* context) {
        auto* reply = (redisReply*)redisCommand(context, "PING");
        if (reply == nullptr || context->err) {
            if (reply != nullptr) {
                freeReplyObject(reply);
            }
            return false;
        }
        const bool ok = (reply->type == REDIS_REPLY_STATUS);
        freeReplyObject(reply);
        return ok;
    }

    void checkLoop() {
        while (!b_stop_) {
            std::this_thread::sleep_for(std::chrono::seconds(check_interval_));
            if (b_stop_) {
                break;
            }
            checkThreadPro();
        }
    }

    // 逐个 PING 巡检；坏连接销毁并计数，再按需补齐（受指数退避约束）。
    void checkThreadPro() {
        size_t pool_size = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_size = connections_.size();
        }

        for (size_t i = 0; i < pool_size && !b_stop_; ++i) {
            auto* context = getConNonBlock();
            if (context == nullptr) {
                break;
            }
            if (ping(context)) {
                returnConnection(context);
            } else {
                redisFree(context);
                ++fail_count_;
                LOG_WARN("redis pool: dead connection dropped, fail_count = {}",
                         fail_count_.load());
            }
        }

        if (fail_count_ == 0) {
            return;
        }

        // 退避中：跳过本轮补齐（Redis 重启时别高频重连把它再打爆）
        if (backoff_skip_ > 0) {
            --backoff_skip_;
            return;
        }

        int made = 0;
        while (fail_count_ > 0 && !b_stop_) {
            if (!tryConnect()) {
                break;
            }
            --fail_count_;
            ++made;
        }
        if (made > 0) {
            LOG_INFO("redis pool: reconnected {} connection(s), fail_count = {}",
                     made, fail_count_.load());
            backoff_sec_ = 1;
            backoff_skip_ = 0;
        } else {
            // 一次都没成功：退避翻倍，按巡检间隔换算要跳过几轮
            backoff_skip_ = (backoff_sec_ / check_interval_);
            backoff_sec_ = (backoff_sec_ * 2 > 30) ? 30 : backoff_sec_ * 2;
            LOG_WARN("redis pool: reconnect failed, backoff to {}s", backoff_sec_);
        }
    }

    RedisConPool(const RedisConPool&) = delete;
    RedisConPool& operator=(const RedisConPool&) = delete;

    size_t poolSize_;
    std::string host_;
    int port_;
    std::string pwd_;
    std::atomic<bool> b_stop_;
    std::atomic<int> fail_count_;
    int check_interval_;
    int backoff_sec_;
    int backoff_skip_;

    std::queue<redisContext*> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread check_thread_;
};
