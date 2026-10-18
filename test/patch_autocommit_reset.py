#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
修复连接池 autocommit 泄漏（群聊/加好友等偶发失败的根因）。

现象：CreateGroupChat / AddGroupMembers / AddFriend / AddChatMsg 等事务方法
调用 conn->setAutoCommit(false) 后只 commit()/rollback()，却从不把 autocommit
恢复成 true 就还回连接池。借到这种"中毒"连接的非事务读方法
（GetThreadType / GetGroupMembers / GetUserThreads ...）在 autocommit=false 下，
第一条语句会开启 REPEATABLE READ 隐式事务，快照固定在借到连接的瞬间——若那一刻
群/成员还没提交，就读不到刚写入的行，表现成"会话列表找不到群""成员校验失败"
等偶发错误（同样的二进制时好时坏，9/9 与 4/9 交替）。

修复：在连接池唯一出口 returnConnection 里把 autocommit 复位成 true。
这是所有 DAO 借/还连接的唯一收敛点，一处修改覆盖全部方法，且不影响事务方法
（它们自己的 commit/rollback 已经定稿写操作，复位只是恢复池不变式）。

GBK + CRLF 安全：按字节 decode('gbk') -> 替换 -> encode('gbk')，保留 \r\n。
两个 ChatServer 必须逐字节一致。幂等：已打过补丁则跳过。
"""
import sys
import io

TARGETS = [
    "ChatServer/MysqlDao.h",
    "ChatServer2/MysqlDao.h",
]

OLD = (
    "\tvoid returnConnection(std::unique_ptr<SqlConnection> con) {\r\n"
    "\t\tstd::unique_lock<std::mutex> lock(mutex_);\r\n"
    "\t\tif (b_stop_) {\r\n"
    "\t\t\treturn;\r\n"
    "\t\t}\r\n"
    "\t\tpool_.push(std::move(con));\r\n"
    "\t\tcond_.notify_one();\r\n"
    "\t}\r\n"
)

NEW = (
    "\tvoid returnConnection(std::unique_ptr<SqlConnection> con) {\r\n"
    "\t\tstd::unique_lock<std::mutex> lock(mutex_);\r\n"
    "\t\tif (b_stop_) {\r\n"
    "\t\t\treturn;\r\n"
    "\t\t}\r\n"
    "\t\t// Reset autocommit before pooling back. Transactional DAOs call\r\n"
    "\t\t// setAutoCommit(false) but never restore it; a pooled connection left\r\n"
    "\t\t// in autocommit=false makes the next borrower start an implicit\r\n"
    "\t\t// REPEATABLE READ transaction (stale snapshots -> freshly-committed\r\n"
    "\t\t// rows missed by reads) or skip the implicit commit on writes.\r\n"
    "\t\ttry {\r\n"
    "\t\t\tif (con && con->_con) {\r\n"
    "\t\t\t\tcon->_con->setAutoCommit(true);\r\n"
    "\t\t\t}\r\n"
    "\t\t}\r\n"
    "\t\tcatch (...) {\r\n"
    "\t\t}\r\n"
    "\t\tpool_.push(std::move(con));\r\n"
    "\t\tcond_.notify_one();\r\n"
    "\t}\r\n"
)


def patch_file(path: str) -> str:
    with open(path, "rb") as f:
        raw = f.read()
    # GBK 解码（含中文注释），失败即报警
    try:
        text = raw.decode("gbk")
    except UnicodeDecodeError as e:
        return "SKIP  解码失败 %s: %s" % (path, e)

    if "con->_con->setAutoCommit(true);" in text:
        return "OK    已打过补丁，跳过 %s" % path

    cnt = text.count(OLD)
    if cnt != 1:
        return "FAIL  锚点匹配数=%d（期望 1）%s" % (cnt, path)

    text = text.replace(OLD, NEW, 1)
    out = text.encode("gbk")
    with open(path, "wb") as f:
        f.write(out)

    # 自检：能重新按 gbk 解码 + 新片段存在 + 旧片段消失
    with open(path, "rb") as f:
        check = f.read().decode("gbk")
    assert "con->_con->setAutoCommit(true);" in check
    assert OLD not in check
    return "PATCH 已应用 %s" % path


def main():
    results = []
    for t in TARGETS:
        try:
            results.append(patch_file(t))
        except FileNotFoundError:
            results.append("SKIP  文件不存在 %s" % t)
        except Exception as e:
            results.append("FAIL  %s: %s" % (t, e))
    for r in results:
        print(r)
    if any(r.startswith("FAIL") for r in results):
        sys.exit(1)


if __name__ == "__main__":
    main()
