#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include "global.h"
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <exception>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
// ★ 必须先定这两个宏再包含 windows.h：
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

int main(int argc, char *argv[])
{
#ifdef _WIN32
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
