# -*- coding: utf-8 -*-
"""给 GateServer 加 /health 探活：Redis Ping + MySQL Ping。

涉及 6 个文件（全是 GBK+CRLF 字节补丁）：
  GateServer/RedisMgr.h     + bool Ping();
  GateServer/RedisMgr.cpp   + 实现（PING，坏连接直接丢弃让巡检补齐）
  GateServer/MysqlDao.h     + bool Ping();
  GateServer/MysqlDao.cpp   + 实现（SELECT 1）
  GateServer/MysqlMgr.h     + bool Ping();
  GateServer/MysqlMgr.cpp   + 转发
  GateServer/LogicSystem.cpp+ HandleGet 里加 /health 分支
"""
import sys

CRLF = '\r\n'
PATCHES = []


def patch(path, subs):
    PATCHES.append((path, subs))


# ---------- 声明 ----------
patch('GateServer/RedisMgr.h', [
    ('    bool ExistsKey(const std::string& key);',
     '    bool ExistsKey(const std::string& key);' + CRLF +
     '\t// 探活：PING 一下 Redis。失败时坏连接会被丢弃（不还池），交给后台巡检补齐。' + CRLF +
     '\tbool Ping();'),
])
patch('GateServer/MysqlDao.h', [
    ('    bool CheckEmail(const std::string& name, const std::string& email);',
     '    bool CheckEmail(const std::string& name, const std::string& email);' + CRLF +
     '    // 探活：执行 SELECT 1，用于 /health' + CRLF +
     '    bool Ping();'),
])
patch('GateServer/MysqlMgr.h', [
    ('\tbool CheckEmail(const std::string& name, const std::string& email);',
     '\tbool CheckEmail(const std::string& name, const std::string& email);' + CRLF +
     '\t// 探活：MySQL 是否可用，用于 /health' + CRLF +
     '\tbool Ping();'),
])

# ---------- 实现 ----------
patch('GateServer/RedisMgr.cpp', [
    ('bool RedisMgr::ExistsKey(const std::string& key)',
     'bool RedisMgr::Ping() {' + CRLF +
     '\tauto* context = _con_pool->getConnection();' + CRLF +
     '\tif (context == nullptr) {' + CRLF +
     '\t\treturn false;' + CRLF +
     '\t}' + CRLF +
     CRLF +
     '\tauto* reply = (redisReply*)redisCommand(context, "PING");' + CRLF +
     '\tconst bool ok = (reply != nullptr) && !context->err' + CRLF +
     '\t\t&& (reply->type == REDIS_REPLY_STATUS);' + CRLF +
     '\tif (reply != nullptr) {' + CRLF +
     '\t\tfreeReplyObject(reply);' + CRLF +
     '\t}' + CRLF +
     '\tif (ok) {' + CRLF +
     '\t\t_con_pool->returnConnection(context);' + CRLF +
     '\t} else {' + CRLF +
     '\t\t// 坏连接直接销毁：池里少一条，后台巡检会按 fail_count 补齐' + CRLF +
     '\t\tredisFree(context);' + CRLF +
     '\t}' + CRLF +
     '\treturn ok;' + CRLF +
     '}' + CRLF + CRLF +
     'bool RedisMgr::ExistsKey(const std::string& key) {'),
])
patch('GateServer/MysqlDao.cpp', [
    ('bool MysqlDao::CheckEmail(const std::string& name, const std::string& email)',
     'bool MysqlDao::Ping() {' + CRLF +
     '\tauto con = pool_->getConnection();' + CRLF +
     '\tif (con == nullptr) {' + CRLF +
     '\t\treturn false;' + CRLF +
     '\t}' + CRLF +
     '\tDefer defer([this, &con]() {' + CRLF +
     '\t\tpool_->returnConnection(std::move(con));' + CRLF +
     '\t\t});' + CRLF +
     CRLF +
     '\ttry {' + CRLF +
     '\t\tstd::unique_ptr<sql::Statement> stmt(con->_con->createStatement());' + CRLF +
     '\t\tstd::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));' + CRLF +
     '\t\treturn res->next();' + CRLF +
     '\t}' + CRLF +
     '\tcatch (sql::SQLException& e) {' + CRLF +
     '\t\tLOG_ERROR("mysql ping failed: {} (code {})", e.what(), e.getErrorCode());' + CRLF +
     '\t\treturn false;' + CRLF +
     '\t}' + CRLF +
     '}' + CRLF + CRLF +
     'bool MysqlDao::CheckEmail(const std::string& name, const std::string& email)'),
])
patch('GateServer/MysqlMgr.cpp', [
    ('bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email) {',
     'bool MysqlMgr::Ping() {' + CRLF + CRLF +
     '\treturn _dao.Ping();' + CRLF + CRLF +
     '}' + CRLF + CRLF +
     'bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email) {'),
])

# ---------- /health 路由 ----------
patch('GateServer/LogicSystem.cpp', [
    ('bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {',
     'bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {' + CRLF +
     '    // 健康检查：GET /health，不需要登录/token。' + CRLF +
     '    // 任一依赖不可用就返回 503，运维可据此直接告警（fail-fast 语义）。' + CRLF +
     '    if (path == "/health") {' + CRLF +
     '        const bool redis_ok = RedisMgr::GetInstance()->Ping();' + CRLF +
     '        const bool mysql_ok = MysqlMgr::GetInstance()->Ping();' + CRLF +
     '        Json::Value root;' + CRLF +
     '        root["status"] = (redis_ok && mysql_ok) ? "ok" : "degraded";' + CRLF +
     '        root["redis"] = redis_ok ? "ok" : "fail";' + CRLF +
     '        root["mysql"] = mysql_ok ? "ok" : "fail";' + CRLF +
     '        con->_response.result((redis_ok && mysql_ok)' + CRLF +
     '            ? http::status::ok : http::status::service_unavailable);' + CRLF +
     '        con->_response.set(http::field::content_type, "application/json");' + CRLF +
     '        beast::ostream(con->_response.body()) << root.toStyledString();' + CRLF +
     '        LOG_INFO("health check: redis={} mysql={}", redis_ok, mysql_ok);' + CRLF +
     '        return true;' + CRLF +
     '    }' + CRLF),
])

# ---------- 执行 ----------
for path, subs in PATCHES:
    raw = open(path, 'rb').read()
    try:
        text = raw.decode('gbk')
        enc = 'gbk'
    except Exception:
        text = raw.decode('utf-8')
        enc = 'utf-8'
    if '\ufffd' in text:
        print('FAIL 文件已含 U+FFFD:', path); sys.exit(1)

    if 'Ping()' in text and 'health check' in text:
        print('SKIP 已改过:', path); continue

    for old, new in subs:
        if new in text:
            continue                      # 幂等：这条已改
        if text.count(old) != 1:
            print('FAIL 锚点不唯一 in %s (%d 处): %s'
                  % (path, text.count(old), old[:50].replace(CRLF, ' ')))
            sys.exit(1)
        text = text.replace(old, new, 1)

    open(path, 'wb').write(text.encode(enc))
    print('OK', path, '(%s)' % enc)

print('health 补丁完成')
