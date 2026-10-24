#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Rewrite GateServer/HttpConnection.h for streaming uploads.

GBK encoded, CRLF line endings. Comments must stay ASCII (GBK round-trip).
"""
import io, sys

PATH = r"D:\QTcode\HChat\GateServer\HttpConnection.h"

NEW = """#pragma once
#include "const.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// ---- streaming upload tuning (see HttpConnection.cpp for the rationale) ----
// Chunk the request body is copied into before it hits the disk.
constexpr std::size_t kBodyChunkSize = 64 * 1024;
// Hard cap on the declared Content-Length. Beast compares it against
// Content-Length while parsing the REQUEST HEADER, and it is a decrementing
// budget, so it must be armed before async_read and cover the largest body
// we accept. Beast's default for requests is only 1MB.
constexpr std::uint64_t kMaxHttpBodyBytes = 320ull * 1024 * 1024;
// A connection that produced no activity for this long is considered dead.
constexpr int kIdleTimeoutSec = 60;
// How often the idle patrol re-checks the connection.
constexpr int kPatrolIntervalSec = 5;

class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
public:
    friend class LogicSystem;
	HttpConnection(boost::asio::io_context& ioc);
	~HttpConnection();
	void Start();
    tcp::socket& GetSocket();

    // ---- streaming upload state, shared with LogicSystem ----
    // Filled by HttpConnection once the request header is parsed; read by
    // LogicSystem::PrepareUpload() and consumed by LogicSystem::FinishUpload().
    bool          _is_upload = false;   // true for POST /upload_image|/upload_file
    bool          _upload_finished = false;
    bool          _has_content_length = false;
    std::uint64_t _content_length = 0;
    std::string   _content_type;
    std::string   _upload_file_name;    // original name from the query string
    std::string   _upload_ext;          // sanitized extension
    std::string   _upload_unique_name;  // final name on disk
    std::string   _upload_path;         // current path on disk (.part or final)
    std::ofstream _upload_ofs;          // streamed output file
    std::uint64_t _upload_written = 0;  // bytes written so far
    // FNV-1a running state, updated per chunk so the content hash is known
    // without ever holding the whole file in memory.
    unsigned long long _upload_hash = 1469598103934665603ULL;

    // Accumulated body for non-upload routes (small JSON payloads).
    // Replaces buffers_to_string(_request.body().data()).
    std::string _body_str;

private:
    // Periodic idle patrol. A one-shot deadline armed at accept time kills
    // slow-but-progressing uploads, so we re-check on an interval instead.
    void ArmDeadline();

    // Serialize _response and shut the connection down.
    void WriteResponse();

    // Parse the request URI into _get_url / _get_params.
    void PreParseGetParam();

    // Dispatch a fully received message to LogicSystem.
    void HandleReq();

    // Streaming read loop.
    void DoReadBody();
    void OnReadBody(beast::error_code ec, std::size_t bytes_transferred);
    void FinishMessage();

    tcp::socket _socket;

    //_buffer is the read buffer
    beast::flat_buffer  _buffer{ 8192 };

    // buffer_body lets us consume the request body incrementally instead of
    // buffering the whole payload in RAM: body().data / body().size must be
    // re-armed before every async_read, and the parser reports need_buffer
    // once the destination is full.
    http::request_parser<http::buffer_body> _parser;

    // _response is the response sent back to the client, filled in HandleReq()
    http::response<http::dynamic_body> _response;

    //_deadline is the idle patrol timer
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(kPatrolIntervalSec)
    };
    std::chrono::steady_clock::time_point _last_activity{};

    // Destination the parser copies body bytes into.
    std::vector<char> _scratch;
    bool _header_done = false;

    //get url
	std::string _get_url;

    // Raw bytes of a static resource served by GET /upload/<name>. Kept in
    // memory here because beast::buffers_body would point into storage that
    // does not outlive the handler.
    std::string _file_body;

    //get params of the url, structure: key->value
    std::unordered_map<std::string, std::string> _get_params;
};
"""

data = NEW.replace("\n", "\r\n").encode("gbk")
with open(PATH, "wb") as f:
    f.write(data)
print("HttpConnection.h rewritten:", len(data), "bytes")
# verify round trip
back = open(PATH, "rb").read().decode("gbk")
assert back.replace("\r\n", "\n") == NEW, "round trip mismatch"
print("GBK round-trip OK")
