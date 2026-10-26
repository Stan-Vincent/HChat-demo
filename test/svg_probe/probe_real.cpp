// 真实 rc.qrc + 真实 stylesheet.qss 复刻探针 v2（深色宿主防取色干扰）
#include <QApplication>
#include <QLabel>
#include <QFrame>
#include <QImage>
#include <QFile>
#include <cstdio>

static int countIconPx(const QImage& img) {
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            QColor c = img.pixelColor(x, y);
            if (c.alpha() > 40 && qAbs(c.red()-213)<40 && qAbs(c.green()-216)<40 && qAbs(c.blue()-224)<40) ++n;
        }
    return n;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QFile qss("Client/style/stylesheet.qss");
    if (!qss.open(QFile::ReadOnly)) { std::printf("QSS open FAIL\n"); return 2; }
    app.setStyleSheet(QLatin1String(qss.readAll()));
    qss.close();

    // 深色宿主模拟侧栏底色，图标浅灰才可分辨
    struct { const char* name; const char* obj; const char* state; } cases[] = {
        {"侧栏聊天图标", "side_chat_lb", "normal"},
        {"搜索图标",     "search_edit",  "normal"},
    };
    for (auto& c : cases) {
        QFrame host; host.setObjectName("probehost");
        host.setFixedSize(40, 40);
        host.setStyleSheet("#probehost{background-color:#2A2E35;}");
        QLabel* lb = new QLabel(&host);
        lb->setObjectName(c.obj);
        lb->setProperty("state", c.state);
        lb->setGeometry(0, 0, 40, 40);
        QImage img = host.grab().toImage();
        int n = countIconPx(img);
        std::printf("%s: icon_px=%d %s\n", c.name, n, n>0?"[DRAWN]":"[BLANK]");
    }
    std::fflush(stdout);
    return 0;
}
