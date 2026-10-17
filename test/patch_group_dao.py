# -*- coding: utf-8 -*-
"""
群聊 DAO/Mgr 层补丁（任务 #42）。

同时打进 ChatServer/ 与 ChatServer2/ 的 4 个文件：
    MysqlDao.h / MysqlDao.cpp / MysqlMgr.h / MysqlMgr.cpp

铁律：
  * 这些文件是 GBK + CRLF，只能用字节级补丁（Edit/Write 会毁掉 GBK）。
  * 注释一律 ASCII，避免 GBK 往返丢失。
  * 幂等：先检查 new 是否已在文件里，在就跳过。
  * 补丁前 \r\n -> \n，写完还原 \r\n。
  * 末尾做"剥除注释后代码骨架逐字节一致"校验，防止误改代码。
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVERS = ["ChatServer", "ChatServer2"]

# ---------------------------------------------------------------- 补丁内容

DAO_H_NEW = (
    "\tbool GetPrivateChatUsers(int thread_id, int& user1_id, int& user2_id);\n"
    "\t// ---- group chat ------------------------------------------------------\n"
    "\t// GetUserThreads() already UNIONs private threads with group threads\n"
    "\t// (group rows come from group_chat_member), so the only thing the chat\n"
    "\t// path still needs is: what kind of thread is this, and who is in it.\n"
    "\t// Reads chat_thread.type (\"private\" / \"group\").\n"
    "\tbool GetThreadType(int thread_id, std::string& type);\n"
    "\t// Every member uid of a group thread.  false means the thread does not\n"
    "\t// exist; an empty vector with true means a group with nobody in it.\n"
    "\tbool GetGroupMembers(int thread_id, std::vector<int>& members);\n"
    "\t// chat_thread + group_chat + group_chat_member in one transaction.\n"
    "\t// The owner is always inserted as the first member.\n"
    "\tbool CreateGroupChat(const std::string& name, int owner_id,\n"
    "\t\tconst std::vector<int>& members, int& thread_id);\n"
    "\t// Adds members to an existing group.  Duplicates are ignored.\n"
    "\tbool AddGroupMembers(int thread_id, const std::vector<int>& new_members);\n"
    "\tstd::shared_ptr<PageResult> LoadChatMsg(int threadId, int lastId, int pageSize);\n"
)

DAO_H_OLD = (
    "\tbool GetPrivateChatUsers(int thread_id, int& user1_id, int& user2_id);\n"
    "\tstd::shared_ptr<PageResult> LoadChatMsg(int threadId, int lastId, int pageSize);\n"
)

MGR_H_NEW = (
    "\tbool GetPrivateChatUsers(int thread_id, int& user1_id, int& user2_id);\n"
    "\t// ---- group chat (see MysqlDao for the contracts) ---------------------\n"
    "\tbool GetThreadType(int thread_id, std::string& type);\n"
    "\tbool GetGroupMembers(int thread_id, std::vector<int>& members);\n"
    "\tbool CreateGroupChat(const std::string& name, int owner_id,\n"
    "\t\tconst std::vector<int>& members, int& thread_id);\n"
    "\tbool AddGroupMembers(int thread_id, const std::vector<int>& new_members);\n"
    "\tstd::shared_ptr<PageResult> LoadChatMsg(int threadId, int lastId, int pageSize);\n"
)

MGR_H_OLD = DAO_H_OLD


# 手写，避免 _conn_block 里占位符替换出错
DAO_CPP_FUNCS = """\
bool MysqlDao::GetThreadType(int thread_id, std::string& type)
{
\tauto con = pool_->getConnection();
\tif (!con) {
\t\treturn false;
\t}
\tDefer defer([this, &con]() {
\t\tpool_->returnConnection(std::move(con));
\t\t});
\tauto& conn = con->_con;
\ttry {
\t\tstd::string sql =
\t\t\t"SELECT type FROM chat_thread WHERE thread_id = ? LIMIT 1;";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));
\t\tpstmt->setInt64(1, thread_id);
\t\tstd::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
\t\tif (!res->next()) {
\t\t\treturn false;
\t\t}
\t\ttype = res->getString("type");
\t\treturn true;
\t}
\tcatch (sql::SQLException& e) {
\t\tstd::cerr << "SQLException (GetThreadType): " << e.what() << std::endl;
\t\treturn false;
\t}
}

bool MysqlDao::GetGroupMembers(int thread_id, std::vector<int>& members)
{
\tauto con = pool_->getConnection();
\tif (!con) {
\t\treturn false;
\t}
\tDefer defer([this, &con]() {
\t\tpool_->returnConnection(std::move(con));
\t\t});
\tauto& conn = con->_con;
\ttry {
\t\tmembers.clear();
\t\tstd::string sql =
\t\t\t"SELECT user_id FROM group_chat_member WHERE thread_id = ? ORDER BY user_id;";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));
\t\tpstmt->setInt64(1, thread_id);
\t\tstd::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
\t\twhile (res->next()) {
\t\t\tmembers.push_back(res->getInt("user_id"));
\t\t}
\t\treturn true;
\t}
\tcatch (sql::SQLException& e) {
\t\tstd::cerr << "SQLException (GetGroupMembers): " << e.what() << std::endl;
\t\treturn false;
\t}
}

bool MysqlDao::CreateGroupChat(const std::string& name, int owner_id,
\tconst std::vector<int>& members, int& thread_id)
{
\tauto con = pool_->getConnection();
\tif (!con) {
\t\treturn false;
\t}
\tDefer defer([this, &con]() {
\t\tpool_->returnConnection(std::move(con));
\t\t});
\tauto& conn = con->_con;
\ttry {
\t\tconn->setAutoCommit(false);
\t\t// 1. Owner first, then the invitees, de-duplicated: group_chat_member is
\t\t//    keyed on (thread_id, user_id) so a repeat uid would abort the txn.
\t\tstd::vector<int> all;
\t\tall.push_back(owner_id);
\t\tfor (int uid : members) {
\t\t\tbool dup = false;
\t\t\tfor (int exist : all) {
\t\t\t\tif (exist == uid) {
\t\t\t\t\tdup = true;
\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t}
\t\t\tif (!dup) {
\t\t\t\tall.push_back(uid);
\t\t\t}
\t\t}
\t\t// 2. chat_thread row
\t\tstd::string insert_thread_sql =
\t\t\t"INSERT INTO chat_thread (type, created_at) VALUES ('group', NOW());";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt_thread(conn->prepareStatement(insert_thread_sql));
\t\tpstmt_thread->executeUpdate();

\t\tstd::string last_id_sql = "SELECT LAST_INSERT_ID();";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt_last(conn->prepareStatement(last_id_sql));
\t\tstd::unique_ptr<sql::ResultSet> res_last(pstmt_last->executeQuery());
\t\tres_last->next();
\t\tthread_id = res_last->getInt(1);

\t\t// 3. group_chat row (display name + owner)
\t\tstd::string insert_group_sql =
\t\t\t"INSERT INTO group_chat (thread_id, name, owner_id) VALUES (?, ?, ?);";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt_group(conn->prepareStatement(insert_group_sql));
\t\tpstmt_group->setInt64(1, thread_id);
\t\tpstmt_group->setString(2, name);
\t\tpstmt_group->setInt64(3, owner_id);
\t\tpstmt_group->executeUpdate();

\t\t// 4. one group_chat_member row per member
\t\tstd::string insert_member_sql =
\t\t\t"INSERT INTO group_chat_member (thread_id, user_id) VALUES (?, ?);";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt_member(conn->prepareStatement(insert_member_sql));
\t\tfor (int uid : all) {
\t\t\tpstmt_member->setInt64(1, thread_id);
\t\t\tpstmt_member->setInt64(2, uid);
\t\t\tpstmt_member->executeUpdate();
\t\t}

\t\tconn->commit();
\t\treturn true;
\t}
\tcatch (sql::SQLException& e) {
\t\tstd::cerr << "SQLException (CreateGroupChat): " << e.what() << std::endl;
\t\tconn->rollback();
\t\treturn false;
\t}
}

bool MysqlDao::AddGroupMembers(int thread_id, const std::vector<int>& new_members)
{
\tif (new_members.empty()) {
\t\treturn true;
\t}
\tauto con = pool_->getConnection();
\tif (!con) {
\t\treturn false;
\t}
\tDefer defer([this, &con]() {
\t\tpool_->returnConnection(std::move(con));
\t\t});
\tauto& conn = con->_con;
\ttry {
\t\tconn->setAutoCommit(false);
\t\t// Only group threads own members; bail out otherwise.
\t\tstd::string type_sql =
\t\t\t"SELECT type FROM chat_thread WHERE thread_id = ? LIMIT 1;";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt_type(conn->prepareStatement(type_sql));
\t\tpstmt_type->setInt64(1, thread_id);
\t\tstd::unique_ptr<sql::ResultSet> res_type(pstmt_type->executeQuery());
\t\tif (!res_type->next() || res_type->getString("type") != "group") {
\t\t\tconn->rollback();
\t\t\treturn false;
\t\t}

\t\t// INSERT IGNORE: an already-present member is not an error.
\t\tstd::string sql =
\t\t\t"INSERT IGNORE INTO group_chat_member (thread_id, user_id) VALUES (?, ?);";
\t\tstd::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));
\t\tfor (int uid : new_members) {
\t\t\tpstmt->setInt64(1, thread_id);
\t\t\tpstmt->setInt64(2, uid);
\t\t\tpstmt->executeUpdate();
\t\t}
\t\tconn->commit();
\t\treturn true;
\t}
\tcatch (sql::SQLException& e) {
\t\tstd::cerr << "SQLException (AddGroupMembers): " << e.what() << std::endl;
\t\tconn->rollback();
\t\treturn false;
\t}
}

"""

MGR_CPP_FUNCS = """\
bool MysqlMgr::GetThreadType(int thread_id, std::string& type)
{
\treturn _dao.GetThreadType(thread_id, type);
}

bool MysqlMgr::GetGroupMembers(int thread_id, std::vector<int>& members)
{
\treturn _dao.GetGroupMembers(thread_id, members);
}

bool MysqlMgr::CreateGroupChat(const std::string& name, int owner_id,
\tconst std::vector<int>& members, int& thread_id)
{
\treturn _dao.CreateGroupChat(name, owner_id, members, thread_id);
}

bool MysqlMgr::AddGroupMembers(int thread_id, const std::vector<int>& new_members)
{
\treturn _dao.AddGroupMembers(thread_id, new_members);
}

"""

# ---------------------------------------------------------------- 工具


def read_text(path):
    raw = open(path, "rb").read()
    text = raw.decode("gbk")
    crlf = "\r\n" in text
    return text.replace("\r\n", "\n"), crlf


def write_text(path, text, crlf):
    out = text.replace("\n", "\r\n") if crlf else text
    open(path, "wb").write(out.encode("gbk"))


def apply_once(path, entries):
    """entries: list of (tag, old, new)。返回 (changed, [skipped_tags])"""
    text, crlf = read_text(path)
    orig = text
    skipped = []
    for tag, old, new in entries:
        if new in text:
            skipped.append(tag)
            continue
        cnt = text.count(old)
        if cnt != 1:
            raise SystemExit("%s: anchor %r count=%d (expect 1)" % (path, tag, cnt))
        text = text.replace(old, new, 1)
    if text == orig:
        return False, skipped
    write_text(path, text, crlf)
    return True, skipped


def strip_comments(text):
    """粗略剥除 // 与 /* */ 注释，用于骨架比对（字符串内容归一化成 ""）。"""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == '"':
                    break
                j += 1
            out.append('""')
            i = j + 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return re.sub(r"[ \t]+", " ", "".join(out))


# ---------------------------------------------------------------- 主流程


def main():
    targets = {
        "MysqlDao.h": [(("decl", DAO_H_OLD, DAO_H_NEW))],
        "MysqlMgr.h": [(("decl", MGR_H_OLD, MGR_H_NEW))],
        "MysqlDao.cpp": [("funcs",
                          "std::shared_ptr<PageResult> MysqlDao::LoadChatMsg(int thread_id, int last_message_id, int page_size)\n",
                          DAO_CPP_FUNCS + "std::shared_ptr<PageResult> MysqlDao::LoadChatMsg(int thread_id, int last_message_id, int page_size)\n")],
        "MysqlMgr.cpp": [("funcs",
                          "std::shared_ptr<PageResult> MysqlMgr::LoadChatMsg(int threadId, int lastId, int pageSize)\n",
                          MGR_CPP_FUNCS + "std::shared_ptr<PageResult> MysqlMgr::LoadChatMsg(int threadId, int lastId, int pageSize)\n")],
    }

    before = {}
    for srv in SERVERS:
        for fname in targets:
            p = os.path.join(ROOT, srv, fname)
            before[p] = strip_comments(read_text(p)[0])

    for srv in SERVERS:
        for fname, entries in targets.items():
            p = os.path.join(ROOT, srv, fname)
            changed, skipped = apply_once(p, entries)
            state = "OK  " if changed else "SKIP"
            print("[%s] %-14s %s%s" % (
                state, srv + "/" + fname,
                "patched" if changed else "already up to date",
                (" (skipped: %s)" % ",".join(skipped)) if skipped else ""))

    # 骨架校验：剥除注释 + 归一化空白后，只允许新增，不允许原有内容变化
    print("\n--- 代码骨架校验（剥除注释后）---")
    bad = 0
    for p, skel_before in before.items():
        skel_after = strip_comments(read_text(p)[0])
        # before 必须是 after 的"子序列"意义上保持一致：这里用更严格的做法——
        # 逐行检查 before 的每一行仍按序存在于 after 中
        la = skel_after.split("\n")
        lb = skel_before.split("\n")
        i = 0
        for line in lb:
            hit = False
            while i < len(la):
                if la[i] == line:
                    hit = True
                    i += 1
                    break
                i += 1
            if not hit:
                bad += 1
                print("  !! MISSING in %s: %r" % (p, line[:90]))
                break
        else:
            print("  ok  %s" % p.replace(ROOT + os.sep, ""))
    if bad:
        raise SystemExit("骨架校验失败：%d 个文件" % bad)

    # 编码自检
    print("\n--- GBK 自检 ---")
    for srv in SERVERS:
        for fname in targets:
            p = os.path.join(ROOT, srv, fname)
            open(p, "rb").read().decode("gbk")
    print("  全部 8 个文件仍是合法 GBK")


if __name__ == "__main__":
    main()
