#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Restore Chinese comments in GateServer/HttpConnection.{h,cpp}.

Both files were rewritten as pure ASCII during the streaming-upload port,
which silently dropped the original Chinese documentation (19 lines in the
header, 48 in the cpp). This patcher translates the comment blocks back to
Chinese and re-encodes both files as GBK, matching the rest of GateServer.

Only comment text is touched. Every anchor must match exactly once, and the
"code skeleton" (source with comments stripped) must be byte-identical
before and after, so it is impossible for this script to change behaviour.
"""
import re
import sys

H_PATH = r"D:\QTcode\HChat\GateServer\HttpConnection.h"
C_PATH = r"D:\QTcode\HChat\GateServer\HttpConnection.cpp"


def strip_comments(text):
    """Drop // and /* */ comments and blank out string literal contents.

    String contents are normalised to "" so that translating a log message
    (which is intended) does not look like a code change, while any real
    structural edit still shows up as a skeleton difference.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            out.append('""')
            i += 1
            while i < n:
                if text[i] == "\\" and i + 1 < n:
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if c == "'":
            q = c
            out.append(c)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
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
    return "".join(out)


def skeleton(text):
    """Whitespace-insensitive, comment-free view of the code."""
    return re.sub(r"\s+", "", strip_comments(text))


def apply(path, pairs):
    raw = open(path, "rb").read()
    try:
        text = raw.decode("gbk")
    except UnicodeDecodeError:
        text = raw.decode("utf-8")
    before = skeleton(text)

    for old, new in pairs:
        cnt = text.count(old)
        if cnt == 0 and text.count(new) > 0:
            continue          # already applied, keep the patcher idempotent
        if cnt != 1:
            print("FATAL %s: anchor found %d times (expected 1):\n---\n%s\n---"
                  % (path, cnt, old[:200]))
            sys.exit(1)
        text = text.replace(old, new)

    after = skeleton(text)
    if before != after:
        print("FATAL %s: code skeleton changed!" % path)
        # locate first difference for the report
        for i, (a, b) in enumerate(zip(before, after)):
            if a != b:
                print("  first diff at %d:\n  before: ...%s...\n  after:  ...%s..."
                      % (i, before[max(0, i - 40):i + 40],
                         after[max(0, i - 40):i + 40]))
                break
        else:
            print("  length differs: %d vs %d" % (len(before), len(after)))
        sys.exit(1)

    out = text.encode("gbk")
    open(path, "wb").write(out)
    back = open(path, "rb").read().decode("gbk")
    assert back == text, "GBK round trip failed"
    print("OK %-34s %5d bytes, %3d non-ascii, skeleton unchanged"
          % (path, len(out), sum(1 for b in out if b > 127)))


# ---------------------------------------------------------------- header ----
H_PAIRS = [
    (
        "// ---- streaming upload tuning (see HttpConnection.cpp for the rationale) ----\n"
        "// Chunk the request body is copied into before it hits the disk.\n",
        "// ---- 流式上传相关参数（设计原理见 HttpConnection.cpp） ----\n"
        "// 请求体每次拷入的块大小，落盘也按这个粒度写。\n",
    ),
    (
        "// Parser-level safety net only. Beast compares body_limit against the declared\n"
        "// Content-Length while parsing the REQUEST HEADER (basic_parser.ipp), and it is\n"
        "// a decrementing budget, so it must be armed before async_read. It is kept\n"
        "// deliberately loose: the user-facing caps below are what actually decide\n"
        "// whether a request is accepted, and a loose parser limit lets a REFUSED upload\n"
        "// drain its body so the client can still read the error response.\n"
        "// Beast's default for requests is only 1MB, which silently rejected anything\n"
        "// bigger even when the route itself was fine with it.\n",
        "// 解析器级别的兜底上限，仅此而已。Beast 在解析请求头时就把 body_limit 和声明\n"
        "// 的 Content-Length 做比较（basic_parser.ipp），而且它是递减的预算而不是累计\n"
        "// 值，所以必须在 async_read 之前装好。这里故意放宽：真正决定收不收的是下面两\n"
        "// 个业务上限，而放宽解析器上限有一个具体好处——被拒绝的上传还能把 body 排空，\n"
        "// 客户端才读得到错误响应。Beast 对请求的默认值只有 1MB，原来连正常请求都会被\n"
        "// 悄悄拒掉。\n",
    ),
    (
        "// Real cap for non-upload routes (JSON payloads). Preserves the 1MB ceiling\n"
        "// Beast's default used to impose, now that the parser limit is loose.\n",
        "// 非上传路由的真实上限（JSON 负载）。解析器上限放宽之后，原来 Beast 默认\n"
        "// 1MB 的紧箍咒挪到这里来戴。\n",
    ),
    (
        "// Real cap for streamed uploads, enforced twice: against the declared\n"
        "// Content-Length in PrepareUpload(), and against the bytes actually written in\n"
        "// OnReadBody() so a chunked upload that declares no length cannot bypass it.\n",
        "// 流式上传的真实上限，两处共同把关：PrepareUpload() 里校验声明的\n"
        "// Content-Length，OnReadBody() 里校验实际写出的字节数，这样不带长度的\n"
        "// chunked 上传也绕不过去。\n",
    ),
    (
        "// A connection that produced no activity for this long is considered dead.\n",
        "// 连接多久没有任何活动就判定为死亡。\n",
    ),
    (
        "// How often the idle patrol re-checks the connection.\n",
        "// 空闲巡检每隔多久复查一次。\n",
    ),
    (
        "\tvoid Start();\n",
        "\t//启动连接：装 body_limit、启动空闲巡检、开始读请求\n"
        "\tvoid Start();\n",
    ),
    (
        "    // ---- streaming upload state, shared with LogicSystem ----\n"
        "    // Filled by HttpConnection once the request header is parsed; read by\n"
        "    // LogicSystem::PrepareUpload() and consumed by LogicSystem::FinishUpload().\n",
        "    // ---- 流式上传状态，与 LogicSystem 共享 ----\n"
        "    // 由 HttpConnection 在请求头解析完成后填充，LogicSystem::PrepareUpload()\n"
        "    // 读取、LogicSystem::FinishUpload() 消费。\n",
    ),
    (
        "    bool          _is_upload = false;   // true for POST /upload_image|/upload_file\n"
        "    bool          _upload_finished = false;\n",
        "    bool          _is_upload = false;   // POST /upload_image|/upload_file 为真\n"
        "    bool          _upload_finished = false;   // 上传是否已成功收尾\n",
    ),
    (
        "    std::string   _upload_file_name;    // original name from the query string\n"
        "    std::string   _upload_ext;          // sanitized extension\n"
        "    std::string   _upload_unique_name;  // final name on disk\n"
        "    std::string   _upload_path;         // current path on disk (.part or final)\n"
        "    std::ofstream _upload_ofs;          // streamed output file\n"
        "    std::uint64_t _upload_written = 0;  // bytes written so far\n",
        "    std::string   _upload_file_name;    // 查询串里的原始文件名\n"
        "    std::string   _upload_ext;          // 消毒后的扩展名\n"
        "    std::string   _upload_unique_name;  // 落盘后的最终文件名\n"
        "    std::string   _upload_path;         // 磁盘上的当前路径（.part 或最终名）\n"
        "    std::ofstream _upload_ofs;          // 流式输出文件\n"
        "    std::uint64_t _upload_written = 0;  // 已写入的字节数\n",
    ),
    (
        "    // FNV-1a running state, updated per chunk so the content hash is known\n"
        "    // without ever holding the whole file in memory.\n",
        "    // FNV-1a 滚动哈希状态，每块更新一次，这样不用把整个文件留在内存里，\n"
        "    // 也能在最后算出内容哈希。\n",
    ),
    (
        "    // Accumulated body for non-upload routes (small JSON payloads).\n"
        "    // Replaces buffers_to_string(_request.body().data()).\n",
        "    // 非上传路由累积的请求体（很小的 JSON 负载）。\n"
        "    // 取代原来的 buffers_to_string(_request.body().data())。\n",
    ),
    (
        "    // Periodic idle patrol. A one-shot deadline armed at accept time kills\n"
        "    // slow-but-progressing uploads, so we re-check on an interval instead.\n"
        "    void ArmDeadline();\n",
        "    // 周期性的空闲巡检。accept 时一次性 armed 的定时器会误杀“慢但在推进”的\n"
        "    // 上传，所以改成按间隔反复复查。\n"
        "    void ArmDeadline();\n",
    ),
    (
        "    // Serialize _response and shut the connection down.\n"
        "    void WriteResponse();\n",
        "    //把处理好的响应异步发回客户端，发完后关闭连接的发送端，并取消超时定时器\n"
        "    void WriteResponse();\n",
    ),
    (
        "    // Fill _response with a JSON error body and write it. Used by every\n"
        "    // failure path in the streaming read loop.\n"
        "    void WriteJsonError(const char* msg);\n",
        "    // 用 JSON 错误体填充 _response 并发送。流式读循环里的每条失败路径都走这里。\n"
        "    void WriteJsonError(const char* msg);\n",
    ),
    (
        "    // Parse the request URI into _get_url / _get_params.\n"
        "    void PreParseGetParam();\n",
        "    //从 HTTP 请求的 URI 里，切分出路径和查询参数，存到成员变量 _get_url 和 _get_params\n"
        "    void PreParseGetParam();\n",
    ),
    (
        "    // Dispatch a fully received message to LogicSystem.\n"
        "    void HandleReq();\n",
        "    //把完整收到的请求交给 LogicSystem 分发处理\n"
        "    void HandleReq();\n",
    ),
    (
        "    // Streaming read loop.\n"
        "    void DoReadBody();\n",
        "    // 流式读循环\n"
        "    void DoReadBody();\n",
    ),
    (
        "    tcp::socket _socket;\n",
        "    //套接字\n"
        "    tcp::socket _socket;\n",
    ),
    (
        "    //_buffer is the read buffer\n",
        "    //_buffer 用来接受数据\n",
    ),
    (
        "    // buffer_body lets us consume the request body incrementally instead of\n"
        "    // buffering the whole payload in RAM: body().data / body().size must be\n"
        "    // re-armed before every async_read, and the parser reports need_buffer\n"
        "    // once the destination is full.\n",
        "    // buffer_body 让请求体可以一小块一小块地消费，而不是整个塞进内存：\n"
        "    // body().data / body().size 必须在每次 async_read 之前重新装填，\n"
        "    // 目标缓冲区满了的时候解析器会报 need_buffer。\n",
    ),
    (
        "    // _response is the response sent back to the client, filled in HandleReq()\n",
        "    // _response 用来回应客户端 在HandleReq()里填充内容\n",
    ),
    (
        "    //_deadline is the idle patrol timer\n",
        "    //_deadline 用来做定时器判断请求是否超时\n",
    ),
    (
        "    // Destination the parser copies body bytes into.\n"
        "    std::vector<char> _scratch;\n"
        "    bool _header_done = false;\n"
        "    // Set when an upload route was refused (PrepareUpload returned false) but\n"
        "    // the request body has not arrived yet. The body is consumed and discarded\n"
        "    // until the parser is done, and only then is the error response written:\n"
        "    // answering straight away would race with the client, which is still\n"
        "    // busy sending its file and would most likely see a reset instead of the\n"
        "    // JSON error.\n"
        "    bool _drain_mode = false;\n",
        "    // 解析器把请求体字节拷入的目标缓冲区。\n"
        "    std::vector<char> _scratch;\n"
        "    bool _header_done = false;\n"
        "    // 上传路由被拒绝（PrepareUpload 返回 false）、但请求体还没收完时置位。\n"
        "    // 这时继续把 body 读出来扔掉，等解析器跑完再写错误响应：立刻回复会和\n"
        "    // 客户端赛跑——它还在忙着发文件，大概率只会看到一个 reset 而不是 JSON。\n"
        "    bool _drain_mode = false;\n",
    ),
    (
        "    //get url\n"
        "\tstd::string _get_url;\n",
        "    //get请求的url\n"
        "\tstd::string _get_url;\n",
    ),
    (
        "    // Raw bytes of a static resource served by GET /upload/<name>. Kept in\n"
        "    // memory here because beast::buffers_body would point into storage that\n"
        "    // does not outlive the handler.\n",
        "    // ★ 存放静态资源（upload 下的文件）的二进制内容。\n"
        "    //   必须放在成员里：beast::buffers_body 持有的是指针，\n"
        "    //   如果指向 handler 里的局部变量，返回后数据就没了，客户端会收到乱码。\n",
    ),
    (
        "    //get params of the url, structure: key->value\n",
        "    //get请求的url的参数 结构:key->value\n",
    ),
]

# ------------------------------------------------------------------- cpp ----
C_PAIRS = [
    (
        "    // An abandoned streaming upload must not leave a .part file behind:\n"
        "    // upload/ would slowly fill with garbage on every dropped connection.\n",
        "    // 中断的流式上传不能留下 .part 文件：否则每掉一个连接，\n"
        "    // upload/ 目录就会慢慢堆满垃圾。\n",
    ),
    (
        "    // body_limit is compared against the declared Content-Length while the\n"
        "    // REQUEST HEADER is being parsed (boost/beast/http/impl/basic_parser.ipp),\n"
        "    // and it behaves as a decrementing budget rather than a running total.\n"
        "    // So it has to be armed before the first async_read and must be at least\n"
        "    // as large as the biggest body we accept. Beast's default for requests\n"
        "    // is only 1MB, which silently rejected anything bigger.\n",
        "    // ★ 显式设置 body_limit。\n"
        "    //   Beast 在解析请求头时就把 body_limit 和声明的 Content-Length 做比较\n"
        "    //   （boost/beast/http/impl/basic_parser.ipp），而且它是递减的预算而不是\n"
        "    //   累计值，所以必须在第一次 async_read 之前装好，且要能覆盖我们接受的\n"
        "    //   最大请求体。Beast 对请求的默认值只有 1MB，比它大的都会被悄悄拒掉。\n",
    ),
    (
        "    // Patrol on an interval and only close after kIdleTimeoutSec of NO\n"
        "    // activity. Refreshing from inside the read handler is not enough: a peer\n"
        "    // that stalls produces no read completions at all, so a one-shot timer\n"
        "    // armed at accept time still kills a slow-but-progressing upload.\n",
        "    // 按间隔巡检，只有连续 kIdleTimeoutSec 没有任何活动才关闭连接。\n"
        "    // 光在读回调里刷新是不够的：卡住的对端根本不会产生读完成事件，\n"
        "    // 所以 accept 时一次性 armed 的定时器依然会误杀“慢但在推进”的上传。\n",
    ),
    (
        "    // buffer_body copies body bytes into a caller supplied buffer, so the\n"
        "    // destination has to be re-armed before every read.\n",
        "    // buffer_body 把请求体字节拷入调用方给的缓冲区，\n"
        "    // 所以目标缓冲区必须在每次读之前重新装填。\n",
    ),
    (
        "// Every failure path answers with HTTP 200 plus an error code inside the JSON\n"
        "// body. That is the shape the client has always parsed, so a refused upload is\n"
        "// reported through the normal ErrorCodes path instead of depending on the\n"
        "// client inspecting the HTTP status.\n",
        "// 每条失败路径都用 HTTP 200 + JSON 里的错误码来回复。这是客户端一直在解析的\n"
        "// 形状，所以被拒的上传会走正常的 ErrorCodes 分支上报，而不是依赖客户端去检查\n"
        "// HTTP 状态码。\n",
    ),
    (
        "        // Declared Content-Length is over the parser safety net. The parser is\n"
        "        // in a failed state here, so draining the rest of the body is not\n"
        "        // possible; answer immediately and accept that a peer which is still\n"
        "        // sending may see a reset instead of this response.\n",
        "        // 声明的 Content-Length 超过了解析器兜底上限。此时解析器已进入失败\n"
        "        // 状态，没法把剩下的 body 排空；只能立即回复，并接受一个现实：还在发\n"
        "        // 数据的对端可能看到的是 reset 而不是这个响应。\n",
    ),
    (
        "    // The header is always parsed before the first body byte is delivered,\n"
        "    // so this branch runs exactly once: pick the route and, for uploads,\n"
        "    // open the output file before any chunk arrives.\n",
        "    // 请求头总是在第一个 body 字节交付之前就解析完了，所以这个分支只会执行\n"
        "    // 一次：在这里选定路由，如果是上传，就在任何数据块到达之前把输出文件打开。\n",
    ),
    (
        "                // _response already holds the JSON error. Keep reading and\n"
        "                // discarding until the peer has finished sending, so it can\n"
        "                // actually read the error instead of racing it with a reset.\n",
        "                // _response 里已经装好了 JSON 错误。继续读并扔掉，\n"
        "                // 等对端把数据发完，这样它才读得到错误，\n"
        "                // 而不是和一个 reset 赛跑。\n",
    ),
    (
        "    // How many body bytes landed in the scratch buffer this round. The parser\n"
        "    // advances body().data and decrements body().size as it copies.\n",
        "    // 本轮有多少 body 字节落进了 scratch 缓冲区。解析器一边拷贝，\n"
        "    // 一边推进 body().data 并递减 body().size。\n",
    ),
    (
        "        if (_drain_mode) {\n"
        "            // Request already refused: throw the bytes away.\n"
        "        }\n",
        "        if (_drain_mode) {\n"
        "            // 请求已经被拒：这些字节直接扔掉。\n"
        "        }\n",
    ),
    (
        "            // Defence in depth for chunked uploads that declare no\n"
        "            // Content-Length: the declared check inside PrepareUpload() cannot\n"
        "            // see those, so stop as soon as the cap is actually exceeded.\n",
        "            // 兜底：不带 Content-Length 的 chunked 上传，PrepareUpload() 里\n"
        "            // 那个声明式检查看不到，所以等实际写入量一超上限就立刻掐断。\n",
    ),
    (
        "            // FNV-1a, updated incrementally so the content hash is available\n"
        "            // at the end without ever holding the whole file in memory.\n",
        "            // FNV-1a 增量更新，这样不用把整个文件留在内存里，\n"
        "            // 到最后也能拿到内容哈希。\n",
    ),
    (
        "            // Non-upload routes carry small JSON payloads. The parser limit is\n"
        "            // deliberately loose now, so the tight ceiling lives here instead.\n",
        "            // 非上传路由只承载很小的 JSON 负载。解析器上限现在是故意放宽的，\n"
        "            // 所以紧的闸门改在这里。\n",
    ),
    (
        "    if (_drain_mode) {\n"
        "        // Wait until the peer has sent everything before answering.\n",
        "    if (_drain_mode) {\n"
        "        // 等对端把整个 body 发完再回复。\n",
    ),
    (
        "        // Scratch buffer is full and the chunk was already flushed above:\n"
        "        // re-arm and read the next one.\n",
        "        // scratch 缓冲区满了，而且这一块已经在上面落盘/累积过了：\n"
        "        // 重新装填，读下一块。\n",
    ),
    (
        "void HttpConnection::FinishMessage()\n"
        "{\n"
        "    _last_activity = std::chrono::steady_clock::now();\n"
        "    if (_is_upload) {\n",
        "void HttpConnection::FinishMessage()\n"
        "{\n"
        "    _last_activity = std::chrono::steady_clock::now();\n"
        "    // 上传路由不走 HandleReq()：请求体已经被流式处理完了，\n"
        "    // 这里只负责收尾（定名、写响应）。\n"
        "    if (_is_upload) {\n",
    ),
    (
        "void HttpConnection::HandleReq() {\n"
        "    //set version\n"
        "    _response.version(_parser.get().version());\n"
        "    //set keep alive\n"
        "    _response.keep_alive(false);\n"
        "    //parse the url and params\n"
        "    PreParseGetParam();\n"
        "\n"
        "    //handle get req\n",
        "void HttpConnection::HandleReq() {\n"
        "    //设置版本\n"
        "    _response.version(_parser.get().version());\n"
        "    //设置为短链接\n"
        "    _response.keep_alive(false);\n"
        "    //切分请求的url和参数对\n"
        "    PreParseGetParam();\n"
        "\n"
        "    //处理get请求\n",
    ),
    (
        '        LOG_INFO("handle GET req success");\n',
        '        LOG_INFO("处理 GET 请求成功");\n',
    ),
    (
        "    //handle post req\n",
        "    //处理post请求\n",
    ),
    (
        '        LOG_INFO("handle POST req success");\n',
        '        LOG_INFO("处理 POST 请求成功");\n',
    ),
    (
        "//write the response back to the client asynchronously, then close the send\n"
        "//side of the connection and cancel the deadline\n"
        "void HttpConnection::WriteResponse() {\n",
        "//把处理好的响应异步发回客户端，发完后关闭连接的发送端，并取消超时定时器\n"
        "void HttpConnection::WriteResponse() {\n",
    ),
    (
        "    //set the content length of the HTTP response header\n",
        "    //在 HTTP 响应头里设置 Content-Length\n",
    ),
    (
        '            LOG_INFO("async write HTTP response done");\n'
        "            //only close the send side, keep the connection alive otherwise\n"
        "            self->_socket.shutdown(tcp::socket::shutdown_send, ec);\n"
        "            //cancel the deadline\n"
        "            self->deadline_.cancel();\n",
        '            LOG_INFO("异步写 HTTP 响应完成");\n'
        "            //只关闭发送端,接收方向还开着\n"
        "            self->_socket.shutdown(tcp::socket::shutdown_send, ec);\n"
        "            //取消超时定时器\n"
        "            self->deadline_.cancel();\n",
    ),
    (
        "//hex --> dec\n"
        "unsigned char ToHex(unsigned char x)\n",
        "//十进制-->十六进制\n"
        "unsigned char ToHex(unsigned char x)\n",
    ),
    (
        "//dec --> hex\n"
        "unsigned char FromHex(unsigned char x)\n",
        "//十六进制-->十进制\n"
        "unsigned char FromHex(unsigned char x)\n",
    ),
    (
        "//url encode\n"
        "std::string UrlEncode(const std::string& str)\n",
        "//url编码\n"
        "std::string UrlEncode(const std::string& str)\n",
    ),
    (
        "        //check whether the character is a letter or a digit\n",
        "        //判断是否仅有数字和字母构成\n",
    ),
    (
        "        else if (str[i] == ' ') //space -> +\n",
        "        else if (str[i] == ' ') //为空字符\n",
    ),
    (
        "            //other characters are percent-encoded\n",
        "            //其他字符需要提前加%并且高四位和低四位分别转为16进制\n",
    ),
    (
        "//url decode\n"
        "std::string UrlDecode(const std::string& str)\n",
        "//url解码\n"
        "std::string UrlDecode(const std::string& str)\n",
    ),
    (
        "        //check whether it is a percent-encoded sequence\n",
        "        //遇到%将后面的两个字符从16进制转为char再拼接\n",
    ),
    (
        "        else if (str[i] == '+') //+ -> space\n",
        "        else if (str[i] == '+') //还原+为空\n",
    ),
    (
        "//parse the request URI, splitting the path from the query string into\n"
        "//_get_url and _get_params\n"
        "void HttpConnection::PreParseGetParam() {\n"
        "    //get the URI\n"
        "    auto uri = _parser.get().target();\n"
        "    //find the start of the query string\n"
        "    auto query_pos = uri.find('?');\n",
        "//从 HTTP 请求的 URI 里，切分出路径和查询参数，存到成员变量 _get_url 和 _get_params\n"
        "void HttpConnection::PreParseGetParam() {\n"
        "    // 提取 URI\n"
        "    auto uri = _parser.get().target();\n"
        "    // 查找查询字符串的开始位置（即 '?' 的位置）\n"
        "    auto query_pos = uri.find('?');\n",
    ),
    (
        "            key = UrlDecode(pair.substr(0, eq_pos));\n",
        "            key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码\n",
    ),
    (
        "    //handle the last pair, which has no trailing '&'\n",
        "    // 处理最后一个参数对（如果没有 & 分隔符）\n",
    ),
    # ---- log messages: the project keeps log strings in Chinese ----
    (
        '        LOG_ERROR("request body over parser limit, rejected");\n',
        '        LOG_ERROR("请求体超过解析器兜底上限，已拒绝");\n',
    ),
    (
        '        LOG_ERROR("HTTP read error: {}", ec.what());\n',
        '        LOG_ERROR("HTTP 读取错误: {}", ec.what());\n',
    ),
    (
        '                LOG_ERROR("stream upload exceeded {} bytes mid-flight, aborting",\n'
        "                          (long long)kMaxUploadBytes);\n",
        '                LOG_ERROR("流式上传中途超过 {} 字节，中止",\n'
        "                          (long long)kMaxUploadBytes);\n",
    ),
    (
        '                LOG_ERROR("upload write to disk failed");\n',
        '                LOG_ERROR("上传写盘失败");\n',
    ),
    (
        '                LOG_ERROR("non-upload body over {} bytes, rejected",\n'
        "                          kMaxJsonBodyBytes);\n",
        '                LOG_ERROR("非上传路由请求体超过 {} 字节，已拒绝",\n'
        "                          kMaxJsonBodyBytes);\n",
    ),
    (
        '            LOG_INFO("http connection idle for {}s, closing", kIdleTimeoutSec);\n',
        '            LOG_INFO("连接空闲 {} 秒，关闭", kIdleTimeoutSec);\n',
    ),
]

if __name__ == "__main__":
    apply(H_PATH, H_PAIRS)
    apply(C_PATH, C_PAIRS)
    print("all comment translations applied")
