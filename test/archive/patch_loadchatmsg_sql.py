# -*- coding: utf-8 -*-
"""修ChatServer 的 LoadChatMsg SQL 语法错误 —— 这是「登录后无限加载 + 卡死」的根因。

【症状】登录后无限加载，模态加载框不消失，整个程序无法退出。

【根因】MysqlDao::LoadChatMsg 里的 SQL 缺了 FROM / WHERE 那一行：
        SELECT message_id, ..., status
               AND message_id > ?      <-- 直接AND，语法错误
        ORDER BY message_id ASC
    prepareStatement 抛 SQLSyntaxErrorException -> catch 返回 nullptr
    -> LogicSystem::LoadChatMsg 置 error=LOAD_CHAT_FAILED(1013)
    -> 客户端 tcpmgr 的 handler 看到 error!=0 直接 return
    -> ChatDialog::slot_load_chat_msg 永远走不到 showLoadingDlg(false)
    -> 模态 LoadingDlg 一直挡着窗口，点关闭都没用（modal 拦截所有输入）。

    这是改 AddChatMsg 字段（加 msg_type/unique_name/total_size/md5）时误删的。

【修法】补回 FROM chat_message WHERE thread_id = ?。

源文件是 GBK，注意该文件的换行是 "\r\n"（双CR）。
"""
import sys

for srv in ("ChatServer", "ChatServer2"):
    p = r"D:\QTcode\HChat\%s\MysqlDao.cpp" % srv
    raw = open(p, "rb").read().decode("gbk")

    # 用纯 ASCII 锚点
    old = ("               created_at, updated_at, status\r\n"
           "          AND message_id > ?\r\n")
    new = ("               created_at, updated_at, status\r\n"
           "          FROM chat_message\r\n"
           "         WHERE thread_id = ?\r\n"
           "           AND message_id > ?\r\n")

    if "FROM chat_message" in raw and "WHERE thread_id = ?" in raw:
        print("[SKIP] %s: 已修复" % srv)
        continue

    cnt = raw.count(old)
    if cnt != 1:
        print("[FAIL] %s: anchor count = %d" % (srv, cnt))
        sys.exit(1)

    raw = raw.replace(old, new, 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] %s: 补回 FROM chat_message WHERE" % srv)