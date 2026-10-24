# -*- coding: utf-8 -*-
"""服务端：1018（加载会话列表）回包带上群成员列表，让客户端"看得见群成员"。

背景：原来只有建群(1033/1034)和加人(1035/1036)协议，**没有"查群成员"的协议**，
客户端想看成员是无米之炊。方案：不加新协议，扩展 GetUserThreads：
  a) data.h: ChatThreadInfo 加 std::vector<int> _members;
  b) MysqlDao::GetUserThreads: 查完 thread 列表后，用一条
     `SELECT thread_id,user_id FROM group_chat_member WHERE thread_id IN (...)` 批量补齐
     （避免每个群一次查询的 N+1）；
  c) LogicSystem::GetUserThreadsHandler: 回包每个 group thread 加 "members" 数组。
     私聊线程不带该字段，老客户端忽略即可 -> 向后兼容。

铁律：GBK + CRLF 字节补丁；幂等；行级吞行自检；括号深度自检=0。
"""
import sys

CRLF = '\r\n'


def patch(path, subs, must_have):
    raw = open(path, 'rb').read()
    text = raw.decode('gbk')
    if all(s in text for s in must_have):
        print('SKIP（已改过）', path)
        return
    for old, new in subs:
        if old in text:
            text = text.replace(old, new, 1)
        elif new in text:
            pass
        else:
            print('FAIL 未找到锚点 in', path, '->', old[:60].replace(CRLF, ' '))
            sys.exit(1)
    # 行级吞行自检：每个非空旧行都还在
    old_lines = [l.rstrip('\r\n') for l in raw.decode('gbk').split('\n')
                 if l.rstrip('\r\n').strip()]
    for l in old_lines:
        if l.strip() and l not in text:
            print('FAIL 行丢失:', l[:70]); sys.exit(1)
    # 括号深度自检
    def d(s, a, b):
        n = 0
        for c in s:
            if c == a: n += 1
            elif c == b: n -= 1
        return n
    raw2 = text.encode('gbk')
    for a, b in (('(', ')'), ('{', '}')):
        if d(raw.decode('gbk'), a, b) != d(text, a, b):
            print('FAIL %s%s 深度变化' % (a, b)); sys.exit(1)
    open(path, 'wb').write(raw2)
    print('OK', path)


# ---------- a) data.h: ChatThreadInfo 加 _members ----------
patch('ChatServer/data.h', [
    ('std::string _name;     // group display name (group_chat.name); empty for private' + CRLF +
     '};',
     'std::string _name;     // group display name (group_chat.name); empty for private' + CRLF +
     '    // group members (user_id list); empty for private threads.' + CRLF +
     '    // 1018 loads them together with the thread list so the client can' + CRLF +
     '    // show the member list without a separate query.' + CRLF +
     '    std::vector<int> _members;' + CRLF +
     '};'),
], ['std::vector<int> _members;'])

# ---------- b) MysqlDao::GetUserThreads: 批量补齐群成员 ----------
patch('ChatServer/MysqlDao.cpp', [
    ('\t\tthreads = std::move(tmp);' + CRLF,
     '\t\tthreads = std::move(tmp);' + CRLF
     + CRLF
     + '\t\t// Group members: one batched query instead of a query per group (N+1).' + CRLF
     + '\t\t// Collect the group thread ids first, then load all their members in one go.' + CRLF
     + '\t\tstd::vector<long long> group_ids;' + CRLF
     + '\t\tfor (auto& t : threads) {' + CRLF
     + '\t\t\tif (t->_type == "group") {' + CRLF
     + '\t\t\t\tgroup_ids.push_back(t->_thread_id);' + CRLF
     + '\t\t\t}' + CRLF
     + '\t\t}' + CRLF
     + '\t\tif (!group_ids.empty()) {' + CRLF
     + '\t\t\tstd::string m_sql =' + CRLF
     + '\t\t\t\t"SELECT thread_id, user_id FROM group_chat_member WHERE thread_id IN (";' + CRLF
     + '\t\t\tfor (size_t i = 0; i < group_ids.size(); ++i) {' + CRLF
     + '\t\t\t\tm_sql += std::to_string(group_ids[i]);' + CRLF
     + '\t\t\t\tif (i + 1 < group_ids.size()) {' + CRLF
     + '\t\t\t\t\tm_sql += ",";' + CRLF
     + '\t\t\t\t}' + CRLF
     + '\t\t\t}' + CRLF
     + '\t\t\tm_sql += ");";' + CRLF
     + '\t\t\tstd::unique_ptr<sql::Statement> m_stmt(conn->createStatement());' + CRLF
     + '\t\t\tstd::unique_ptr<sql::ResultSet> m_res(m_stmt->executeQuery(m_sql));' + CRLF
     + '\t\t\twhile (m_res->next()) {' + CRLF
     + '\t\t\t\tlong long tid = m_res->getInt64("thread_id");' + CRLF
     + '\t\t\t\tint uid_m = m_res->getInt("user_id");' + CRLF
     + '\t\t\t\tfor (auto& t : threads) {' + CRLF
     + '\t\t\t\t\tif (t->_thread_id == tid) {' + CRLF
     + '\t\t\t\t\t\tt->_members.push_back(uid_m);' + CRLF
     + '\t\t\t\t\t\tbreak;' + CRLF
     + '\t\t\t\t\t}' + CRLF
     + '\t\t\t\t}' + CRLF
     + '\t\t\t}' + CRLF
     + '\t\t}' + CRLF),
], ['group_chat_member WHERE thread_id IN'])

# ---------- c) LogicSystem: 回包加 members ----------
patch('ChatServer/LogicSystem.cpp', [
    ('\t\tthread_value["name"] = thread->_name;' + CRLF,
     '\t\tthread_value["name"] = thread->_name;' + CRLF
     + '\t\t// Group threads also carry their member list (empty array for private).' + CRLF
     + '\t\t// Older clients ignore unknown fields, so this stays compatible.' + CRLF
     + '\t\tfor (int m_uid : thread->_members) {' + CRLF
     + '\t\t\tthread_value["members"].append(m_uid);' + CRLF
     + '\t\t}' + CRLF),
], ['thread_value["members"]'])

print('三个文件全部改完')
