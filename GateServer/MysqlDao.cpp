#include "MysqlDao.h"
#include "ConfigMgr.h"

// ============================================================
//                     MySqlPool 实现
// ============================================================

MySqlPool::MySqlPool(const std::string& host, int port,
    const std::string& user, const std::string& pass,
    const std::string& schema, int poolSize)
    : host_(host), port_(port), user_(user), pass_(pass),
    schema_(schema), poolSize_(poolSize), b_stop_(false), _fail_count(0)
{
    try {
        for (int i = 0; i < poolSize_; ++i) {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

            // ★ 用 ConnectOptionsMap 传参，不用 URL 字符串
            sql::ConnectOptionsMap opts;
            opts["hostName"] = host_;      // "127.0.0.1"
            opts["port"] = port_;      // 3308 (int)
            opts["userName"] = user_;      // "root"
            opts["password"] = pass_;      // "123456"
            opts["schema"] = schema_;    // "xxxl"

            auto* con = driver->connect(opts);
            // con->setSchema(schema_);  // 已经在 opts 里指定 schema 了，这行可省

            auto currentTime = std::chrono::system_clock::now().time_since_epoch();
            long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
            pool_.push(std::make_unique<SqlConnection>(con, timestamp));
        }

        std::cout << "MySQL 连接池初始化完成，成功连接数: " << pool_.size() << std::endl;

        _check_thread = std::thread([this]() {
            int counter = 0;
            while (!b_stop_.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (b_stop_.load()) break;
                if (++counter >= 60) {
                    counter = 0;
                    checkConnectionPro();
                }
            }
            });
    }
    catch (sql::SQLException& e) {
        std::cout << "mysql pool init failed, error is " << e.what() << std::endl;
    }
    catch (std::exception& e) {
        // 非 SQLException 也兜住，避免异常穿过连接池构造函数把整个进程打死
        std::cout << "mysql pool init threw: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "mysql pool init threw unknown exception" << std::endl;
    }
}


MySqlPool::~MySqlPool()
{
    // 1. 通知后台线程退出
    Close();

    // 2. 等线程结束（最多等 1 秒）
    if (_check_thread.joinable()) {
        _check_thread.join();
    }

    // 3. 清空连接队列（unique_ptr 析构时自动关闭每个连接）
    std::unique_lock<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        pool_.pop();
    }
}

void MySqlPool::Close()
{
    b_stop_ = true;
    cond_.notify_all();
}

//还连接
void MySqlPool::returnConnection(std::unique_ptr<SqlConnection> con) {
    if (con == nullptr) return;   // 空指针不收

    std::unique_lock<std::mutex> lock(mutex_);   // 加锁
    if (b_stop_) return;                          // 池关了就不还了

    pool_.push(std::move(con));                   // 塞进队尾
    cond_.notify_one();                           // 叫醒一个睡着的线程
}

//借连接
std::unique_ptr<SqlConnection> MySqlPool::getConnection() 
{
    std::unique_lock<std::mutex> lock(mutex_);   // 加锁

    cond_.wait(lock, [this] {                    // 等条件
        return b_stop_ || !pool_.empty();
    });

    if (b_stop_) 
        return nullptr;                 // 池关了，返回空

    auto con = std::move(pool_.front());         // 从队头取一条
    pool_.pop();                                 // 弹出队列
    return con;                                  // 返回给调用者
}


//连接保活
void MySqlPool::checkConnectionPro()
{
    // 1. 先记录当前池里有几个连接（检查过程中会变，所以要固定目标数）
    size_t targetCount = 0;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        targetCount = pool_.size();
    }

    size_t processed = 0;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    // 2. 逐个取出检查
    while (processed < targetCount) {
        std::unique_ptr<SqlConnection> con;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (pool_.empty()) break;
            con = std::move(pool_.front());
            pool_.pop();
        }
        //出锁后做 IO 检查，不阻塞其他线程借还连接

        bool healthy = true;

        // 距离上次使用超过 5 秒才探活（减少无谓 IO）
        if (timestamp - con->_last_oper_time >= 5) {
            try {
                std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
                stmt->executeQuery("SELECT 1");
                con->_last_oper_time = timestamp;
            }
            catch (sql::SQLException& e) {
                std::cout << "Error keeping connection alive: " << e.what() << std::endl;
                healthy = false;
                _fail_count++;
            }
        }

        if (healthy) {
            std::lock_guard<std::mutex> guard(mutex_);
            pool_.push(std::move(con));
            cond_.notify_one();
        }
        // 不健康 -> con 离开作用域，unique_ptr 自动关闭这个坏连接

        ++processed;
    }

    // 3. 补足失败的连接数
    while (_fail_count.load() > 0) {
        if (reconnect(timestamp)) {
            _fail_count--;
        }
        else {
            break;   // 重连失败，留着下次再补
        }
    }
}

bool MySqlPool::reconnect(long long timestamp)
{
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

        sql::ConnectOptionsMap opts;
        opts["hostName"] = host_;
        opts["port"] = port_;
        opts["userName"] = user_;
        opts["password"] = pass_;
        opts["schema"] = schema_;

        auto* con = driver->connect(opts);
        auto newCon = std::make_unique<SqlConnection>(con, timestamp);
        {
            std::lock_guard<std::mutex> guard(mutex_);
            pool_.push(std::move(newCon));
        }
        std::cout << "mysql connection reconnect success" << std::endl;
        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "Reconnect failed, error is " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
//                     MysqlDao 实现
// ============================================================

MysqlDao::MysqlDao()
{
    auto& cfg = ConfigMgr::Inst();
    const auto& host = cfg["Mysql"]["Host"];
    const auto& port = cfg["Mysql"]["Port"];
    const auto& pwd = cfg["Mysql"]["Passwd"];
    const auto& schema = cfg["Mysql"]["Schema"];
    const auto& user = cfg["Mysql"]["User"];

    // ★ 直接传 host 和 port，不拼 url
    pool_.reset(new MySqlPool(host, std::stoi(port), user, pwd, schema, 5));
}

MysqlDao::~MysqlDao()
{
    pool_->Close();
}

// 注册用户（通过存储过程）
int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
    auto con = pool_->getConnection();
    if (con == nullptr) {
        return -1;
    }
    // RAII：无论怎么退出，都自动归还连接
    Defer defer([this, &con]() {
        pool_->returnConnection(std::move(con));
        });

    try {
        // 准备调用存储过程
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1, name);
        stmt->setString(2, email);
        stmt->setString(3, pwd);

        // 执行存储过程
        stmt->execute();

        // 通过会话变量 @result 拿返回值（JDBC 不能直接注册输出参数）
        std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
        if (res->next()) {
            int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            return result;
        }
        return -1;
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return -1;
    }
}

// 注册用户（事务版本）
int MysqlDao::RegUserTransaction(const std::string& name, const std::string& email,
    const std::string& pwd)
{
    auto con = pool_->getConnection();
    if (con == nullptr) {
        return -1;
    }
    Defer defer([this, &con]() {
        //  借出去的连接在事务模式下，归还前必须还原为自动提交，
        //   否则这条连接会带着一个未结束的事务回到池里，污染后续使用者
        try {
            if (con) con->_con->setAutoCommit(true);
        }
        catch (...) {}
        pool_->returnConnection(std::move(con));
        });

    try {
        // 开始事务
        con->_con->setAutoCommit(false);

        // 1. 检查 email 是否已存在
        std::unique_ptr<sql::PreparedStatement> pstmt_email(
            con->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));
        pstmt_email->setString(1, email);
        std::unique_ptr<sql::ResultSet> res_email(pstmt_email->executeQuery());
        if (res_email->next()) {
            con->_con->rollback();
            std::cout << "email " << email << " exist" << std::endl;
            return 0;
        }

        // 2. 检查 name 是否已存在
        std::unique_ptr<sql::PreparedStatement> pstmt_name(
            con->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
        pstmt_name->setString(1, name);
        std::unique_ptr<sql::ResultSet> res_name(pstmt_name->executeQuery());
        if (res_name->next()) {
            con->_con->rollback();
            std::cout << "name " << name << " exist" << std::endl;
            return 0;
        }

        // 3. 更新 user_id 表的自增 id
        std::unique_ptr<sql::PreparedStatement> pstmt_upid(
            con->_con->prepareStatement("UPDATE user_id SET id = id + 1"));
        pstmt_upid->executeUpdate();

        // 4. 读取新 id
        std::unique_ptr<sql::PreparedStatement> pstmt_uid(
            con->_con->prepareStatement("SELECT id FROM user_id"));
        std::unique_ptr<sql::ResultSet> res_uid(pstmt_uid->executeQuery());
        int newId = 0;
        if (res_uid->next()) {
            newId = res_uid->getInt("id");
        }
        else {
            std::cout << "select id from user_id failed" << std::endl;
            con->_con->rollback();
            return -1;
        }

        // 5. 插入 user 表（★ 占位符有 6 个，6 个都必须绑定，少一个 executeUpdate 就会抛
        //      "Parameter 6 is not set"）
        std::unique_ptr<sql::PreparedStatement> pstmt_insert(
            con->_con->prepareStatement(
                "INSERT INTO user (uid, name, email, pwd, nick, icon) VALUES (?, ?, ?, ?, ?, ?)"));
        pstmt_insert->setInt(1, newId);
        pstmt_insert->setString(2, name);
        pstmt_insert->setString(3, email);
        pstmt_insert->setString(4, pwd);
        pstmt_insert->setString(5, name);   // nick：默认与用户名相同
        pstmt_insert->setString(6, "");     // icon：默认为空
        pstmt_insert->executeUpdate();

        // 6. 提交事务
        con->_con->commit();
        std::cout << "newuser insert into user success" << std::endl;
        return newId;
    }
    catch (sql::SQLException& e) {
        // 出错回滚
        try {
            con->_con->rollback();
        }
        catch (...) {}
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return -1;
    }
    catch (std::exception& e) {
        // 兜住非 SQLException（例如占位符没绑全、驱动抛的其它异常），
        // 返回 -1 让上层回一个错误码，而不是让整个进程直接挂掉
        try {
            con->_con->rollback();
        }
        catch (...) {}
        std::cerr << "RegUserTransaction exception: " << e.what() << std::endl;
        return -1;
    }
}

// 检查 email
bool MysqlDao::CheckEmail(const std::string& name, const std::string& email)
{
    auto con = pool_->getConnection();
    if (con == nullptr) {
        return false;
    }
    Defer defer([this, &con]() {
        pool_->returnConnection(std::move(con));
        });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
        pstmt->setString(1, name);

        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res->next()) {
            std::string dbEmail = res->getString("email");
            std::cout << "Check Email: " << dbEmail << std::endl;
            return (email == dbEmail);   // 直接返回，Defer 保证归还
        }
        return false;
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}

// 更新密码
bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd)
{
    auto con = pool_->getConnection();
    if (con == nullptr) {
        return false;
    }
    Defer defer([this, &con]() {
        pool_->returnConnection(std::move(con));
        });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
        pstmt->setString(1, newpwd);
        pstmt->setString(2, name);

        int updateCount = pstmt->executeUpdate();
        std::cout << "Updated rows: " << updateCount << std::endl;
        return true;
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}

// 检查密码
bool MysqlDao::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo)
{
    auto con = pool_->getConnection();
    if (con == nullptr) {
        return false;
    }
    Defer defer([this, &con]() {
        pool_->returnConnection(std::move(con));
        });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->_con->prepareStatement("SELECT * FROM user WHERE email = ?"));
        pstmt->setString(1, email);

        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        // 先判断有没有查到
        if (!res->next()) {
            return false;
        }

        std::string origin_pwd = res->getString("pwd");
        std::cout << "Password: " << origin_pwd << std::endl;

        if (pwd != origin_pwd) {
            return false;
        }

        userInfo.name = res->getString("name");
        userInfo.email = res->getString("email");
        userInfo.uid = res->getInt("uid");
        userInfo.pwd = origin_pwd;
        return true;
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}

// 测试存储过程（输出参数）
bool MysqlDao::TestProcedure(const std::string& email, int& uid, std::string& name)
{
    auto con = pool_->getConnection();
    if (con == nullptr) {
        return false;
    }
    Defer defer([this, &con]() {
        pool_->returnConnection(std::move(con));
        });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->_con->prepareStatement("CALL test_procedure(?,@userId,@userName)"));
        stmt->setString(1, email);
        stmt->execute();

        // 读输出参数 @userId
        std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @userId AS uid"));
        if (!res->next()) {
            return false;
        }
        uid = res->getInt("uid");
        std::cout << "uid: " << uid << std::endl;

        // 读输出参数 @userName
        stmtResult.reset(con->_con->createStatement());
        res.reset(stmtResult->executeQuery("SELECT @userName AS name"));
        if (!res->next()) {
            return false;
        }
        name = res->getString("name");
        std::cout << "name: " << name << std::endl;
        return true;
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}