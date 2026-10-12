//==============================================================
// Hcat unified logging facade (based on spdlog, header-only)
//
// IMPORTANT: all comments here are ASCII on purpose.
//   Backend sources (GateServer/, ChatServer/...) are GBK-encoded.
//   A UTF-8 file with CJK comments breaks compilation when MSVC uses
//   codepage 936 without the /utf-8 flag. Keep this file ASCII-safe.
//
// Usage:
//   1) once at startup (main):
//          hcat::log::init("gate");
//   2) anywhere:
//          LOG_INFO("user {} login ok, uid={}", name, uid);
//          LOG_WARN("redis {} timeout, retry={}", host, n);
//          LOG_ERROR("db query failed: {}", ex.what());
//
// Design notes:
//   * header-only: spdlog bundles fmt, so no extra .lib to link.
//     This deliberately avoids the MSVC /MD vs /MDd + Release-only-DLL
//     pitfalls this project already hit with MySQL Connector/C++.
//   * safe to call before init(): falls back to console-only logger.
//   * level via env var HCHAT_LOG_LEVEL (trace/debug/info/warn/error/off).
//   * file sink: logs/<module>_<YYYY-MM-DD>.log, daily rotation, keep 7.
//==============================================================
#pragma once

// ------------------------------------------------------------
// Must be defined BEFORE including spdlog.
// MSVC defaults to codepage 936 (GBK), so fmt's UTF-8 probe fails and
// trips: static_assert(!FMT_UNICODE || use_utf8, "requires /utf-8").
// Adding /utf-8 globally would make MSVC parse GBK sources as UTF-8 and
// emit C4828 warnings on every CJK comment.
// Disabling fmt's Unicode feature lets GBK bytes pass through verbatim,
// so Chinese still shows up correctly in the log.
// ------------------------------------------------------------
#define FMT_UNICODE 0

#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>

#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>

namespace hcat {
namespace log {

inline constexpr int kRotateHour   = 0;
inline constexpr int kRotateMinute = 0;
inline constexpr int kMaxFiles     = 7;

inline std::shared_ptr<spdlog::logger>& logger() {
    static std::shared_ptr<spdlog::logger> inst;
    return inst;
}

inline spdlog::level::level_enum parse_level() {
    const char* env = std::getenv("HCHAT_LOG_LEVEL");
    if (!env) return spdlog::level::info;
    std::string s(env);
    for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (s == "trace") return spdlog::level::trace;
    if (s == "debug") return spdlog::level::debug;
    if (s == "warn")  return spdlog::level::warn;
    if (s == "error") return spdlog::level::err;
    if (s == "off")   return spdlog::level::off;
    return spdlog::level::info;
}

// Init: colored console + daily rotating file. Idempotent.
inline void init(const std::string& module, const std::string& file_dir = "logs") {
    if (logger()) return;

    // spdlog 1.15 has no sink_list.h; build the logger from a vector.
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    try {
        // Use the *_format_sink variant: its base_filename IS a strftime
        // pattern, producing logs/<module>_<date>.log.
        // Do NOT use daily_file_sink_mt: it treats the string as a directory
        // and appends the date again, yielding xxx_2026-10-11_2026-10-11.
        std::string dir_s = file_dir;
        if (!dir_s.empty() && dir_s.back() != '/' && dir_s.back() != '\\') {
            dir_s += "/";
        }
        sinks.push_back(std::make_shared<spdlog::sinks::daily_file_format_sink_mt>(
            dir_s + module + "_%Y-%m-%d.log",
            kRotateHour, kRotateMinute, false, kMaxFiles));
    } catch (const std::exception& e) {
        // Never let a bad log dir stop the service from starting.
        spdlog::warn("open log dir '{}' failed: {}, console only", file_dir, e.what());
    }

    auto lg = std::make_shared<spdlog::logger>(module, sinks.begin(), sinks.end());
    lg->set_level(parse_level());
    // Flush on warn+: info may be buffered and lost on crash,
    // but warn/error always hit disk. Best trade-off for debugging.
    lg->flush_on(spdlog::level::warn);
    lg->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    spdlog::set_default_logger(lg);
    logger() = lg;
}

// Fallback logger so calling before init() is safe.
inline std::shared_ptr<spdlog::logger> get() {
    if (!logger()) {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
        auto lb = std::make_shared<spdlog::logger>("hcat", sink);
        lb->set_level(spdlog::level::warn);
        spdlog::set_default_logger(lb);
        logger() = lb;
    }
    return logger();
}

inline void shutdown() {
    if (logger()) {
        spdlog::shutdown();
        logger().reset();
    }
}

}  // namespace log
}  // namespace hcat

// Call the logger member functions directly instead of SPDLOG_LOGGER_XX.
// The official macros expand SPDLOG_SOURCE_LOCATION / SPDLOG_FUNCTION, which
// under MSVC degrade to "only logger + level passed" and fail with
// "cannot convert source_loc to level_enum". Member calls behave the same.
#define LOG_TRACE hcat::log::get()->trace
#define LOG_DEBUG hcat::log::get()->debug
#define LOG_INFO  hcat::log::get()->info
#define LOG_WARN  hcat::log::get()->warn
#define LOG_ERROR hcat::log::get()->error