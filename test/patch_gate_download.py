# -*- coding: utf-8 -*-
"""修 GateServer 静态图片下载的 404 bug。

【症状】GET /upload/<file> 返回 404，但响应体里既有完整的 PNG 二进制，
        末尾又缀了 "url not found\\r\\n"。

【根因】LogicSystem::HandleGet 里，/upload/ 分支把图片写进 response body 后
        【没有 return true】，而是继续往下走到 _get_handlers.find(path)。
        上传文件名是动态的（时间戳_哈希.png），永远不在 _get_handlers 里，
        于是 find 失败 -> return false -> HttpConnection::HandleReq 认为
        "url 没找到"，把 result 覆盖成 404，又往【同一个 body】追加了
        "url not found" —— body 没被 clear，所以两段内容一起发出去。
        客户端拿到 404，图片也废了。

【修法】在 /upload/ 分支处理完后补 return true，不再 fallthrough。

源文件是 GBK，保留 CRLF。
"""
p = r"D:\QTcode\HChat\GateServer\LogicSystem.cpp"
raw = open(p, "rb").read().decode("gbk")

old = (
    "mb.commit(con->_file_body.size());   // ★ 关键：可写 -> 可读\r\n"
    "        }\r\n"
    "    }\r\n"
    "\r\n"
    "    if (_get_handlers.find(path) == _get_handlers.end()) {\r\n"
    "        return false;\r\n"
    "    }\r\n"
    "    _get_handlers[path](con);\r\n"
    "    return true;\r\n"
    "}\r\n"
)

new = (
    "mb.commit(con->_file_body.size());   // ★ 关键：可写 -> 可读\r\n"
    "        }\r\n"
    "    }\r\n"
    "\r\n"
    "    // ★ 必须 return true，不能 fallthrough 到下面的 _get_handlers.find(path)。\r\n"
    "    //   上传文件名是动态的（时间戳_哈希.png），永远不在 _get_handlers 里，\r\n"
    "    //   find 一定失败 -> return false，HandleReq 于是把结果覆盖成 404，\r\n"
    "    //   再往同一个 body 追加 \"url not found\"（body 没被 clear），\r\n"
    "    //   客户端拿到的就是「404 + 图片 + 错误文本」，图片也废了。\r\n"
    "    return true;\r\n"
    "\r\n"
    "    if (_get_handlers.find(path) == _get_handlers.end()) {\r\n"
    "        return false;\r\n"
    "    }\r\n"
    "    _get_handlers[path](con);\r\n"
    "    return true;\r\n"
    "}\r\n"
)

cnt = raw.count(old)
if cnt != 1:
    raise SystemExit("[FAIL] anchor count = %d" % cnt)

raw = raw.replace(old, new, 1)
open(p, "wb").write(raw.encode("gbk"))
print("[OK] patched HandleGet static branch")