#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include "global.h"
#include <QMessageBox>
#include <QDebug>
#include <QIcon>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QTextStream>
#include <exception>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include "logcategory.h"

#ifdef _WIN32
//   ★必须先定这两个宏再包含 windows.h：
//   windows.h 会连带引入 rpcndr.h，里面 `typedef byte cs_byte;`
//   与 QtCore 的 byte 撞名，报 "reference to 'byte' is ambiguous"。
//   WIN32_LEAN_AND_MEAN 让它跳过 RPC/ Winsock 那一堆用不上的头。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#endif

// ============================================================
//  崩溃日志：Qt 里空指针/越界是"静默崩溃"（双击运行时控制台窗口
//  会随进程一起消失，什么线索都留不下）。这里把崩溃信息落盘到
//  exe 同级的 crash.log，下次闪退后就能看到出错地址。
// ============================================================
static void writeCrashLog(const QString& text)
{
    QDir dir(QCoreApplication::applicationDirPath());
    QFile f(dir.filePath("crash.log"));

    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        f.write("\n===== crash @ ");
        f.write(QDateTime::currentDateTime().toString(Qt::ISODate).toLocal8Bit());
        f.write(" =====\n");
        f.write(text.toLocal8Bit());
        f.close();
    }
}

#ifdef _WIN32
// 抓调用栈：Qt 的空指针/越界多数走 SEH，但栈溢出、abort() 这类不一定
// 会走到 UnhandledExceptionFilter，所以再补一个 POSIX 信号处理器兜底。
static void dumpStack(FILE* fp)
{
#ifdef _WIN32
    HANDLE hProcess = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(hProcess, NULL, TRUE);

    void* frames[64];
    int n = CaptureStackBackTrace(0, 64, frames, NULL);
    fprintf(fp, "call stack (%d frames):\n", n);
    for (int k = 0; k < n; ++k) {
        DWORD64 disp = 0;
        SYMBOL_INFOW sym = { 0 };
        sym.SizeOfStruct = sizeof(SYMBOL_INFOW);
        sym.MaxNameLen = MAX_SYM_NAME;
        if (SymFromAddrW(hProcess, (DWORD64)frames[k], &disp, &sym) == TRUE) {
            char name[512] = { 0 };
            wcstombs(name, sym.Name, sizeof(name) - 1);
            fprintf(fp, "  #%02d %p  %s + 0x%llX\n", k, frames[k], name,
                (unsigned long long)disp);
        } else {
            fprintf(fp, "  #%02d %p  <no symbol>\n", k, frames[k]);
        }
    }
    SymCleanup(hProcess);
    fflush(fp);
#else
    (void)fp;
#endif
}

static void signalHandler(int sig)
{
    FILE* fp = fopen("crash.log", "a");
    if (fp) {
        fprintf(fp, "\n===== crash (signal %d) =====\n", sig);
        dumpStack(fp);
        fclose(fp);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

// ★★ 这里原来写成了 `static LONG WINAPIUnhandledExceptionFilter(...)`：
//   WINAPI 和函数名之间少了空格，结果函数名变成了 "WINAPIUnhandledExceptionFilter"
//   （WINAPI 是宏，展开成 __stdcall，本该作为调用约定前缀）。
//   于是下面 SetUnhandledExceptionFilter(UnhandledExceptionFilter) 里的
//   UnhandledExceptionFilter 解析到的是 **Windows API 自己**，不是这个处理器 ——
//   等于往系统里注册了系统默认过滤器，抓崩溃的逻辑完全没生效。
//   编译器给的线索是 warning: ... defined but not used（本函数从未被引用）。
static LONG WINAPI MyUnhandledExceptionFilter(EXCEPTION_POINTERS* ep)
{

    HANDLE h = CreateFileA("crash.log", GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char head[128];
        int n = _snprintf(head, sizeof(head),
            "\n===== crash @ %s =====\nexception code: 0x%llX\n崩溃地址: %llX\n",
            qPrintable(QDateTime::currentDateTime().toString(Qt::ISODate)),
            (unsigned long long)ep->ExceptionRecord->ExceptionCode,
            (unsigned long long)(size_t)ep->ExceptionRecord->ExceptionAddress);
        DWORD w = 0;
        WriteFile(h, head, n, &w, nullptr);
        CloseHandle(h);
    }
    // 追加调用栈
    FILE* fp = fopen("crash.log", "a");
    if (fp) { dumpStack(fp); fclose(fp); }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

// ============================================================
//  运行日志落盘：Hcat.exe 是 -subsystem,windows 的 GUI 程序，
//  没有控制台，qDebug/qCDebug 的输出重定向不到文件里（以前排查问题
//  只能上 gdb）。这里装一个 message handler，把所有日志同时写到
//  exe 同级的 client.log（带时间戳 + 分类），超过 2MB 自动截断重写。
//  用法：正常跑就有；想看实时可以 Get-Content client.log -Wait
// ============================================================
static QFile* g_logFile = nullptr;
static QtMessageHandler g_prevHandler = nullptr;

static void logToFile(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (g_logFile && g_logFile->isOpen()) {
        QTextStream ts(g_logFile);
        ts << QDateTime::currentDateTime().toString(QString("yyyy-MM-dd hh:mm:ss.zzz"))
           << " [" << (ctx.category ? ctx.category : QString("default")) << "] "
           << msg << "\n";
        g_logFile->flush();     // 崩溃时也要能留下最后几行
    }
    if (g_prevHandler) {
        g_prevHandler(type, ctx, msg);
    }
}

static void openClientLog()
{
    QDir dir(QCoreApplication::applicationDirPath());
    const QString path = dir.filePath(QString("client.log"));
    // 超过 2MB 就重开一个，防止无限增长
    if (QFile::exists(path) && QFileInfo(path).size() > 2 * 1024 * 1024) {
        QFile::remove(path);
    }
    g_logFile = new QFile(path);
    if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
    }
}

int main(int argc, char *argv[])
{
//双保险
#ifdef _WIN32
    //SetUnhandledExceptionFilter 能捕获绝大多数 Windows 上的访问违例（空指针、越界）
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE,  signalHandler);
    signal(SIGILL,  signalHandler);
#endif
    // 让 C++ 异常（std::bad_function_call 等）也能留下记录
    std::set_terminate([]() {
        writeCrashLog(QString("std::terminate —— 未捕获的 C++ 异常"));
        abort();
    });

    QApplication a(argc, argv);

    // 初始化客户端日志分类（hcat.network / chat / ui / user / avatar）。
    // 必须在 QApplication 之后，因为 QLoggingCategory 依赖 Qt 的日志系统。
    // 运行时控制示例：$env:QT_LOGGING_RULES="hcat.network.debug=true"
    init_log_categories();

    // 日志落盘（见文件顶部 logToFile 的注释）
    openClientLog();
    g_prevHandler = qInstallMessageHandler(logToFile);

    // 应用图标：所有窗口标题栏 + 任务栏统一显示这只猫。
    // 用 512 的 PNG（qrc 里），比 exe 内嵌的 .ico（上限256）更清晰；
    // Qt 会按窗口/任务栏实际需要自动缩放。
    a.setWindowIcon(QIcon(":/res/Icon.png"));

    QFile qss(":/style/stylesheet.qss");

    if( qss.open(QFile::ReadOnly))
    {
        qDebug("main.cpp [Qss open success]");
        QString style = QLatin1String(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }
    else{
        QMessageBox::critical(nullptr,"错误","样式表加载错误",QMessageBox::Ok);
    }


    // 获取当前应用程序的路径
    QString app_path = QCoreApplication::applicationDirPath();
    // 拼接文件名，取得config.ini的内容
    QString fileName = "config.ini";
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + fileName);

    qDebug()<<config_path;

    QSettings settings(config_path, QSettings::IniFormat);
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    //获取GateServer的网络地址和端口
    gate_url_prefix = "http://"+gate_host+":"+gate_port;

    MainWindow w;
    w.show();
    return a.exec();
}
