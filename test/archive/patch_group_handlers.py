# -*- coding: utf-8 -*-
"""
群聊建群 / 加人接口（任务 #44）。

同时打进 ChatServer/ 与 ChatServer2/：
  const.h        新增 MSG_IDS 1033~1036 与两个错误码
  LogicSystem.h  声明 CreateGroupChat / AddGroupMembers 两个 handler
  LogicSystem.cpp 注册 + 实现（追加到文件末尾）
并同步 Client/global.h 的 ReqId 枚举（保持两端编号一致）。

安全口径：只有本群成员才能拉人进群，建群者自动成为 owner + 第一个成员，
成员数量上限 200（否则 DealChatTextMsg 的扇出循环会被放大）。

铁律：服务端文件 GBK + CRLF，只能字节级补丁；注释 ASCII；幂等。
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVERS = ["ChatServer", "ChatServer2"]

# ---------------------------------------------------------------- 补丁片段

NEW_ERR = (
    "\tError_MySql = 1014, //database error //加载聊天失败\n"
    "\tCREATE_GROUP_FAILED = 1015,      //创建群聊失败\n"
    "\tADD_GROUP_MEMBER_FAILED = 1016,  //添加群成员失败\n"
)
OLD_ERR = "\tError_MySql = 1014, //database error //加载聊天失败\n"

NEW_IDS = (
    "\tID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response\n"
    "\t// ---- group chat ----\n"
    "\tID_CREATE_GROUP_CHAT_REQ = 1033,  //创建群聊请求\n"
    "\tID_CREATE_GROUP_CHAT_RSP = 1034,  //创建群聊回复\n"
    "\tID_ADD_GROUP_MEMBER_REQ = 1035,   //添加群成员请求\n"
    "\tID_ADD_GROUP_MEMBER_RSP = 1036,   //添加群成员回复\n"
)
OLD_IDS = "\tID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response\n"

NEW_DECL = (
    "\tvoid LoadChatMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);\n"
    "\t// ---- group chat ----\n"
    "\tvoid CreateGroupChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);\n"
    "\tvoid AddGroupMembers(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);\n"
)
OLD_DECL = "\tvoid LoadChatMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);\n"

NEW_REG = (
    "\t_fun_callbacks[ID_LOAD_CHAT_MSG_REQ] = std::bind(&LogicSystem::LoadChatMsg, this,\n"
    "\t\tplaceholders::_1, placeholders::_2, placeholders::_3);\n"
    "\n"
    "\t_fun_callbacks[ID_CREATE_GROUP_CHAT_REQ] = std::bind(&LogicSystem::CreateGroupChat, this,\n"
    "\t\tplaceholders::_1, placeholders::_2, placeholders::_3);\n"
    "\n"
    "\t_fun_callbacks[ID_ADD_GROUP_MEMBER_REQ] = std::bind(&LogicSystem::AddGroupMembers, this,\n"
    "\t\tplaceholders::_1, placeholders::_2, placeholders::_3);\n"
    "}"
)
OLD_REG = (
    "\t_fun_callbacks[ID_LOAD_CHAT_MSG_REQ] = std::bind(&LogicSystem::LoadChatMsg, this,\n"
    "\t\tplaceholders::_1, placeholders::_2, placeholders::_3);\n"
    "}"
)

HANDLERS = """\

// ---------------------------------------------------------------------------
// 群聊：建群 / 加人。
// 这一轮群聊的创建入口是 SQL，这两个接口是给客户端/测试脚本用的正规通道：
// 建群 = chat_thread + group_chat + group_chat_member 三条写入（一个事务），
// 加人 = 只能由现有成员发起，避免靠猜 thread_id 把自己塞进别人的群。
// ---------------------------------------------------------------------------
void LogicSystem::CreateGroupChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
\tJson::Reader reader;
\tJson::Value root;
\treader.parse(msg_data, root);
\tauto uid = root["uid"].asInt();
\tauto name = root["name"].asString();

\tJson::Value  rtvalue;
\trtvalue["error"] = ErrorCodes::Success;
\trtvalue["uid"] = uid;
\trtvalue["name"] = name;

\t// The creator becomes the owner AND the first member row, so it has to be
\t// a real uid -- otherwise group_chat_member ends up holding uid 0 and every
\t// fan-out in DealChatTextMsg would try to deliver to a ghost.
\tif (uid <= 0) {
\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\treturn;
\t}

\tDefer defer([this, &rtvalue, session]() {
\t\tstd::string return_str = rtvalue.toStyledString();
\t\tsession->Send(return_str, ID_CREATE_GROUP_CHAT_RSP);
\t\t});

\tstd::vector<int> members;
\tfor (const auto& m : root["members"]) {
\t\tint muid = m.asInt();
\t\tif (muid <= 0 || muid == uid) {
\t\t\tcontinue;
\t\t}
\t\tmembers.push_back(muid);
\t}

\t// Hard cap: DealChatTextMsg loops over every member per message, and the DAO
\t// inserts one row per member.  200 keeps both bounded.
\tconst size_t kMaxGroupMembers = 200;
\tif (members.size() + 1 > kMaxGroupMembers) {
\t\tLOG_ERROR("CreateGroupChat: uid {} tried to create a group of {} members",
\t\t\tuid, members.size() + 1);
\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\treturn;
\t}

\tint thread_id = 0;
\tif (!MysqlMgr::GetInstance()->CreateGroupChat(name, uid, members, thread_id)) {
\t\trtvalue["error"] = ErrorCodes::CREATE_GROUP_FAILED;
\t\treturn;
\t}

\trtvalue["thread_id"] = thread_id;
\tLOG_INFO("创建群聊: thread={}, owner={}, 成员数={}", thread_id, uid, members.size() + 1);
}

void LogicSystem::AddGroupMembers(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
\tJson::Reader reader;
\tJson::Value root;
\treader.parse(msg_data, root);
\tauto uid = root["uid"].asInt();
\tauto thread_id = root["thread_id"].asInt();

\tJson::Value  rtvalue;
\trtvalue["error"] = ErrorCodes::Success;
\trtvalue["uid"] = uid;
\trtvalue["thread_id"] = thread_id;

\tif (uid <= 0 || thread_id <= 0) {
\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\treturn;
\t}

\tDefer defer([this, &rtvalue, session]() {
\t\tstd::string return_str = rtvalue.toStyledString();
\t\tsession->Send(return_str, ID_ADD_GROUP_MEMBER_RSP);
\t\t});

\t// Only somebody already in the group may invite.  Without this check a
\t// client could push itself (or a victim) into any group by guessing ids.
\tstd::vector<int> members;
\tif (!MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members)) {
\t\trtvalue["error"] = ErrorCodes::Error_MySql;
\t\treturn;
\t}
\tbool in_group = false;
\tfor (int m : members) {
\t\tif (m == uid) {
\t\t\tin_group = true;
\t\t\tbreak;
\t\t}
\t}
\tif (!in_group) {
\t\tLOG_ERROR("AddGroupMembers: uid {} is not in thread {}", uid, thread_id);
\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\treturn;
\t}

\tstd::vector<int> new_members;
\tfor (const auto& m : root["members"]) {
\t\tint muid = m.asInt();
\t\tif (muid <= 0) {
\t\t\tcontinue;
\t\t}
\t\tbool dup = false;
\t\tfor (int exist : members) {
\t\t\tif (exist == muid) {
\t\t\t\tdup = true;
\t\t\t\tbreak;
\t\t\t}
\t\t}
\t\tif (!dup) {
\t\t\tnew_members.push_back(muid);
\t\t}
\t}

\t// Nothing new to add: still Success, just a no-op.
\tif (new_members.empty()) {
\t\treturn;
\t}

\tif (!MysqlMgr::GetInstance()->AddGroupMembers(thread_id, new_members)) {
\t\trtvalue["error"] = ErrorCodes::ADD_GROUP_MEMBER_FAILED;
\t\treturn;
\t}

\trtvalue["added"] = (int)new_members.size();
\tLOG_INFO("群聊加人: thread={}, 新增={}", thread_id, new_members.size());
}
"""

# ---------------------------------------------------------------- 客户端枚举

NEW_CLIENT_IDS = (
    "    ID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response      //加载聊天消息\n"
    "    // ---- 群聊（与 ChatServer/const.h 的 MSG_IDS 对齐）----\n"
    "    ID_CREATE_GROUP_CHAT_REQ = 1033,  //创建群聊请求\n"
    "    ID_CREATE_GROUP_CHAT_RSP = 1034,  //创建群聊回复\n"
    "    ID_ADD_GROUP_MEMBER_REQ = 1035,   //添加群成员请求\n"
    "    ID_ADD_GROUP_MEMBER_RSP = 1036,   //添加群成员回复\n"
)
OLD_CLIENT_IDS = "    ID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response      //加载聊天消息\n"

# ---------------------------------------------------------------- 工具


def read_gbk(path):
    raw = open(path, "rb").read()
    text = raw.decode("gbk")
    crlf = "\r\n" in text
    return text.replace("\r\n", "\n"), crlf


def write_gbk(path, text, crlf):
    out = text.replace("\n", "\r\n") if crlf else text
    open(path, "wb").write(out.encode("gbk"))


def sub_once(path, old, new, tag, text=None, crlf=None, write=False):
    cnt = text.count(old)
    if cnt != 1:
        raise SystemExit("%s: %s anchor count=%d (expect 1)" % (path, tag, cnt))
    return text.replace(old, new, 1)


def patch_server(path, fname):
    text, crlf = read_gbk(path)
    orig = text
    if fname == "const.h":
        if NEW_ERR not in text:
            text = sub_once(path, OLD_ERR, NEW_ERR, "err", text)
        if NEW_IDS not in text:
            text = sub_once(path, OLD_IDS, NEW_IDS, "ids", text)
    elif fname == "LogicSystem.h":
        if NEW_DECL not in text:
            text = sub_once(path, OLD_DECL, NEW_DECL, "decl", text)
    elif fname == "LogicSystem.cpp":
        if NEW_REG not in text:
            text = sub_once(path, OLD_REG, NEW_REG, "reg", text)
        if "void LogicSystem::CreateGroupChat(" not in text:
            text = text.rstrip("\n") + "\n" + HANDLERS
    if text == orig:
        return False
    write_gbk(path, text, crlf)
    return True


def patch_client(path):
    raw = open(path, "rb").read()
    bom = raw[:3] == b"\xef\xbb\xbf"
    text = raw.decode("utf-8-sig")
    crlf = "\r\n" in text
    t = text.replace("\r\n", "\n")
    if NEW_CLIENT_IDS in t:
        return False
    t = sub_once(path, OLD_CLIENT_IDS, NEW_CLIENT_IDS, "client-ids", t)
    out = t.replace("\n", "\r\n") if crlf else t
    data = out.encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    open(path, "wb").write(data)
    return True


def brace_balance(text):
    return text.count("{") - text.count("}")


def main():
    files = ["const.h", "LogicSystem.h", "LogicSystem.cpp"]
    for srv in SERVERS:
        for fname in files:
            p = os.path.join(ROOT, srv, fname)
            changed = patch_server(p, fname)
            print("[%s] %s/%s" % ("OK  " if changed else "SKIP", srv, fname))
    p = os.path.join(ROOT, "Client", "global.h")
    changed = patch_client(p)
    print("[%s] Client/global.h" % ("OK  " if changed else "SKIP"))

    print("\n--- 校验 ---")
    ok = True
    for srv in SERVERS:
        for fname in files:
            p = os.path.join(ROOT, srv, fname)
            t = read_gbk(p)[0]
            open(p, "rb").read().decode("gbk")
            need = {
                "const.h": ["ID_CREATE_GROUP_CHAT_REQ = 1033",
                            "ID_ADD_GROUP_MEMBER_RSP = 1036",
                            "CREATE_GROUP_FAILED = 1015",
                            "ADD_GROUP_MEMBER_FAILED = 1016"],
                "LogicSystem.h": ["void CreateGroupChat(std::shared_ptr<CSession>",
                                  "void AddGroupMembers(std::shared_ptr<CSession>"],
                "LogicSystem.cpp": ["ID_CREATE_GROUP_CHAT_REQ] = std::bind",
                                    "ID_ADD_GROUP_MEMBER_REQ] = std::bind",
                                    "void LogicSystem::CreateGroupChat(",
                                    "void LogicSystem::AddGroupMembers("],
            }[fname]
            miss = [s for s in need if s not in t]
            rel = "%s/%s" % (srv, fname)
            if miss:
                ok = False
                print("  !! %s missing=%s" % (rel, miss))
            else:
                print("  ok  %s (括号配平 %d)" % (rel, brace_balance(t)))

    # 两端必须逐字节一致，否则会出现"换台服务器行为不同"的诡异 bug
    for fname in files:
        a = open(os.path.join(ROOT, "ChatServer", fname), "rb").read()
        b = open(os.path.join(ROOT, "ChatServer2", fname), "rb").read()
        if a != b:
            ok = False
            print("  !! ChatServer 与 ChatServer2 的 %s 不一致" % fname)
    print("  两端一致性: %s" % ("OK" if ok else "FAIL"))

    t = open(os.path.join(ROOT, "Client", "global.h"), "rb").read().decode("utf-8-sig")
    miss = [s for s in ("ID_CREATE_GROUP_CHAT_REQ = 1033",
                        "ID_ADD_GROUP_MEMBER_RSP = 1036") if s not in t]
    print("  Client/global.h: %s" % ("OK" if not miss else "missing=%s" % miss))
    if miss:
        ok = False

    if not ok:
        raise SystemExit("校验失败")


if __name__ == "__main__":
    main()
