#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Add streaming-upload entry points to LogicSystem.h (GBK, CRLF)."""
PATH = r"D:\QTcode\HChat\GateServer\LogicSystem.h"

OLD = """    void RegGet(std::string url, HttpHandler handler);
    void RegPost(std::string url, HttpHandler handler);
private:"""

NEW = """    void RegGet(std::string url, HttpHandler handler);
    void RegPost(std::string url, HttpHandler handler);

    // Streaming upload. HttpConnection intercepts POST /upload_image and
    // POST /upload_file before HandlePost(), streams the raw request body to
    // disk chunk by chunk, and calls PrepareUpload() once the request header
    // is parsed (open the output file, validate parameters) and FinishUpload()
    // once the whole body has landed (finalise the name, write the response).
    bool PrepareUpload(std::shared_ptr<HttpConnection> con);
    void FinishUpload(std::shared_ptr<HttpConnection> con);

private:"""

raw = open(PATH, "rb").read()
text = raw.decode("gbk").replace("\r\n", "\n")
assert text.count(OLD) == 1, "anchor not found exactly once: %d" % text.count(OLD)
text = text.replace(OLD, NEW)
out = text.replace("\n", "\r\n").encode("gbk")
open(PATH, "wb").write(out)
back = open(PATH, "rb").read().decode("gbk").replace("\r\n", "\n")
assert back == text
print("LogicSystem.h patched:", len(out), "bytes")
print("non-ascii bytes:", sum(1 for b in out if b > 127))
