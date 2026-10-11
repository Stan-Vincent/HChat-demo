# -*- coding: utf-8 -*-
"""给 GateServer 的 HTTP 解析设显式 body_limit，并给超限返回干净的 413。

【问题】原来：
        http::async_read(_socket, _buffer, _request, ...)
    直接用 http::request 读，beast 的 request_parser 默认 body_limit 只有 1MB。
    图片要 base64（膨胀 4/3），所以：
      - 原图超过 ~750KB 就被拒
      - 而且是【直接断开连接】，客户端收到 WinError 10053，
        没有任何错误提示，用户只会看到"上传失败"却不知道为什么
    手机照片随便 3-5MB —— 这是实打实的功能 bug。

【修法】
    1. 改用 request_parser，显式 body_limit(8MB)
       （客户端上限 5MB -> base64 后约 6.7MB，留足余量）
    2. 超限时返回标准 413 Payload Too Large，不再默默断连

HttpConnection.cpp 是 GBK，换行 CRLF。
"""
import sys

EOL = "\r\n"


def blk(*lines):
    return "".join(l + EOL for l in lines)


p = r"D:\QTcode\HChat\GateServer\HttpConnection.cpp"
raw = open(p, "rb").read().decode("gbk", errors="replace")

OLD = blk(
    "    auto self = shared_from_this();",
    "    http::async_read(_socket, _buffer, _request, ",
    "        [self](beast::error_code ec,std::size_t bytes_transferred) ",
    "        {",
    "            try {",
    '                std::cout << "HttpConnection Async_read...\\n";',
    "                if (ec) {",
    '                    std::cout << "Http read err is " << ec.what() << std::endl;',
    "                    return;",
    "                }",
)

NEW = blk(
    "    auto self = shared_from_this();",
    "",
    "    // \u2605 \u663e\u5f0f\u8bbe\u7f6e body_limit\u3002",
    "    //   \u9ed8\u8ba4 1MB \u592a\u5c0f\uff1a\u56fe\u7247 base64 \u4f1a\u81a8\u80c0 4/3\uff0c",
    "    //   \u539f\u56fe\u8d85\u8fc7 ~750KB \u5c31\u88ab\u62d2\uff0c\u800c\u4e14\u662f\u76f4\u63a5\u65ad\u8fde\u63a5\u3001",
    "    //   \u5ba2\u6237\u7aef\u6ca1\u6709\u4efb\u4f55\u9519\u8bef\u63d0\u793a\uff08\u53ea\u4f1a\u6536\u5230 10053\uff09\u3002",
    "    //   \u5ba2\u6237\u7aef\u4e0a\u9650 5MB -> base64 \u540e\u7ea6 6.7MB\uff0c\u8fd9\u91cc\u7ed9 8MB \u7559\u8db3\u91cf\u3002",
    "    self->_parser.body_limit(8 * 1024 * 1024);",
    "",
    "    http::async_read(_socket, _buffer, self->_parser,",
    "        [self](beast::error_code ec,std::size_t bytes_transferred) ",
    "        {",
    "            try {",
    '                std::cout << "HttpConnection Async_read...\\n";',
    "                if (ec) {",
    "                    // \u2605 \u8d85\u9650\u65f6\u7ed9\u4e00\u4e2a\u5e72\u51c0\u7684 413\uff0c",
    "                    //   \u800c\u4e0d\u662f\u9ed8\u9ed8\u65ad\u5f00\u8fde\u63a5",
    "                    if (ec == http::error::body_limit) {",
    "                        std::cout << \"request body too large (>=8MB), send 413\" << std::endl;",
    "                        self->_response.result(http::status::payload_too_large);",
    '                        self->_response.set(http::field::content_type, "text/plain");',
    '                        beast::ostream(self->_response.body()) << "request body too large";',
    "                        self->_response.content_length(self->_response.body().size());",
    "                        self->WriteResponse();",
    "                        return;",
    "                    }",
    '                    std::cout << "Http read err is " << ec.what() << std::endl;',
    "                    return;",
    "                }",
    "",
    "                // \u89e3\u6790\u5b8c\u6210\uff0c\u628a\u6d88\u606f\u53d6\u51fa\u6765\u5b58\u5230 _request",
    "                self->_request = self->_parser.release();",
)

if "_parser.body_limit" in raw:
    print("[SKIP] already patched")
elif raw.count(OLD) != 1:
    print("[FAIL] anchor count = %d" % raw.count(OLD))
    sys.exit(1)
else:
    raw = raw.replace(OLD, NEW, 1)
    open(p, "wb").write(raw.encode("gbk"))
    print("[OK] HttpConnection.cpp: body_limit 8MB + 413")