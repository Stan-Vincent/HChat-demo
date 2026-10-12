# -*- coding: utf-8 -*-
"""Task17: 新增"修改好友备注"消息（1031/1032）。
- const.h(GBK)/global.h: ReqId + Error_MySql 错误码
- MysqlDao: UpdateFriendBack(self_uid, friend_uid, back)
- MysqlMgr: 转发
- LogicSystem: handler + 注册
全部插入 ASCII 代码与 ASCII 注释，与文件编码无关。
"""
def patch(path, fixes):
    b = open(path, 'rb').read()
    crlf = b'\r\n' in b
    conv = lambda s: s.replace(b'\n', b'\r\n') if crlf else s
    for old, new, cnt in fixes:
        o, n = conv(old), conv(new)
        if n in b:
            print('SKIP (already applied) %s: %r' % (path, new[:50]))
            continue
        c = b.count(o)
        assert c == cnt, '%s: expect %d, got %d for %r' % (path, cnt, c, o[:70])
        b = b.replace(o, n)
    open(path, 'wb').write(b)
    print('OK %s (%d fixes, %s)' % (path, len(fixes), 'CRLF' if crlf else 'LF'))

REQID = (b'\tID_UPDATE_FRIEND_BACK_REQ = 1031, //modify friend remark request\n'
         b'\tID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response\n')

# ---------- 1. ChatServer/const.h（GBK）：ReqId + 错误码 ----------
patch('ChatServer2/const.h', [
    # 错误码：LOAD_CHAT_FAILED 行后加 Error_MySql（锚点只取 ASCII 部分）
    (b'LOAD_CHAT_FAILED = 1013,',
     b'LOAD_CHAT_FAILED = 1013,\n\tError_MySql = 1014, //database error', 1),
    # ReqId：ID_LOAD_CHAT_MSG_RSP = 1030, 行后加
    (b'ID_LOAD_CHAT_MSG_RSP = 1030,',
     b'ID_LOAD_CHAT_MSG_RSP = 1030,\n' + REQID, 1),
])

# ---------- 2. Client/global.h ----------
patch('Client/global.h', [
    (b'    ID_LOAD_CHAT_MSG_RSP = 1030,',
     b'    ID_LOAD_CHAT_MSG_RSP = 1030,\n'
     b'    ID_UPDATE_FRIEND_BACK_REQ = 1031, //modify friend remark request\n'
     b'    ID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response', 1),
])

# ---------- 3. MysqlDao.h 声明 ----------
patch('ChatServer2/MysqlDao.h', [
    (b'\tbool AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name);',
     b'\tbool AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name);\n'
     b'\tbool UpdateFriendBack(int self_uid, int friend_uid, const std::string& back);', 1),
])

# ---------- 4. MysqlDao.cpp 实现（插在 UpdatePwd 实现后） ----------
UPDATE_BACK_IMPL = b'''
bool MysqlDao::UpdateFriendBack(int self_uid, int friend_uid, const std::string& back) {
\tauto con = pool_->getConnection();
\ttry {
\t\tif (con == nullptr) {
\t\t\treturn false;
\t\t}

\t\tstd::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
\t\t\t"UPDATE friend SET back = ? WHERE self_id = ? AND friend_id = ?"));

\t\tpstmt->setString(1, back);
\t\tpstmt->setInt(2, self_uid);
\t\tpstmt->setInt(3, friend_uid);

\t\tint updateCount = pstmt->executeUpdate();
\t\tLOG_INFO("update friend remark rows: {}", updateCount);
\t\tpool_->returnConnection(std::move(con));
\t\treturn updateCount > 0;
\t}
\tcatch (sql::SQLException& e) {
\t\tpool_->returnConnection(std::move(con));
\t\tstd::cerr << "SQLException: " << e.what() << std::endl;
\t\treturn false;
\t}
}
'''
patch('ChatServer2/MysqlDao.cpp', [
    # 锚点：AddFriendApply 函数定义（唯一），实现插在它前面
    (b'bool MysqlDao::AddFriendApply(const int& from, const int& to,',
     UPDATE_BACK_IMPL + b'\nbool MysqlDao::AddFriendApply(const int& from, const int& to,', 1),
])

# ---------- 5. MysqlMgr.h 声明 ----------
patch('ChatServer2/MysqlMgr.h', [
    (b'\tbool AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name);',
     b'\tbool AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name);\n'
     b'\tbool UpdateFriendBack(int self_uid, int friend_uid, const std::string& back);', 1),
])

# ---------- 6. MysqlMgr.cpp 转发 ----------
patch('ChatServer2/MysqlMgr.cpp', [
    (b'bool MysqlMgr::AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name)\n{\n\treturn _dao.AddFriendApply(from, to, desc, back_name);\n}',
     b'bool MysqlMgr::AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name)\n{\n\treturn _dao.AddFriendApply(from, to, desc, back_name);\n}\n\n'
     b'bool MysqlMgr::UpdateFriendBack(int self_uid, int friend_uid, const std::string& back)\n{\n\treturn _dao.UpdateFriendBack(self_uid, friend_uid, back);\n}', 1),
])

# ---------- 7. LogicSystem.h 声明 ----------
patch('ChatServer2/LogicSystem.h', [
    (b'\tvoid AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);',
     b'\tvoid AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);\n'
     b'\tvoid UpdateFriendBack(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);', 1),
])

# ---------- 8. LogicSystem.cpp：注册 + handler ----------
HANDLER_IMPL = b'''
void LogicSystem::UpdateFriendBack(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
\tJson::Reader reader;
\tJson::Value root;
\treader.parse(msg_data, root);
\tauto uid = root["uid"].asInt();
\tauto friend_uid = root["friend_uid"].asInt();
\tauto back = root["back"].asString();
\tLOG_INFO("modify friend remark: uid={}, friend_uid={}, back={}", uid, friend_uid, back);

\tJson::Value rtvalue;
\trtvalue["error"] = ErrorCodes::Success;
\trtvalue["uid"] = uid;
\trtvalue["friend_uid"] = friend_uid;
\trtvalue["back"] = back;
\tDefer defer([this, &rtvalue, session]() {
\t\tstd::string return_str = rtvalue.toStyledString();
\t\tsession->Send(return_str, ID_UPDATE_FRIEND_BACK_RSP);
\t});

\tif (!MysqlMgr::GetInstance()->UpdateFriendBack(uid, friend_uid, back)) {
\t\trtvalue["error"] = ErrorCodes::Error_MySql;
\t\treturn;
\t}
}
'''
patch('ChatServer2/LogicSystem.cpp', [
    # 注册（AuthFriendApply 注册块后）
    (b'\t_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,\n\t\tplaceholders::_1, placeholders::_2, placeholders::_3);',
     b'\t_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,\n\t\tplaceholders::_1, placeholders::_2, placeholders::_3);\n\n'
     b'\t_fun_callbacks[ID_UPDATE_FRIEND_BACK_REQ] = std::bind(&LogicSystem::UpdateFriendBack, this,\n\t\tplaceholders::_1, placeholders::_2, placeholders::_3);', 1),
    # handler 实现（AuthFriendApply 实现之后、下一个函数之前）
    (b'\nvoid LogicSystem::DealChatTextMsg',
     b'\n' + HANDLER_IMPL + b'\nvoid LogicSystem::DealChatTextMsg', 1),
])

print('Task17 done')
