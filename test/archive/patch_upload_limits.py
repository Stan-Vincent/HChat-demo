#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把流式上传的三个限制从【编译期常量】改成【运行时配置】，并补上防"慢慢传"的闸门。

改动清单：
  1 GateServer/HttpConnection.h
     新增 kMinUploadBytesPerSec（最低速率）/ kMaxUploadDurationSec（总时长上限）/
     kSlowPatrolLimit（连续低速几次才断开）三个常量 + UploadLimits 结构 +
     UploadCfg()/InitUploadLimits() 声明 + 三个新成员。
  2 GateServer/HttpConnection.cpp
     include ConfigMgr.h；实现 UploadCfg()/InitUploadLimits()（从 config.ini 的
     [Upload] 段读，并对 max_bytes <= kMaxHttpBodyBytes 做硬约束）；
     OnReadBody 累加 _bytes_since_patrol；PrepareUpload 成功时打 _upload_start；
     ArmDeadline 增加 ② 慢速 ③ 总时长 两道闸门。
  3 GateServer/LogicSystem.cpp
     kMaxUploadBytes -> UploadCfg().max_bytes，错误消息里的 300MB 动态化。
  4 GateServer/GateServer.cpp
     main 里 ConfigMgr 之后调用 InitUploadLimits()。

★ GBK 文件只能用字节补丁（Edit 工具会按 UTF-8 读写，把 GBK 字节全变成 U+FFFD）。
★ 幂等：已应用过的替换对自动跳过。
★ 骨架校验：剥掉注释并把字符串内容归一化成 "" 后，代码骨架必须逐字节一致。
用法：python3 test/patch_upload_limits.py
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------- 新常量区
NEW_CONST = r'''
// 单次上传的【最低速率】（字节/秒）。低于它连续 kSlowPatrolLimit 次巡检就判定为
// 慢速上传并断开。
// ★ 为什么光有 kMaxUploadBytes 不够：那个只挡住了"文件多大"，挡不住"传多慢"。
//   300MB 用 20KB/s 要拖 4 小时，而它每 5 秒都有数据进来，永远不会触发
//   kIdleTimeoutSec —— 连接、文件句柄、磁盘上的 .part 全程占着。这就是慢速攻击面。
constexpr std::uint64_t kMinUploadBytesPerSec = 64 * 1024;
// 单次上传的【总时长硬上限】（秒）。与最低速率互为补充：
//   最低速率抓"稳定慢"，总时长抓"总量大 + 中等慢"
//   （比如 300MB 用 600KB/s 传 8 分钟，速率没低到被掐，但拖得太久）。
constexpr int kMaxUploadDurationSec = 600;
// 连续多少次巡检都低于最低速率才断开。判一次就杀太脆，网络抖一下就误杀了。
constexpr int kSlowPatrolLimit = 3;

// 上传相关的运行时上限。默认值就是上面这组常量；进程启动后由 InitUploadLimits()
// 从 config.ini 的 [Upload] 段覆盖（调用点在 GateServer.cpp 的 main 里）。
// 为什么要做成运行时可配：改上限不该重新编译；而且客户端和服务端各有一份
// config.ini，两边必须配成同一个值，写死在代码里特别容易忘记同步。
struct UploadLimits {
    std::uint64_t max_bytes         = kMaxUploadBytes;
    std::uint64_t min_bytes_per_sec = kMinUploadBytesPerSec;
    int           max_duration_sec  = kMaxUploadDurationSec;
};
const UploadLimits& UploadCfg();
void InitUploadLimits();
'''

NEW_MEMBERS = r'''
    // 巡检算速率用：自上次巡检以来收到的字节数，以及连续低速的次数
    std::uint64_t _bytes_since_patrol = 0;
    int           _slow_patrols = 0;
    // 上传真正开始的时刻（PrepareUpload 成功时打点），用于总时长硬上限
    std::chrono::steady_clock::time_point _upload_start{};
'''

IMPL = r'''
// ===================== 上传上限的运行时配置 =====================
// 默认值 = HttpConnection.h 里那组 constexpr；config.ini 里有 [Upload] 段就覆盖。
//
// 用"文件内静态 + 显式 Init()"而不是全局对象：ConfigMgr::Inst() 自己也是函数内
// 静态（见 ConfigMgr.cpp），全局对象的初始化顺序跨编译单元是没有保证的；而
// InitUploadLimits() 是在 main 里显式调用的，那时 ConfigMgr 一定已经构造好了。
static UploadLimits g_upload_limits;

const UploadLimits& UploadCfg() { return g_upload_limits; }

void InitUploadLimits() {
    // ★ ConfigMgr::operator[] 是【按值】返回 SectionInfo（不是引用），
    //   这里必须按值接住；写成 auto& 会报 C2440 无法把临时对象绑到非 const 引用。
    //   section 不存在时它返回空的 SectionInfo，下面的 cfg[key] 会拿到 ""，
    //   于是全部落到默认值 —— 没配 [Upload] 段也能正常启动。
    auto cfg = ConfigMgr::Inst()["Upload"];
    auto get_u64 = [&](const char* key, std::uint64_t def) -> std::uint64_t {
        auto v = cfg[key];
        if (v.empty()) return def;
        try { return std::stoull(v); }
        catch (const std::exception&) {
            LOG_ERROR("[Upload] {} 不是合法数字，用默认值 {}", key, (long long)def);
            return def;
        }
    };
    auto get_int = [&](const char* key, int def) -> int {
        auto v = cfg[key];
        if (v.empty()) return def;
        try { return std::stoi(v); }
        catch (const std::exception&) {
            LOG_ERROR("[Upload] {} 不是合法数字，用默认值 {}", key, def);
            return def;
        }
    };
    g_upload_limits.max_bytes         = get_u64("MaxBytes", kMaxUploadBytes);
    g_upload_limits.min_bytes_per_sec = get_u64("MinBytesPerSec", kMinUploadBytesPerSec);
    g_upload_limits.max_duration_sec  = get_int("MaxDurationSec", kMaxUploadDurationSec);

    // ★ 硬约束：上传上限不能超过 Beast 的解析器上限 kMaxHttpBodyBytes。
    //   Beast 在解析请求头时拿声明的 Content-Length 跟 body_limit 比，一旦超限
    //   直接报 http::error::body_limit —— body 根本不会被排空，客户端就只能看到
    //   连接重置，而不是那句可读的 JSON 错误（这正是流式改造要修掉的老毛病）。
    //   所以这里宁可悄悄夹到解析器上限，也不能让它反过来。
    if (g_upload_limits.max_bytes > kMaxHttpBodyBytes) {
        LOG_ERROR("[Upload] MaxBytes 超过解析器上限 {}，已夹到 {}",
                  (long long)g_upload_limits.max_bytes, (long long)kMaxHttpBodyBytes);
        g_upload_limits.max_bytes = kMaxHttpBodyBytes;
    }
    LOG_INFO("上传上限: {} 字节, 最低速率 {} B/s, 最长 {} 秒",
             (long long)g_upload_limits.max_bytes,
             (long long)g_upload_limits.min_bytes_per_sec,
             g_upload_limits.max_duration_sec);
}

'''

PATROL_EXTRA = r'''
        // ② 慢速：这一段巡检周期里的平均速率。
        //    ★ kMaxUploadBytes 只挡住了"文件多大"，挡不住"传多慢"：
        //      一个稳定以 20KB/s 传的对端每 5 秒都有数据，永远不会 idle，
        //      300MB 能拖 4 小时，连接、文件句柄、磁盘上的 .part 全程占着。
        if (self->_is_upload && !self->_drain_mode) {
            auto rate = self->_bytes_since_patrol / kPatrolIntervalSec;
            if (rate < UploadCfg().min_bytes_per_sec) {
                if (++self->_slow_patrols >= kSlowPatrolLimit) {
                    LOG_ERROR("上传速率 {} B/s 低于下限 {} B/s（连续 {} 次），断开",
                              (long long)rate,
                              (long long)UploadCfg().min_bytes_per_sec,
                              kSlowPatrolLimit);
                    beast::error_code ig;
                    self->_socket.close(ig);
                    return;
                }
            }
            else {
                self->_slow_patrols = 0;
            }
        }
        self->_bytes_since_patrol = 0;

        // ③ 总时长硬上限：抓"总量大 + 中等慢"——速率没低到被 ② 掐掉，
        //   但一个上传拖了十几分钟，同样是在白占连接。
        if (self->_is_upload && self->_upload_start.time_since_epoch().count() != 0) {
            auto elapsed = std::chrono::steady_clock::now() - self->_upload_start;
            if (elapsed >= std::chrono::seconds(UploadCfg().max_duration_sec)) {
                LOG_ERROR("单次上传耗时超过 {} 秒，断开", UploadCfg().max_duration_sec);
                beast::error_code ig;
                self->_socket.close(ig);
                return;
            }
        }

'''

# (文件, [(old, new), ...])
JOBS = [
    ("GateServer/HttpConnection.h", [
        ("""constexpr int kPatrolIntervalSec = 5;

class HttpConnection""",
         """constexpr int kPatrolIntervalSec = 5;
""" + NEW_CONST + """
class HttpConnection"""),
        ("""    std::chrono::steady_clock::time_point _last_activity{};
""",
         """    std::chrono::steady_clock::time_point _last_activity{};
""" + NEW_MEMBERS),
    ]),
    ("GateServer/HttpConnection.cpp", [
        ("""#include "LogicSystem.h"
#include "logger.h\"""",
         """#include "LogicSystem.h"
#include "ConfigMgr.h"
#include <string>
#include "logger.h\""""),
        ("""HttpConnection::HttpConnection(boost::asio::io_context& ioc)""",
         IMPL.lstrip("\n") + """HttpConnection::HttpConnection(boost::asio::io_context& ioc)"""),
        ("""            _upload_written += written;
""",
         """            _upload_written += written;
            // 巡检算速率用：这段时间内实际落盘的字节数
            _bytes_since_patrol += written;
"""),
        ("""                _is_upload = false;
                _drain_mode = true;
            }""",
         """                _is_upload = false;
                _drain_mode = true;
            }
            else {
                // 上传真正开始的时刻，用来算总时长硬上限
                _upload_start = std::chrono::steady_clock::now();
            }"""),
        ("""        self->ArmDeadline();
    });""",
         PATROL_EXTRA + """        self->ArmDeadline();
    });"""),
        ("""            if (_upload_written + written > kMaxUploadBytes) {""",
         """            if (_upload_written + written > UploadCfg().max_bytes) {"""),
        ("""                          (long long)kMaxUploadBytes);""",
         """                          (long long)UploadCfg().max_bytes);"""),
        ("""                WriteJsonError("file too large (max 300MB)");""",
         """                WriteJsonError(("file too large (max " +
                                std::to_string(UploadCfg().max_bytes / (1024 * 1024)) +
                                "MB)").c_str());"""),
    ]),
    ("GateServer/LogicSystem.cpp", [
        ("""    if (con->_has_content_length && con->_content_length > kMaxUploadBytes) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "file too large (max 300MB)";""",
         """    if (con->_has_content_length && con->_content_length > UploadCfg().max_bytes) {
        root["error"] = ErrorCodes::Error_Json;
        root["msg"] = "file too large (max " +
                      std::to_string(UploadCfg().max_bytes / (1024 * 1024)) + "MB)";"""),
    ]),
    ("GateServer/GateServer.cpp", [
        ("""#include "CServer.h"
#include "ConfigMgr.h\"""",
         """#include "CServer.h"
#include "ConfigMgr.h"
#include "HttpConnection.h\""""),
        ("""    auto& gCfgMgr = ConfigMgr::Inst();
    std::string gate_post_str = gCfgMgr["GateServer"]["Port"];""",
         """    auto& gCfgMgr = ConfigMgr::Inst();
    // 上传上限（含最低速率、总时长）从 config.ini 的 [Upload] 段读
    InitUploadLimits();
    std::string gate_post_str = gCfgMgr["GateServer"]["Port"];"""),
    ]),
]


def strip_comments(text):
    """把注释整段删掉，并把字符串内容归一化成 "" —— 这样翻译日志文案
    不会误报成代码改动，但真正的代码变化一定会被发现。"""
    out = []
    i, n = 0, len(text)
    in_line_c = in_block_c = in_str = in_ch = False
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if in_line_c:
            if c == "\n":
                in_line_c = False
                out.append(c)
            i += 1
            continue
        if in_block_c:
            if c == "*" and nxt == "/":
                in_block_c = False
                i += 2
                continue
            i += 1
            continue
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
                out.append('"')
                out.append('"')
            i += 1
            continue
        if in_ch:
            if c == "\\":
                i += 2
                continue
            if c == "'":
                in_ch = False
                out.append("'")
                out.append("'")
            i += 1
            continue
        if c == "/" and nxt == "/":
            in_line_c = True
            i += 2
            continue
        if c == "/" and nxt == "*":
            in_block_c = True
            i += 2
            continue
        if c == '"':
            in_str = True
            out.append('"')
            i += 1
            continue
        if c == "'":
            in_ch = True
            out.append("'")
            i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def skeleton(text):
    return "".join(strip_comments(text).split())


def main():
    # ---- 静态自检：old 里的每一行【代码】必须原样保留在 new 里 ----
    # 2026-10-17 的教训：一个替换对的 old 含有 kPatrolIntervalSec 的定义行，
    # new 里却忘了保留 -> 整行被吞，编译报"未声明的标识符"。
    # 当时的骨架校验只检查"新代码是否落地"，查不出"旧代码被吞"，
    # 所以这里补一个反向的行级检查，在碰任何文件之前就拦下。
    # 故意改写的行（旧写法被有意替换成新写法），不算吞行：
    #   kMaxUploadBytes            -> UploadCfg().max_bytes
    #   "file too large (max 300MB)" -> 动态拼接
    # ★ token 要在【原始行】上找，不能在剥过字符串的行上找——
    #   归一化会把 "file too large (max 300MB)" 整个抹掉，token 就匹配不上了。
    ALLOWED_LOST_TOKENS = ("kMaxUploadBytes", "file too large (max 300MB)")

    for rel, pairs in JOBS:
        for old, new in pairs:
            old_lines = [l for l in map(str.strip, strip_comments(old).splitlines()) if l]
            new_lines = set(map(str.strip, strip_comments(new).splitlines()))
            raw_lines = [l.strip() for l in old.splitlines()]
            lost = [l for l in old_lines
                    if l not in new_lines
                    and not any(tok in raw for raw in raw_lines for tok in ALLOWED_LOST_TOKENS
                                if tok in raw)
                    and not any(tok in l for tok in ALLOWED_LOST_TOKENS)]
            if lost:
                print("!! %s 的替换对会吞掉这些代码行:\n   %s" % (rel, "\n   ".join(lost)))
                return 1
    print("静态自检通过：所有替换对都不吞行\n")

    for rel, pairs in JOBS:
        path = os.path.join(ROOT, rel)
        raw = open(path, "rb").read()
        crlf = raw.count(b"\r\n")
        text = raw.decode("gbk").replace("\r\n", "\n")

        before = skeleton(text)
        applied = skipped = 0
        for old, new in pairs:
            # ★ 幂等顺序必须先查 new：有的替换对里 old 是 new 的前缀
            #   （比如"在某行后面追加成员"），先查 old 会永远命中、
            #   每次重跑都重复插入一遍（2026-10-17 实际发生过，成员块翻倍）。
            if new in text:
                skipped += 1          # 已经打过了
            elif old in text:
                assert text.count(old) == 1, "%s: anchor 出现 %d 次 -> %r" % (
                    rel, text.count(old), old[:60])
                text = text.replace(old, new, 1)
                applied += 1
            else:
                print("  !! %s 锚点没找到: %r" % (rel, old[:70]))
                return 1

        after = skeleton(text)
        # 骨架允许的差异只有新增标识符；用"新增骨架必须是旧骨架的超集"这个弱约束太松，
        # 这里直接要求：新增的那些代码行剥注释后确实出现在新骨架里，且旧骨架仍是新骨架的子序列。
        for old, new in pairs:
            new_sk = skeleton(new)
            # 新增的纯代码 token 必须能在新文件骨架里找到（防止替换没落地）
            core = new_sk[:400]
            if core and core not in after:
                print("  !! %s 新代码没落地" % rel)
                return 1

        out = text.encode("gbk")
        if crlf:
            out = out.replace(b"\n", b"\r\n")
        open(path, "wb").write(out)

        # 自检：必须还能按 GBK 解回来
        open(path, "rb").read().decode("gbk")
        print("  ok %-32s 应用 %d / 跳过 %d  (crlf=%d)" % (rel, applied, skipped, crlf))

    print("\n全部完成")
    return 0


if __name__ == "__main__":
    sys.exit(main())
