// SVG 解码自检：验证当前运行环境 qsvg 插件能否解码 HChat 的线性图标
// 用法: svg_decode_test.exe <file1.svg> [file2.svg ...]
#include <QImageReader>
#include <QImage>
#include <QDebug>
#include <QCoreApplication>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int fail = 0;
    for (int i = 1; i < argc; ++i) {
        QImageReader r(QString::fromLocal8Bit(argv[i]));
        QImage img = r.read();
        bool ok = !img.isNull() && img.size().width() > 0;
        if (!ok) ++fail;
        qDebug() << argv[i]
                 << "canRead=" << r.canRead()
                 << "decoded=" << !img.isNull()
                 << "size=" << img.size()
                 << (ok ? "[OK]" : "[FAIL]");
    }
    return fail; // 退出码=失败文件数
}
