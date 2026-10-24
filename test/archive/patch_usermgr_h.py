# -*- coding: utf-8 -*-
"""往 GBK 的 usermgr.h 里加 UpdateLocalUserInfo 声明。"""
import os

p = r"D:\QTcode\HChat\Client\usermgr.h"
raw = open(p, "rb").read().decode("gbk")

anchor = "     std::shared_ptr<UserInfo> GetUserInfo();\r\n"
ifanchor = anchor.replace("\r\n", "\n")

add = (anchor +
       "    // 提交个人资料成功后，把新值同步进内存（否则要重新登录才生效）\r\n"
       "    void UpdateLocalUserInfo(const QString& nick, const QString& desc,\r\n"
       "                              int sex, const QString& icon);\r\n")

if "UpdateLocalUserInfo" in raw:
    print("[SKIP] already declared")
elif anchor in raw:
    raw = raw.replace(anchor, add, 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] declared")
elif ifanchor in raw:
    raw = raw.replace(ifanchor, add.replace("\r\n", "\n"), 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] declared (LF)")
else:
    print("[FAIL] anchor not found")