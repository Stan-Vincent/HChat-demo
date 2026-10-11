# -*- coding: utf-8 -*-
"""给 GateServer 的 upload_image 加服务端大小上限。

【问题】解码后直接写文件，全程没有任何大小检查。
       客户端 UploadManager 有 MAX_IMAGE_SIZE(5MB) 的限制，
       但那只是客户端的 —— 直接 POST 就能绕过，
       然后往磁盘上写任意大的文件（占满磁盘 / 打爆内存）。

【修法】base64 解码之后、写文件之前，显式拒绝超过 5MB 的内容。
       放在解码之后判断（而不是看请求体大小），
       因为 base64 有 4/3 的膨胀，解码后的大小才是真正落盘的字节数。

LogicSystem.cpp 是 GBK，换行 CRLF。
"""
import sys

EOL = "\r\n"


def blk(*lines):
    return "".join(l + EOL for l in lines)


p = r"D:\QTcode\HChat\GateServer\LogicSystem.cpp"
raw = open(p, "rb").read().decode("gbk", errors="replace")

OLD = blk(
    "        // \u2605 base64 \u89e3\u7801\u51fd\u6570\u5185\u90e8\u5df2\u7ecf\u4f1a\u8df3\u8fc7\u6362\u884c/\u7a7a\u683c\uff08\u4f20\u8f93\u9014\u4e2d\u53ef\u80fd\u88ab\u6298\u884c\uff09",
    "        auto raw = base64Decode(b64);",
    "        if (raw.empty()) {",
    '            root["error"] = ErrorCodes::Error_Json;',
    '            root["msg"] = "base64 decode failed";',
    "            beast::ostream(connection->_response.body()) << root.toStyledString();",
    "            return true;",
    "        }",
)

NEW = blk(
    "        // \u2605 base64 \u89e3\u7801\u51fd\u6570\u5185\u90e8\u5df2\u7ecf\u4f1a\u8df3\u8fc7\u6362\u884c/\u7a7a\u683c\uff08\u4f20\u8f93\u9014\u4e2d\u53ef\u80fd\u88ab\u6298\u884c\uff09",
    "        auto raw = base64Decode(b64);",
    "        if (raw.empty()) {",
    '            root["error"] = ErrorCodes::Error_Json;',
    '            root["msg"] = "base64 decode failed";',
    "            beast::ostream(connection->_response.body()) << root.toStyledString();",
    "            return true;",
    "        }",
    "",
    "        // \u2605 \u670d\u52a1\u7aef\u5fc5\u987b\u81ea\u5df1\u9650\u4e00\u6b21\u5927\u5c0f\u3002",
    "        //   \u5ba2\u6237\u7aef UploadManager \u7684 5MB \u9650\u5236\u53ea\u662f\u5ba2\u6237\u7aef\u7684\uff0c",
    "        //   \u76f4\u63a5 POST \u5c31\u80fd\u7ed5\u8fc7 \u2014\u2014 \u90a3\u6837\u53ef\u4ee5\u5f80\u78c1\u76d8\u4e0a\u5199\u4efb\u610f\u5927\u7684\u6587\u4ef6\u3002",
    "        //   \u5728\u89e3\u7801\u4e4b\u540e\u5224\u65ad\uff08base64 \u6709 4/3 \u81a8\u80c0\uff09\uff0c",
    "        //   raw.size() \u624d\u662f\u771f\u6b63\u8981\u843d\u76d8\u7684\u5b57\u8282\u6570\u3002",
    "        const std::size_t MAX_UPLOAD_BYTES = 5 * 1024 * 1024;   // 5MB",
    "        if (raw.size() > MAX_UPLOAD_BYTES) {",
    '            root["error"] = ErrorCodes::Error_Json;',
    '            root["msg"] = "file too large (max 5MB)";',
    "            beast::ostream(connection->_response.body()) << root.toStyledString();",
    "            return true;",
    "        }",
)

if "MAX_UPLOAD_BYTES" in raw:
    print("[SKIP] already patched")
elif raw.count(OLD) != 1:
    print("[FAIL] anchor count = %d" % raw.count(OLD))
    sys.exit(1)
else:
    raw = raw.replace(OLD, NEW, 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] upload_image 服务端大小上限已加")