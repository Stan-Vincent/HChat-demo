#pragma once
#include "const.h"
#include <mysql/mysql.h>
#include <thread>
#include <mysql/jdbc.h>

//给原生连接附加一个"最后使用时间"字段
class SqlConnection {
public:
    SqlConnection(sql::Connection* con, int64_t lasttime) 
    :_con(con), 
    _last_oper_time(lasttime) 
    {
    }
    std::unique_ptr<sql::Connection> _con;  // 原生数据库连接
    int64_t _last_oper_time;                // 最后一次使用的时间戳
};


class MySqlPool 
{
public:
    MySqlPool(const std::string& host, int port,
        const std::string& user, const std::string& pass,
        const std::string& schema, int poolSize);

    void checkConnectionPro();

    bool reconnect(long long timestamp);

    //借连接
    std::unique_ptr<SqlConnection> getConnection();

    //还连接
    void returnConnection(std::unique_ptr<SqlConnection> con);

    //关闭池
    void Close();

    ~MySqlPool();

private:
    std::string host_;
    int port_;
    std::string user_;
    std::string pass_;
    std::string schema_;
    int poolSize_;
    std::queue<std::unique_ptr<SqlConnection>> pool_;  // 存放连接的队列
    std::mutex mutex_;                                 // 一把锁
    std::condition_variable cond_;                     // 一个"叫醒服务"
    std::atomic<bool> b_stop_;                         // 是否关闭
    std::thread _check_thread;      //后台线程，定期跑保活检查
    std::atomic<int> _fail_count;    //记录失败连接数
};

struct UserInfo {
    std::string name;
    std::string pwd;
    int uid;
    std::string email;
};


class MysqlDao
{
public:
    MysqlDao();
    ~MysqlDao();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
    int RegUserTransaction(const std::string& name, const std::string& email, const std::string& pwd);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name, const std::string& newpwd);
    bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
    bool TestProcedure(const std::string& email, int& uid, std::string& name);
private:
    std::unique_ptr<MySqlPool> pool_;
};