# -*- coding: utf-8 -*-
"""
群聊收发打通：服务端（任务 #43）。

同时打进 ChatServer/ 与 ChatServer2/：
  1. LogicSystem.cpp  DealChatTextMsg
     - 线程类型分支：private 走原来的「两个成员」校验，group 走「成员列表」校验
     - 群消息 chat_message.recv_id = 0（用户选定），不再伪造单一接收者
     - 投递从「单人」改成「按 recipients 扇出」，排除发送者自己
     - 跨服 gRPC 补上 msg_type / total_size / md5
       （原来漏了，图片跨服会被接收端当成纯文本路径显示）
  2. ChatServiceImpl.cpp  NotifyTextChatMsg
     - 回包补上 msg_type / total_size / md5，与上面配套

铁律：这些文件是 GBK + CRLF，只能字节级补丁；注释 ASCII；幂等。

协议上【不改 proto】：TextChatData 本来就有 msg_type/total_size/md5 三个字段，
只是没人填。群聊的 group/private 区分由客户端按 thread_id 查本地会话列表解决。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVERS = ["ChatServer", "ChatServer2"]

# ---------------------------------------------------------------- 新代码

NEW_VALIDATION = """\
\t// ==== Never trust thread_id / touid coming from the client ====
\t// A forged or stale thread_id would write this message into somebody else's
\t// conversation -- exactly how cross-user rows once got into chat_message
\t// ("three people talking in one chat"). So: look the thread up, require the
\t// sender to be a member, and derive the receiver(s) from the thread instead
\t// of believing the client.
\t// NOTE: Defer is declared here (before any early return) so every exit path
\t// still answers the client.
\tDefer defer([this, &rtvalue, session]() {
\t\tstd::string return_str = rtvalue.toStyledString();
\t\tsession->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
\t\t});

\t// Threads come in two flavours and the membership rule differs:
\t//   private -> exactly two members, the receiver is the other one
\t//   group   -> N members, every member except the sender gets a copy
\tstd::string thread_type;
\tif (!MysqlMgr::GetInstance()->GetThreadType(thread_id, thread_type)) {
\t\tLOG_ERROR("DealChatTextMsg: thread {} not found (uid={}, touid={})", thread_id, uid, touid);
\t\trtvalue["error"] = ErrorCodes::Error_MySql;
\t\treturn;
\t}
\trtvalue["thread_type"] = thread_type;

\t// Everyone who must receive this message.  The sender is never in here: it
\t// already gets the very same chat_datas back in the response below.
\tstd::vector<int> recipients;

\tif (thread_type == "group") {
\t\tstd::vector<int> members;
\t\tif (!MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members)) {
\t\t\tLOG_ERROR("DealChatTextMsg: group thread {} has no member rows (uid={})",
\t\t\t\tthread_id, uid);
\t\t\trtvalue["error"] = ErrorCodes::Error_MySql;
\t\t\treturn;
\t\t}
\t\tbool in_group = false;
\t\tfor (int m : members) {
\t\t\tif (m == uid) {
\t\t\t\tin_group = true;
\t\t\t\tcontinue;
\t\t\t}
\t\t\trecipients.push_back(m);
\t\t}
\t\tif (!in_group) {
\t\t\tLOG_ERROR("DealChatTextMsg: uid {} is not a member of group thread {}", uid, thread_id);
\t\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\t\treturn;
\t\t}
\t\t// A group message has no single receiver: chat_message.recv_id stores 0
\t\t// and every member is addressed by thread_id instead.
\t\ttouid = 0;
\t\trtvalue["touid"] = 0;
\t}
\telse if (thread_type == "private") {
\t\tint thread_u1 = 0, thread_u2 = 0;
\t\tif (!MysqlMgr::GetInstance()->GetPrivateChatUsers(thread_id, thread_u1, thread_u2)) {
\t\t\tLOG_ERROR("DealChatTextMsg: thread {} not found (uid={}, touid={})", thread_id, uid, touid);
\t\t\trtvalue["error"] = ErrorCodes::Error_MySql;
\t\t\treturn;
\t\t}
\t\tif (uid != thread_u1 && uid != thread_u2) {
\t\t\tLOG_ERROR("DealChatTextMsg: uid {} is not a member of thread {}", uid, thread_id);
\t\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\t\treturn;
\t\t}
\t\tconst int peer_uid = (uid == thread_u1) ? thread_u2 : thread_u1;
\t\tif (touid != peer_uid) {
\t\t\tLOG_WARN("DealChatTextMsg: touid {} != peer {} of thread {}, fixed",
\t\t\t\ttouid, peer_uid, thread_id);
\t\t\ttouid = peer_uid;
\t\t\trtvalue["touid"] = touid;
\t\t}
\t\trecipients.push_back(peer_uid);
\t}
\telse {
\t\tLOG_ERROR("DealChatTextMsg: unknown thread type '{}' for thread {}", thread_type, thread_id);
\t\trtvalue["error"] = ErrorCodes::UidInvalid;
\t\treturn;
\t}
"""

NEW_DELIVERY = """\
\t// ---- deliver to every recipient --------------------------------------
\t// A private chat has exactly one recipient, a group has one per member.
\t// They may live on different ChatServers, so the routing below
\t// (redis USERIPPREFIX -> local session, else gRPC) runs once per uid.
\tauto& cfg = ConfigMgr::Inst();
\tauto self_name = cfg["SelfServer"]["Name"];

\tfor (int ruid : recipients) {
\t\tauto to_str = std::to_string(ruid);
\t\tauto to_ip_key = USERIPPREFIX + to_str;
\t\tstd::string to_ip_value = "";
\t\tbool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
\t\tif (!b_ip) {
\t\t\t// Offline: the row is already in MySQL, the client pulls history later.
\t\t\tLOG_INFO("DealChatTextMsg: uid {} offline, message kept in db", ruid);
\t\t\tcontinue;
\t\t}

\t\t// Same payload for everyone; only touid differs, and that is what the
\t\t// receiving client matches against itself.
\t\tJson::Value notify = rtvalue;
\t\tnotify["touid"] = ruid;

\t\tif (to_ip_value == self_name) {
\t\t\tauto to_session = UserMgr::GetInstance()->GetSession(ruid);
\t\t\tif (to_session) {
\t\t\t\tto_session->Send(notify.toStyledString(), ID_NOTIFY_TEXT_CHAT_MSG_REQ);
\t\t\t}
\t\t\tcontinue;
\t\t}

\t\tTextChatMsgReq text_msg_req;
\t\ttext_msg_req.set_fromuid(uid);
\t\ttext_msg_req.set_touid(ruid);
\t\ttext_msg_req.set_thread_id(thread_id);
\t\tfor (const auto& chat_data : chat_datas) {
\t\t\tauto *text_msg = text_msg_req.add_textmsgs();
\t\t\ttext_msg->set_unique_id(chat_data->unique_id);
\t\t\ttext_msg->set_msgcontent(chat_data->content);
\t\t\ttext_msg->set_msg_id(chat_data->message_id);
\t\t\ttext_msg->set_chat_time(chat_data->chat_time);
\t\t\t// Image/file fields.  Without them the other ChatServer rebuilds the
\t\t\t// payload as plain text and the receiver shows a raw server path.
\t\t\ttext_msg->set_msg_type(chat_data->msg_type);
\t\t\ttext_msg->set_total_size((int64_t)chat_data->total_size);
\t\t\ttext_msg->set_md5(chat_data->md5);
\t\t}

\t\tChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, notify);
\t}
}
"""

NEW_SVC_ELEMENT = (
    '\t\telement["chat_time"] = msg.chat_time();\n'
    '\t\t// Image/file fields: mirror of LogicSystem::DealChatTextMsg, so a\n'
    '\t\t// message relayed from another ChatServer keeps its real type.\n'
    '\t\telement["msg_type"] = msg.msg_type();\n'
    '\t\telement["total_size"] = (Json::Int64)msg.total_size();\n'
    '\t\telement["md5"] = msg.md5();\n'
)

OLD_SVC_ELEMENT = '\t\telement["chat_time"] = msg.chat_time();\n'

# ---------------------------------------------------------------- 工具


def read_text(path):
    raw = open(path, "rb").read()
    text = raw.decode("gbk")
    crlf = "\r\n" in text
    return text.replace("\r\n", "\n"), crlf


def write_text(path, text, crlf):
    out = text.replace("\n", "\r\n") if crlf else text
    open(path, "wb").write(out.encode("gbk"))


START_VALIDATION = "\t// ==== Never trust thread_id / touid coming from the client ===="
END_VALIDATION = "\tstd::vector<std::shared_ptr<ChatMessage>> chat_datas;"
START_DELIVERY = "\tauto to_str = std::to_string(touid);"
END_DELIVERY = "\tChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);\n}"


def replace_region(text, start_marker, end_marker, new_text, inclusive_end, tag, path):
    """把 [start_marker, end_marker) 或 [start_marker, end_marker] 换成 new_text。"""
    i = text.find(start_marker)
    if i < 0:
        raise SystemExit("%s: %s start marker not found" % (path, tag))
    if text.count(start_marker) != 1:
        raise SystemExit("%s: %s start marker count=%d" % (path, tag, text.count(start_marker)))
    j = text.find(end_marker, i)
    if j < 0:
        raise SystemExit("%s: %s end marker not found" % (path, tag))
    if inclusive_end:
        j += len(end_marker)
    return text[:i] + new_text + text[j:]


def patch_logic(path):
    text, crlf = read_text(path)
    orig = text
    if "std::vector<int> recipients;" not in text:
        text = replace_region(text, START_VALIDATION, END_VALIDATION,
                              NEW_VALIDATION, False, "validation", path)
    if "for (int ruid : recipients)" not in text:
        # 投递段要从它前面那句中文注释开始，往前退到注释行首。
        # std::to_string(touid) 在文件里出现 3 次，所以必须先定位到本函数体内。
        fn_at = text.find("void LogicSystem::DealChatTextMsg(")
        if fn_at < 0:
            raise SystemExit("%s: DealChatTextMsg not found" % path)
        if text.count(START_DELIVERY, fn_at) != 1:
            raise SystemExit("%s: delivery start marker count=%d"
                             % (path, text.count(START_DELIVERY, fn_at)))
        i = text.find(START_DELIVERY, fn_at)
        # 往前退到注释行首：那句中文注释 ("查询redis 查找touid...") 已经过时了，
        # 新版是按 recipients 逐个投递，所以连同它上面的空行一起吃掉。
        line_start = text.rfind("\n", 0, i) + 1
        if text[line_start:].startswith("\t//查询redis"):
            prev = text.rfind("\n", 0, line_start - 1) + 1
            if text[prev:line_start].strip() == "":
                line_start = prev
        head = text[:line_start]
        body = text[line_start:]
        k = body.find(END_DELIVERY)
        if k < 0:
            raise SystemExit("%s: delivery end marker not found" % path)
        tail = body[k + len(END_DELIVERY):]
        text = head + NEW_DELIVERY + tail
    if text == orig:
        return False
    write_text(path, text, crlf)
    return True


def patch_service(path):
    text, crlf = read_text(path)
    if NEW_SVC_ELEMENT in text:
        return False
    cnt = text.count(OLD_SVC_ELEMENT)
    if cnt != 1:
        raise SystemExit("%s: svc element count=%d (expect 1)" % (path, cnt))
    text = text.replace(OLD_SVC_ELEMENT, NEW_SVC_ELEMENT, 1)
    write_text(path, text, crlf)
    return True


def strip_comments(text):
    out = []
    i, n = 0, len(text)
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


def brace_balance(text):
    # 字符串/字符字面量外的花括号配平；只关心净差值，作为"没改坏结构"的粗校验
    t = strip_comments(text)
    return t.count("{") - t.count("}")


def main():
    before = {}
    for srv in SERVERS:
        for fname in ("LogicSystem.cpp", "ChatServiceImpl.cpp"):
            p = os.path.join(ROOT, srv, fname)
            before[p] = read_text(p)[0]

    for srv in SERVERS:
        for fname, fn in (("LogicSystem.cpp", patch_logic),
                          ("ChatServiceImpl.cpp", patch_service)):
            p = os.path.join(ROOT, srv, fname)
            changed = fn(p)
            print("[%s] %s/%s" % ("OK  " if changed else "SKIP", srv, fname))

    # 这次是【有意重写】一个函数体，不能用"旧行必须都在"的骨架校验。
    # 改成：(1) 括号配平不变  (2) 关键保留片段仍在  (3) 新片段一定出现
    MUST_KEEP = [
        'Defer defer([this, &rtvalue, session]() {',
        'session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);',
        'if (uid != thread_u1 && uid != thread_u2) {',
        'const int peer_uid = (uid == thread_u1) ? thread_u2 : thread_u1;',
        'auto self_name = cfg["SelfServer"]["Name"];',
        'ID_NOTIFY_TEXT_CHAT_MSG_REQ',
        'MysqlMgr::GetInstance()->AddChatMsg(chat_datas);',
        'session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);',
    ]
    MUST_NEW = [
        'std::vector<int> recipients;',
        'GetThreadType(thread_id, thread_type)',
        'GetGroupMembers(thread_id, members)',
        'for (int ruid : recipients) {',
        'text_msg->set_msg_type(chat_data->msg_type);',
        'element["msg_type"] = msg.msg_type();',
    ]

    print("\n--- 结构校验 ---")
    bad = 0
    for p, txt_before in before.items():
        txt_after = read_text(p)[0]
        rel = p.replace(ROOT + os.sep, "")

        # DealChatTextMsg 是整段重写，必须自己闭合（曾经漏掉结尾 '}' 过）
        if rel.endswith("LogicSystem.cpp"):
            fi = txt_after.find("void LogicSystem::DealChatTextMsg(")
            fj = txt_after.find("void LogicSystem::", fi + 10)
            d = 0
            for ch in txt_after[fi:fj]:
                if ch == "{":
                    d += 1
                elif ch == "}":
                    d -= 1
            if d != 0:
                print("  !! %s DealChatTextMsg 括号深度=%d（应为 0）" % (rel, d))
                bad += 1
                continue

        if brace_balance(txt_before) != brace_balance(txt_after):
            print("  !! %s 括号配平变化 %d -> %d"
                  % (rel, brace_balance(txt_before), brace_balance(txt_after)))
            bad += 1
            continue
        missing_keep = [s for s in MUST_KEEP if s not in txt_after]
        # 保留片段在两个文件里并不同时存在，按"至少命中对应文件的那几条"判断
        missing_keep = [s for s in missing_keep if s in txt_before]
        missing_new = [s for s in MUST_NEW if s not in txt_after
                       and (("LogicSystem" in p and "element" not in s)
                            or ("ChatServiceImpl" in p and "element" in s))]
        if missing_keep or missing_new:
            bad += 1
            print("  !! %s  keep-missing=%s new-missing=%s"
                  % (rel, missing_keep, missing_new))
        else:
            print("  ok  %s (括号配平 %d)" % (rel, brace_balance(txt_after)))
    if bad:
        raise SystemExit("结构校验失败：%d 个文件" % bad)

    print("\n--- GBK 自检 ---")
    for srv in SERVERS:
        for fname in ("LogicSystem.cpp", "ChatServiceImpl.cpp"):
            open(os.path.join(ROOT, srv, fname), "rb").read().decode("gbk")
    print("  全部文件仍是合法 GBK")


if __name__ == "__main__":
    main()
