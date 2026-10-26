// SVG vs PNG 对照探针 v5：grab() + 红底反衬
#include <QApplication>
#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <cstdio>

static int countIconPx(const QImage& img) {
    int n = 0; // 图标浅灰 #D5D8E0，底是纯红，不会再混淆
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            QColor c = img.pixelColor(x, y);
            if (c.alpha() > 40 && qAbs(c.red()-213)<50 && qAbs(c.green()-216)<50 && qAbs(c.blue()-224)<50) ++n;
        }
    return n;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    struct { const char* name; const char* qss; } cases[] = {
        {"1 红底sanity       ", "#w{background-color:#FF0000;}"},
        {"2 红底+border-image png", "#w{background-color:#FF0000; border-image: url(:/t/search.png);}"},
        {"3 红底+border-image svg", "#w{background-color:#FF0000; border-image: url(:/t/chat_icon.svg);}"},
        {"4 红底+image svg       ", "#w{background-color:#FF0000; image: url(:/t/chat_icon.svg);}"},
        {"5 红底+background-image svg", "#w{background-color:#FF0000; background-image: url(:/t/chat_icon.svg);}"},
    };
    for (auto& c : cases) {
        QFrame w; w.setObjectName("w"); w.setFixedSize(24,24);
        w.setStyleSheet(c.qss);
        QImage img = w.grab().toImage();
        int n = countIconPx(img);
        std::printf("%s: icon_px=%d %s\n", c.name, n, n>0?"[ICON DRAWN]":"[BLANK]");
    }
    std::fflush(stdout);
    return 0;
}
