# -*- coding: utf-8 -*-
"""
群聊客户端适配 + 群名透传（任务 #45）。

服务端（ChatServer/ 与 ChatServer2/）：
  data.h           ChatThreadInfo 增加 _name
  MysqlDao.cpp     GetUserThreads 的 group 分支 LEFT JOIN group_chat 取群名
  LogicSystem.cpp  会话列表回包带上 "name"

客户端：
  userdata.h/.cpp  ChatThreadData 增加群标识 / 群名 / 成员，ChatThreadInfo 增加 _name
  tcpmgr.cpp       解析群名；两条收消息链路按会话类型填 ChatFormType（原来写死 PRIVATE）
  chatdialog.cpp   会话列表不再跳过群聊
  chatuserwid.cpp  群会话标题用群名（原来 GetFriendById(0) 返回空 → 列表里一个空行）
  chatpage.cpp     切到群会话不再提前 return；群里非好友发的消息不再被整条丢弃

编码：服务端与 userdata.* 是 GBK，其余客户端文件是 UTF-8。逐个文件按真实编码
读写，CRLF 原样还原。GBK 文件里的新注释用 ASCII。幂等。
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVERS = ["ChatServer", "ChatServer2"]

# ---------------------------------------------------------------- 补丁内容

# ---- 服务端：data.h ----
S_DATA_H = (
    "\tint _user2_id;    // 私聊时对应 private_chat.user2_id；群聊时设为 0\n",
    "\tint _user2_id;    // 私聊时对应 private_chat.user2_id；群聊时设为 0\n"
    "\tstd::string _name;     // group display name (group_chat.name); empty for private\n",
    1,
)

# ---- 服务端：MysqlDao.cpp GetUserThreads ----
S_DAO_PRIVATE = (
    "SELECT thread_id, 'private' AS type, user1_id, user2_id ",
    "SELECT thread_id, 'private' AS type, user1_id, user2_id, '' AS name ",
    1,
)
S_DAO_GROUP = (
    '\t\t\t"  SELECT thread_id, \'group\'   AS type, 0 AS user1_id, 0 AS user2_id "\n'
    '\t\t\t"    FROM group_chat_member "\n'
    '\t\t\t"   WHERE user_id   = ? "\n',
    '\t\t\t"  SELECT m.thread_id, \'group\' AS type, 0 AS user1_id, 0 AS user2_id, "\n'
    '\t\t\t"         IFNULL(g.name, \'\') AS name "\n'
    '\t\t\t"    FROM group_chat_member m "\n'
    '\t\t\t"    LEFT JOIN group_chat g ON g.thread_id = m.thread_id "\n'
    '\t\t\t"   WHERE m.user_id = ? "\n'
    '\t\t\t"     AND m.thread_id > ? "\n',
    1,
)
S_DAO_OUTER = (
    "SELECT thread_id, type, user1_id, user2_id ",
    "SELECT thread_id, type, user1_id, user2_id, name ",
    1,
)
S_DAO_FILL = (
    '\t\t\tcti->_user2_id = res->getInt64("user2_id");\n',
    '\t\t\tcti->_user2_id = res->getInt64("user2_id");\n'
    '\t\t\tcti->_name = res->getString("name");\n',
    1,
)

# ---- 服务端：LogicSystem.cpp ----
S_LOGIC_NAME = (
    '\t\tthread_value["user2_id"] = thread->_user2_id;\n',
    '\t\tthread_value["user2_id"] = thread->_user2_id;\n'
    '\t\t// Group threads carry a display name; private ones send an empty string.\n'
    '\t\tthread_value["name"] = thread->_name;\n',
    1,
)

# ---- 客户端：userdata.h ----
# ★ userdata.h 的 `int _user2_id;` 后面跟了行尾中文注释，不能拿 "int _user2_id;\n"
#   当锚点（匹配不到）。改成"在这一行之后插入一行"，避免手写中文。
C_UD_H_NAME = (
    "int _user2_id;",
    "    QString _name;      // group display name; empty for private",
    "line",
)
C_UD_H_METHODS = (
    "    QString GetGroupName();\n",
    "    QString GetGroupName();\n"
    "    // ---- group chat ----\n"
    "    // A group has no single peer (_other_id stays 0); it carries a display\n"
    "    // name and a member list instead.\n"
    "    void SetGroupInfo(QString name, std::vector<int> members);\n"
    "    bool IsGroup() const { return _is_group; }\n"
    "    std::vector<int> GetGroupMembers() const { return _group_members; }\n",
    1,
)
C_UD_H_MEMBER = (
    "    std::vector<int> _group_members;\n",
    "    std::vector<int> _group_members;\n"
    "    // true once SetGroupInfo() has run; distinguishes \"group\" from\n"
    "    // \"private thread with a broken peer id\" (both have _other_id == 0).\n"
    "    bool _is_group = false;\n",
    1,
)

# ---- 客户端：userdata.cpp ----
C_UD_CPP = (
    "QString ChatThreadData::GetGroupName()\n",
    "void ChatThreadData::SetGroupInfo(QString name, std::vector<int> members)\n"
    "{\n"
    "    _group_name = name;\n"
    "    _group_members = std::move(members);\n"
    "    _is_group = true;\n"
    "    _other_id = 0;\n"
    "}\n"
    "\n"
    "QString ChatThreadData::GetGroupName()\n",
    1,
)

# ---- 客户端：tcpmgr.cpp ----
C_TCP_NAME = (
    '            cti->_user2_id = value["user2_id"].toInt();\n',
    '            cti->_user2_id = value["user2_id"].toInt();\n'
    '            cti->_name = value["name"].toString();\n',
    1,
)


def _form_type_fix(msg_id_part):
    old = ("            auto chat_data = std::make_shared<TextChatData>(%s thread_id, "
           "ChatFormType::PRIVATE,\n" % msg_id_part)
    new = (
        "            // The wire payload only carries thread_id; whether this is a\n"
        "            // group is decided locally from the thread list we loaded at\n"
        "            // login.  Hardcoding PRIVATE used to mislabel every group msg.\n"
        "            auto thread_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);\n"
        "            auto form_type = (thread_data && thread_data->IsGroup())\n"
        "                                 ? ChatFormType::GROUP : ChatFormType::PRIVATE;\n"
        "            auto chat_data = std::make_shared<TextChatData>(%s thread_id, form_type,\n"
        % msg_id_part
    )
    return (old, new, 1)


C_TCP_RSP = _form_type_fix("msg_id,unique_id,")
C_TCP_NOTIFY = _form_type_fix("msg_id, unique_id,")

# ---- 客户端：chatdialog.cpp ----
C_DLG_DROP_SKIP = (
    '\t\t//先处理单聊，群聊跳过，以后添加\n'
    '\t\tif (cti->_type == "group") {\n'
    '\t\t\tcontinue;\n'
    '\t\t}\n',
    '',
    1,
)
C_DLG_GROUP_BRANCH = (
    '\tfor (auto& cti : chat_threads) {\n',
    '\tfor (auto& cti : chat_threads) {\n'
    '\t\t// ★ 群聊：没有单一 peer，other_id 记 0，标题与成员走 SetGroupInfo。\n'
    '\t\t//   以前这里直接 continue，群会话在列表里根本不会出现。\n'
    '\t\tif (cti->_type == "group") {\n'
    '\t\t\tauto group_data = std::make_shared<ChatThreadData>(0, cti->_thread_id, 0);\n'
    '\t\t\tgroup_data->SetGroupInfo(cti->_name, {});\n'
    '\t\t\tUserMgr::GetInstance()->AddChatThreadData(group_data, 0);\n'
    '\t\t\tauto* group_wid = new ChatUserWid();\n'
    '\t\t\tgroup_wid->SetChatData(group_data);\n'
    '\t\t\tQListWidgetItem* group_item = new QListWidgetItem;\n'
    '\t\t\tgroup_item->setSizeHint(group_wid->sizeHint());\n'
    '\t\t\tui->chat_user_list->addItem(group_item);\n'
    '\t\t\tui->chat_user_list->setItemWidget(group_item, group_wid);\n'
    '\t\t\t_chat_thread_items.insert(cti->_thread_id, group_item);\n'
    '\t\t\tcontinue;\n'
    '\t\t}\n',
    1,
)

# ---- 客户端：chatuserwid.cpp ----
C_WID_GROUP = (
    "    auto other_info = UserMgr::GetInstance()->GetFriendById(other_id);\n"
    "    // ★ 必须判空：GetFriendById 查不到会返回 nullptr，\n"
    "    //   之前这里直接 other_info->_icon，库里一旦有 uid=0 的坏会话就会闪退。\n"
    "    if (other_info == nullptr) {\n",
    "    auto other_info = UserMgr::GetInstance()->GetFriendById(other_id);\n"
    "    // ★ 必须判空：GetFriendById 查不到会返回 nullptr，\n"
    "    //   之前这里直接 other_info->_icon，库里一旦有 uid=0 的坏会话就会闪退。\n"
    "    if (other_info == nullptr) {\n"
    "        // ★ 群聊没有单一 peer（other_id 为 0），必然查不到好友条目。\n"
    "        //   这里原来直接 return，结果会话列表里出现一行空白。\n"
    "        //   改成用群名兜底，时间照常取最后一条消息。\n"
    "        if (_chat_data->IsGroup()) {\n"
    "            auto group_name = _chat_data->GetGroupName();\n"
    "            ui->user_name_lb->setText(group_name.isEmpty()\n"
    "                ? QString(\"群聊 %1\").arg(_chat_data->GetThreadId()) : group_name);\n"
    "            ui->user_chat_lb->setText(chat_data->GetLastMsg());\n"
    "            const auto& g_map = chat_data->GetMsgMapRef();\n"
    "            if (!g_map.isEmpty()) {\n"
    "                auto g_last = g_map.end();\n"
    "                --g_last;\n"
    "                setLastTime(QDateTime::fromString(g_last.value()->GetChatTime(),\n"
    "                                                  QString(\"yyyy-MM-dd hh:mm:ss\")));\n"
    "            } else {\n"
    "                setLastTime(QDateTime());\n"
    "            }\n"
    "            return;\n"
    "        }\n",
    1,
)

# ---- 客户端：chatpage.cpp ----
C_PAGE_SETDATA = (
    "    if(other_id == 0) {\n"
    "        //说明是群聊\n"
    "        ui->title_lb->setText(_chat_data->GetGroupName());\n"
    "        //todo...加载群聊信息和成员信息\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // 还原这个会话的草稿（没有草稿就清空输入框）\n"
    "    restoreDraft(_chat_data->GetThreadId());\n"
    "\n"
    "    //私聊\n"
    "    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);\n"
    "    if (friend_info == nullptr) {\n"
    "        return;\n"
    "    }\n"
    "    ui->title_lb->setText(friend_info->GetDisplayName());\n",
    "    // ★ 群聊：other_id 为 0，查不到好友条目，标题用群名。\n"
    "    //   以前这里直接 return，导致切到群会话时消息列表没清、草稿没还原，\n"
    "    //   上一段私聊的气泡会残留在群里。现在只换标题，其余流程与私聊一致。\n"
    "    if (other_id == 0) {\n"
    "        auto group_name = _chat_data->GetGroupName();\n"
    "        ui->title_lb->setText(group_name.isEmpty()\n"
    "                                  ? QString(\"群聊 %1\").arg(_chat_data->GetThreadId())\n"
    "                                  : group_name);\n"
    "    }\n"
    "    else {\n"
    "        //私聊\n"
    "        auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);\n"
    "        if (friend_info == nullptr) {\n"
    "            return;\n"
    "        }\n"
    "        ui->title_lb->setText(friend_info->GetDisplayName());\n"
    "    }\n"
    "\n"
    "    // 还原这个会话的草稿（没有草稿就清空输入框）\n"
    "    restoreDraft(_chat_data->GetThreadId());\n",
    1,
)

C_PAGE_APPEND = (
    "        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());\n"
    "        if (friend_info == nullptr) {\n"
    "            // ★ 对方还没进好友列表（比如消息先于 AuthFriend 回包到达）。\n"
    "            //   这里必须 return：继续走下去 pChatItem 的用户名/头像都是空的。\n"
    "            qCDebug(lcChat) << \"AppendChatMsg: friend not found, uid =\" << msg->GetSendUid();\n"
    "            return;\n"
    "        }\n"
    "        userName = friend_info->GetDisplayName();\n"
    "        userIcon = friend_info->_icon;\n",
    "        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());\n"
    "        if (friend_info == nullptr) {\n"
    "            // ★ 群里发消息的人不一定是自己的好友，查不到是常态。\n"
    "            //   以前一律 return，非好友在群里说的话整条不显示。\n"
    "            //   群聊兜底显示 \"用户 <uid>\"、头像留空；私聊维持原来的 return。\n"
    "            auto thread_data = UserMgr::GetInstance()->GetChatThreadByThreadId(msg->GetThreadId());\n"
    "            if (!(thread_data && thread_data->IsGroup())) {\n"
    "                // ★ 对方还没进好友列表（比如消息先于 AuthFriend 回包到达）。\n"
    "                //   这里必须 return：继续走下去 pChatItem 的用户名/头像都是空的。\n"
    "                qCDebug(lcChat) << \"AppendChatMsg: friend not found, uid =\" << msg->GetSendUid();\n"
    "                return;\n"
    "            }\n"
    "            userName = QString(\"用户 %1\").arg(msg->GetSendUid());\n"
    "            userIcon.clear();\n"
    "        }\n"
    "        else {\n"
    "            userName = friend_info->GetDisplayName();\n"
    "            userIcon = friend_info->_icon;\n"
    "        }\n",
    1,
)

# ---------------------------------------------------------------- 目标表

TARGETS = []
for srv in SERVERS:
    TARGETS.append(("%s/data.h" % srv, "gbk", [("name-field", S_DATA_H)]))
    TARGETS.append(("%s/MysqlDao.cpp" % srv, "gbk", [
        ("sql-private", S_DAO_PRIVATE),
        ("sql-group", S_DAO_GROUP),
        ("sql-outer", S_DAO_OUTER),
        ("fill-name", S_DAO_FILL),
    ]))
    TARGETS.append(("%s/LogicSystem.cpp" % srv, "gbk", [("rsp-name", S_LOGIC_NAME)]))
TARGETS.append(("Client/userdata.h", "gbk", [
    ("cti-name", C_UD_H_NAME),
    ("methods", C_UD_H_METHODS),
    ("member", C_UD_H_MEMBER),
]))
TARGETS.append(("Client/userdata.cpp", "gbk", [("set-group-info", C_UD_CPP)]))
TARGETS.append(("Client/tcpmgr.cpp", "utf-8", [
    ("parse-name", C_TCP_NAME),
    ("formtype-rsp", C_TCP_RSP),
    ("formtype-notify", C_TCP_NOTIFY),
]))
TARGETS.append(("Client/chatdialog.cpp", "utf-8", [
    ("drop-skip", C_DLG_DROP_SKIP),
    ("group-branch", C_DLG_GROUP_BRANCH),
]))
TARGETS.append(("Client/chatuserwid.cpp", "utf-8", [("group-title", C_WID_GROUP)]))
TARGETS.append(("Client/chatpage.cpp", "utf-8", [
    ("setdata", C_PAGE_SETDATA),
    ("append", C_PAGE_APPEND),
]))

# ---------------------------------------------------------------- 工具


def read_text(path, enc):
    raw = open(path, "rb").read()
    text = raw.decode(enc)
    crlf = "\r\n" in text
    return text.replace("\r\n", "\n"), crlf


def insert_after_line(text, marker_prefix, new_line, rel="<unknown>"):
    """在 strip() 后以 marker_prefix 开头的那一行之后插入 new_line（必须恰好命中一行）。"""
    out = []
    hit = 0
    for line in text.split("\n"):
        out.append(line)
        if line.strip().startswith(marker_prefix):
            out.append(new_line)
            hit += 1
    if hit != 1:
        raise SystemExit("%s [line-mode] marker %r hit=%d (expect 1)"
                         % (rel, marker_prefix, hit))
    return "\n".join(out)


def write_text(path, enc, text, crlf):
    out = text.replace("\n", "\r\n") if crlf else text
    open(path, "wb").write(out.encode(enc))


def main():
    before = {}
    for rel, enc, _ in TARGETS:
        p = os.path.join(ROOT, rel)
        before[p] = read_text(p, enc)[0]

    for rel, enc, entries in TARGETS:
        p = os.path.join(ROOT, rel)
        text, crlf = read_text(p, enc)
        orig = text
        touched = []
        for entry in entries:
            # 单条补丁写成 ("tag", (old, new, expect))，这里展开
            if len(entry) == 2:
                tag, (old, new, expect) = entry
            else:
                tag, old, new, expect = entry
            if new and new in text and expect != "line":
                continue                      # 已打过，幂等跳过
            if expect == "line":
                if new.strip() in text:
                    continue
                text = insert_after_line(text, old, new, rel)
                touched.append(tag)
                continue
            cnt = text.count(old)
            if cnt == 0 and new == "":
                continue                      # 纯删除块：old 已不存在即已达成，幂等跳过
            if cnt != expect:
                raise SystemExit("%s [%s] anchor count=%d (expect %d)\n---\n%s"
                                 % (rel, tag, cnt, expect, old[:200]))
            text = text.replace(old, new, 1)
            touched.append(tag)
        if text == orig:
            print("[SKIP] %-28s" % rel)
            continue
        write_text(p, enc, text, crlf)
        print("[OK  ] %-28s %s" % (rel, ",".join(touched)))

    # 校验：括号深度 + 编码可回读 + 新增片段到位
    print("\n--- 校验 ---")
    bad = 0
    for rel, enc, entries in TARGETS:
        p = os.path.join(ROOT, rel)
        open(p, "rb").read().decode(enc)      # 编码自检
        t1 = read_text(p, enc)[0]
        d0 = before[p].count("{") - before[p].count("}")
        d1 = t1.count("{") - t1.count("}")
        mark = "ok " if d0 == d1 else "!! "
        if d0 != d1:
            bad += 1
        print("  %s %-28s 括号配平 %d -> %d" % (mark, rel, d0, d1))

    NEED = {
        "ChatServer/data.h": ["std::string _name;"],
        "ChatServer2/data.h": ["std::string _name;"],
        "ChatServer/MysqlDao.cpp": ["LEFT JOIN group_chat g", "cti->_name = res->getString(\"name\")"],
        "ChatServer2/MysqlDao.cpp": ["LEFT JOIN group_chat g", "cti->_name = res->getString(\"name\")"],
        "ChatServer/LogicSystem.cpp": ['thread_value["name"] = thread->_name;'],
        "ChatServer2/LogicSystem.cpp": ['thread_value["name"] = thread->_name;'],
        "Client/userdata.h": ["void SetGroupInfo(", "bool IsGroup() const", "bool _is_group = false;"],
        "Client/userdata.cpp": ["void ChatThreadData::SetGroupInfo("],
        "Client/tcpmgr.cpp": ['cti->_name = value["name"].toString();',
                              "ChatFormType::GROUP : ChatFormType::PRIVATE"],
        "Client/chatdialog.cpp": ['group_data->SetGroupInfo(cti->_name, {})'],
        "Client/chatuserwid.cpp": ["_chat_data->IsGroup()"],
        "Client/chatpage.cpp": ["_chat_data->IsGroup()" if False else "QString(\"群聊 %1\")",
                                "thread_data->IsGroup()"],
    }
    print("\n--- 新片段到位 ---")
    for rel, enc, _ in TARGETS:
        p = os.path.join(ROOT, rel)
        t = read_text(p, enc)[0]
        miss = [s for s in NEED.get(rel, []) if s not in t]
        if miss:
            bad += 1
            print("  !! %-28s missing=%s" % (rel, miss))
        else:
            print("  ok  %-28s" % rel)

    if bad:
        raise SystemExit("校验失败：%d 项" % bad)
    print("\n全部通过")


if __name__ == "__main__":
    main()
