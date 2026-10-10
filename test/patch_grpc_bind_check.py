# -*- coding: utf-8 -*-
"""修 ChatServer 的 gRPC 端口绑定失败导致 segfault 的问题。

【复现】端口 50055 已被占用时启动 ChatServer：
        E0000 ... Failed to add port to server: ... WSA Error (10048)
        RPC Server listening on 0.0.0.0:50055     <-- 假的，其实失败了
        Segmentation fault

【根因】
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "RPC Server listening on " << server_address << std::endl;
    std::thread grpc_server_thread([&server]() {
            server->Wait();        // <-- BuildAndStart 失败返回 nullptr，这里解引用空指针
        });
    grpc::ServerBuilder::BuildAndStart() 在端口被占用/地址非法时返回 nullptr，
    原代码没检查，还打印了"listening"误导人，随后 Wait() 空指针崩溃。

【修法】拿到 server 后先判空：打印真实原因 + return 退出。

ChatServer.cpp 是 UTF-8（带 BOM），换行是 CRLF。
"""
import sys

EOL = "\r\n"


def L(*parts):
    """把每行接上 CRLF 拼成一块文本（缩进用 tab）。"""
    return "".join(p + EOL for p in parts)


OLD = L(
    "\t\t// \u6784\u5efa\u5e76\u542f\u52a8gRPC\u670d\u52a1\u5668",
    "\t\tstd::unique_ptr<grpc::Server> server(builder.BuildAndStart());",
    '\t\tstd::cout << "RPC Server listening on " << server_address << std::endl;',
)

NEW = L(
    "\t\t// \u6784\u5efa\u5e76\u542f\u52a8gRPC\u670d\u52a1\u5668",
    "\t\tstd::unique_ptr<grpc::Server> server(builder.BuildAndStart());",
    "\t\t// \u2605 \u5fc5\u987b\u5224\u7a7a\uff1a\u7aef\u53e3\u88ab\u5360\u7528 / \u5730\u5740\u975e\u6cd5\u65f6 BuildAndStart \u8fd4\u56de nullptr\u3002",
    "\t\t//   \u539f\u6765\u76f4\u63a5\u5f80\u4e0b\u8d70 \u2014\u2014 \u5148\u6253\u5370\u4e00\u53e5\u5047\u7684 \"RPC Server listening\"\uff0c",
    "\t\t//   \u7d27\u63a5\u7740 RPC \u7ebf\u7a0b\u8c03 server->Wait() \u89e3\u5f15\u7528\u7a7a\u6307\u9488 -> Segmentation fault\u3002",
    "\t\tif (server == nullptr) {",
    '\t\t\tstd::cerr << "FATAL: gRPC \u670d\u52a1\u542f\u52a8\u5931\u8d25\uff0c\u65e0\u6cd5\u76d1\u542c " << server_address << std::endl;',
    '\t\t\tstd::cerr << "       \u5e38\u89c1\u539f\u56e0\uff1a\u7aef\u53e3\u5df2\u88ab\u5360\u7528\uff08\u662f\u5426\u5df2\u7ecf\u6709\u4e00\u4e2a ChatServer \u5728\u8dd1\uff1f\uff09\uff0c\u6216 Host/RPCPort \u914d\u7f6e\u4e0d\u5bf9\u3002" << std::endl;',
    '\t\t\tstd::cerr << "       RPC \u662f ChatServer \u4e4b\u95f4\u8f6c\u53d1\u6d88\u606f\u7528\u7684\uff0c\u8d77\u4e0d\u6765\u5c31\u65e0\u6cd5\u5de5\u4f5c\uff0c\u8fd9\u91cc\u76f4\u63a5\u9000\u51fa\u3002" << std::endl;',
    "\t\t\treturn 1;",
    "\t\t}",
    '\t\tstd::cout << "RPC Server listening on " << server_address << std::endl;',
)

for srv in ("ChatServer", "ChatServer2"):
    p = "D:\\QTcode\\HChat\\%s\\ChatServer.cpp" % srv
    raw = open(p, "rb").read().decode("utf-8", errors="replace")
    if "server == nullptr" in raw:
        print("[SKIP] %s: already patched" % srv)
        continue
    cnt = raw.count(OLD)
    if cnt != 1:
        print("[FAIL] %s: anchor count = %d" % (srv, cnt))
        sys.exit(1)
    raw = raw.replace(OLD, NEW, 1)
    open(p, "wb").write(raw.encode("utf-8"))
    print("[OK] %s: gRPC bind failure handled" % srv)