// Qt 控制台探针：链接【真实】的 Client/uploadmanager.cpp + httpmgr.cpp + global.cpp，
// 打到 mock_gate.exe 上，验证流式上传的客户端↔服务端契约。
//
// 验证点：
//   1 小文件上传成功，服务端落盘内容与本地逐字节一致
//   2 50MB 文件上传成功，并测客户端进程峰值内存（流式改造的核心收益）
//   3 两路并发上传，各自的回调拿到各自的 url（不串包）
//   4 文件不存在 / 空文件 -> 客户端侧直接失败，不发包
//   5 服务端拒收（mock 把上限压到 1MB，客户端发 2MB）-> 客户端能读到服务端 msg
//   6 进度回调 total 是真实大小，且能到 100%
//
// 编译见 build_probe.sh
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QAtomicInt>

#include <windows.h>
#include <psapi.h>

#include <iostream>
#include <string>

#include "global.h"
#include "uploadmanager.h"
#include "logcategory.h"

static int g_pass = 0, g_fail = 0;

static void record(const std::string& name, bool ok, const std::string& detail)
{
    if (ok) ++g_pass; else ++g_fail;
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << name << "  " << detail << std::endl;
}

static SIZE_T peak_kb()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return pmc.PeakWorkingSetSize / 1024;
    return 0;
}

static SIZE_T ws_kb()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return pmc.WorkingSetSize / 1024;
    return 0;
}

static std::string qs(const QString& s) { return s.toStdString(); }

// 生成一个确定内容的大文件（避免真随机 50MB 的开销，且便于比对）
static bool make_file(const QString& path, qint64 size, unsigned char seed)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QByteArray chunk(1024 * 1024, '\0');
    for (int i = 0; i < chunk.size(); ++i)
        chunk[i] = static_cast<char>((i * 31 + seed) & 0xFF);
    qint64 left = size;
    while (left > 0) {
        qint64 n = qMin<qint64>(left, chunk.size());
        if (f.write(chunk.constData(), n) != n) return false;
        left -= n;
    }
    return true;
}

static bool same_file(const QString& a, const QString& b)
{
    QFile fa(a), fb(b);
    if (!fa.open(QIODevice::ReadOnly) || !fb.open(QIODevice::ReadOnly)) return false;
    if (fa.size() != fb.size()) return false;
    const qint64 chunk = 1024 * 1024;
    QByteArray ba, bb;
    while (!fa.atEnd()) {
        ba = fa.read(chunk); bb = fb.read(chunk);
        if (ba != bb) return false;
    }
    return true;
}

struct Ctx {
    QEventLoop loop;
    QAtomicInt pending{ 0 };
};

// 一次上传，同步等待回调
struct Outcome {
    bool ok = false;
    QString url;
    QString err;
    qint64 lastSent = -1;
    qint64 lastTotal = -1;
    int progressTicks = 0;
};

static Outcome run_upload(int uid, const QString& localPath, Ctx& ctx, bool withProgress = true)
{
    Outcome out;
    ctx.pending.fetchAndAddRelaxed(1);
    auto mgr = UploadManager::instance();
    auto done = [&](bool ok, const QString& url, const QString& err) {
        out.ok = ok; out.url = url; out.err = err;
        if (ctx.pending.fetchAndSubRelaxed(1) == 1)
            QTimer::singleShot(0, &ctx.loop, &QEventLoop::quit);
    };
    if (withProgress) {
        mgr->uploadFile(uid, localPath, done,
            [&](qint64 sent, qint64 total) {
                out.lastSent = sent; out.lastTotal = total; ++out.progressTicks;
            });
    } else {
        mgr->uploadFile(uid, localPath, done);
    }
    return out;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    init_log_categories();

    int port = (argc > 1) ? atoi(argv[1]) : 18082;
    QString out_dir = (argc > 2) ? QString::fromLocal8Bit(argv[2]) : QString("mock_upload");
    gate_url_prefix = QString("http://127.0.0.1:%1").arg(port);

    // 两轮跑：mock 的上传上限只能是唯一一个值，而"50MB 成功"和"2MB 被拒"
    // 需要两个不同的上限，所以按 argv[3] 分成 main / reject 两轮。
    QString mode = (argc > 3) ? QString::fromLocal8Bit(argv[3]) : QString("main");

    std::cout << "[probe] gate_url_prefix = " << qs(gate_url_prefix)
              << "  upload dir = " << qs(out_dir) << std::endl;

    Ctx ctx;

    if (mode == "main") {

    // ---- 1 小文件 ----
    {
        QString p = "probe_small.bin";
        make_file(p, 10240, 7);
        QString srvPath = out_dir + "/__tmp__";   // 占位，实际用 url 里的名字
        (void)srvPath;
        ctx.pending.fetchAndAddRelaxed(1);
        Outcome o;
        o.ok = false;
        ctx.pending.fetchAndSubRelaxed(1);
        UploadManager::instance()->uploadFile(2, p,
            [&](bool ok, const QString& url, const QString& err) {
                o.ok = ok; o.url = url; o.err = err;
                ctx.loop.quit();
            });
        ctx.loop.exec();
        bool ok = o.ok;
        if (ok) {
            QString srv = out_dir + "/" + o.url.mid(QString("upload/").length());
            ok = same_file(p, srv);
        }
        record("1 小文件上传+逐字节比对", ok, qs(o.url) + " err=" + qs(o.err));
    }

    // ---- 2 50MB + 峰值内存 ----
    {
        QString p = "probe_50m.bin";
        make_file(p, 50 * 1024 * 1024, 13);
        SIZE_T before = peak_kb();
        QVector<QPair<qint64,qint64>> ticks;
        ctx.pending.fetchAndAddRelaxed(1);
        Outcome o;
        UploadManager::instance()->uploadFile(2, p,
            [&](bool ok, const QString& url, const QString& err) {
                o.ok = ok; o.url = url; o.err = err;
                ctx.loop.quit();
            },
            [&](qint64 sent, qint64 total) {
                o.lastSent = sent; o.lastTotal = total;
                if (ticks.size() < 2000) ticks.push_back(qMakePair(sent, total));
            });
        ctx.loop.exec();
        std::cout << "        [progress] ticks=" << ticks.size();
        for (int i = 0; i < (int)ticks.size() && i < 4; ++i)
            std::cout << " (" << ticks[i].first << "/" << ticks[i].second << ")";
        if (ticks.size() > 4) std::cout << " ...";
        for (int i = qMax(0, (int)ticks.size() - 3); i < (int)ticks.size(); ++i)
            std::cout << " (" << ticks[i].first << "/" << ticks[i].second << ")";
        std::cout << std::endl;
        SIZE_T after = peak_kb();
        int zero_ticks = 0;
        for (const auto& t : ticks) if (t.first == 0 && t.second == 0) ++zero_ticks;
        bool ok = o.ok && o.lastTotal == 50 * 1024 * 1024 && o.lastSent == o.lastTotal
                  && zero_ticks == 0;
        if (ok) {
            QString srv = out_dir + "/" + o.url.mid(QString("upload/").length());
            ok = same_file(p, srv);
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "50MB peak=%lluKB(now %lluKB) total=%lld sent=%lld zeroTicks=%d",
                 (unsigned long long)after, (unsigned long long)ws_kb(),
                 (long long)o.lastTotal, (long long)o.lastSent, zero_ticks);
        record("2 50MB 上传+进度+峰值内存", ok, buf);
        Q_UNUSED(before)
        QFile::remove(p);
    }

    // ---- 3 两路并发 ----
    {
        QString p1 = "probe_c1.bin", p2 = "probe_c2.bin";
        make_file(p1, 8 * 1024 * 1024, 21);
        make_file(p2, 5 * 1024 * 1024, 22);
        int left = 2;
        Outcome o1, o2;
        UploadManager::instance()->uploadFile(2, p1,
            [&](bool ok, const QString& url, const QString& err) {
                o1.ok = ok; o1.url = url; o1.err = err;
                if (--left == 0) ctx.loop.quit();
            });
        UploadManager::instance()->uploadFile(2, p2,
            [&](bool ok, const QString& url, const QString& err) {
                o2.ok = ok; o2.url = url; o2.err = err;
                if (--left == 0) ctx.loop.quit();
            });
        ctx.loop.exec();
        bool ok = o1.ok && o2.ok && o1.url != o2.url;
        if (ok) {
            QString s1 = out_dir + "/" + o1.url.mid(QString("upload/").length());
            QString s2 = out_dir + "/" + o2.url.mid(QString("upload/").length());
            ok = same_file(p1, s1) && same_file(p2, s2);
        }
        record("3 两路并发各拿各的 url", ok,
               qs(o1.url) + " | " + qs(o2.url));
        QFile::remove(p1); QFile::remove(p2);
    }

    // ---- 4a 文件不存在 ----
    {
        bool called = false; Outcome o;
        UploadManager::instance()->uploadFile(2, "no_such_file_xyz.bin",
            [&](bool ok, const QString& url, const QString& err) {
                called = true; o.ok = ok; o.err = err;
                QTimer::singleShot(0, &ctx.loop, &QEventLoop::quit);
            });
        // 客户端侧直接失败，回调是同步的；但保险起见也转一下事件循环
        if (!called) ctx.loop.exec();
        bool ok = called && !o.ok && o.err.contains("不存在");
        record("4a 文件不存在：客户端侧直接失败", ok, qs(o.err));
    }

    // ---- 4b 空文件 ----
    {
        QString p = "probe_empty.bin";
        { QFile f(p); f.open(QIODevice::WriteOnly); }
        bool called = false; Outcome o;
        UploadManager::instance()->uploadFile(2, p,
            [&](bool ok, const QString& url, const QString& err) {
                called = true; o.ok = ok; o.err = err;
                QTimer::singleShot(0, &ctx.loop, &QEventLoop::quit);
            });
        if (!called) ctx.loop.exec();
        bool ok = called && !o.ok && o.err.contains("空的");
        record("4b 空文件：客户端侧直接失败", ok, qs(o.err));
        QFile::remove(p);
    }

    // ---- 9 上传上限运行时可配（模拟 main.cpp 从 config.ini 读）----
    {
        qint64 before = UploadManager::MAX_FILE_SIZE;
        UploadManager::setMaxBytes(1 * 1024 * 1024);   // 假装 config.ini 配了 1MB
        qint64 after  = UploadManager::MAX_FILE_SIZE;
        QString p = "probe_cfg.bin";
        make_file(p, 2 * 1024 * 1024, 41);
        bool called = false; Outcome o;
        UploadManager::instance()->uploadFile(2, p,
            [&](bool ok, const QString& url, const QString& err) {
                called = true; o.ok = ok; o.err = err;
                QTimer::singleShot(0, &ctx.loop, &QEventLoop::quit);
            });
        if (!called) ctx.loop.exec();
        UploadManager::setMaxBytes(before);            // 还原
        bool ok = called && !o.ok && o.err.contains("太大")
                  && before == 300 * 1024 * 1024 && after == 1024 * 1024;
        char buf[256];
        snprintf(buf, sizeof(buf), "default=%lldMB after=%lldMB err=%s",
                 (long long)(before / 1024 / 1024), (long long)(after / 1024 / 1024),
                 qs(o.err).c_str());
        record("9 上传上限运行时可配", ok, buf);
        QFile::remove(p);
    }

    }   // end mode == "main"

    if (mode == "reject") {

    // ---- 5 服务端拒收（mock cap=1MB，客户端发 2MB）----
    {
        QString p = "probe_2m.bin";
        make_file(p, 2 * 1024 * 1024, 33);
        Outcome o;
        UploadManager::instance()->uploadFile(2, p,
            [&](bool ok, const QString& url, const QString& err) {
                o.ok = ok; o.url = url; o.err = err;
                ctx.loop.quit();
            });
        ctx.loop.exec();
        bool ok = !o.ok && (o.err.contains("too large") || o.err.contains("300MB"));
        record("5 服务端拒收：客户端读到服务端 msg", ok, qs(o.err));
        QFile::remove(p);
    }

    }   // end mode == "reject"

    std::cout << std::endl << "==== " << g_pass << "/" << (g_pass + g_fail) << " passed ====" << std::endl;
    return g_fail ? 1 : 0;
}
