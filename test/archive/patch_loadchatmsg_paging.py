# -*- coding: utf-8 -*-
"""补MysqlDao::LoadChatMsg 的 load_more 判断（无限加载的第二重原因）。

【症状】SQL 修好后仍然有问题：某个会话消息超过 10 条时，
        只加载前 10 条就不再翻了。

【根因】代码里 fetch_limit = page_size + 1（多取一条，
        本意是拿「第page_size+1 条存在」来判定还有更多），
        但读完结果集后【从来没写过 page_res->load_more】，
        它一直是初始化的 false。next_cursor 也永远停在传入的
        last_message_id，不推进。结果：
          - load_more 恒 false -> 客户端以为加载完了
          - next_cursor 不动-> 再点也没法继续加载

【修法】按 GetUserThreads 里同样的写法：
        读满 page_size+1 条 -> load_more=true，丢掉那第 N+1 条，
        next_cursor = 最后一条的 message_id。

源文件是 GBK；该函数内换行是单个 \r\n。
"""
import sys

for srv in ("ChatServer", "ChatServer2"):
    p = r"D:\QTcode\HChat\%s\MysqlDao.cpp" % srv
    raw = open(p, "rb").read().decode("gbk")

    old = ("\t\t\tpage_res->messages.push_back(std::move(msg));\r\n"
           "\t\t}\r\n"
           "\r\n"
           "\t\treturn page_res;\r\n")

    new = ("\t\t\tpage_res->messages.push_back(std::move(msg));\r\n"
           "\t\t}\r\n"
           "\r\n"
           "\t\t// ★ 判断是否还有更多 —— 这段原来整个漏掉了。\r\n"
           "\t\t//   SQL 用 fetch_limit = page_size + 1 多取一条：\r\n"
           "\t\t//   如果真的读到了第 N+1 条，说明后面还有，load_more = true，\r\n"
           "\t\t//   并把那条丢掉（只返回 page_size 条）。\r\n"
           "\t\t//   next_cursor 必须推进到本页最后一条的 message_id，\r\n"
           "\t\t//   否则下次请求还是同一个游标，会拿到同一页数据。\r\n"
           "\t\t//   漏掉这段的后果：load_more 恒为 false -> 客户端以为加载完了，\r\n"
           "\t\t//   超过 10 条的消息再也翻不出来。\r\n"
           "\t\tif ((int)page_res->messages.size() > page_size) {\r\n"
           "\t\t\tpage_res->load_more = true;\r\n"
           "\t\t\tpage_res->messages.pop_back();\r\n"
           "\t\t}\r\n"
           "\t\tif (!page_res->messages.empty()) {\r\n"
           "\t\t\tpage_res->next_cursor = (int)page_res->messages.back().message_id;\r\n"
           "\t\t}\r\n"
           "\r\n"
           "\t\treturn page_res;\r\n")

    if "messages.pop_back()" in raw:
        print("[SKIP] %s: 已修复" % srv)
        continue

    cnt = raw.count(old)
    if cnt != 1:
        print("[FAIL] %s: anchor count = %d" % (srv, cnt))
        sys.exit(1)

    raw = raw.replace(old, new, 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] %s: 补回 load_more / next_cursor 判断" % srv)