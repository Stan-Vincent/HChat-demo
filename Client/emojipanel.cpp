#include "emojipanel.h"
#include <QTabWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPalette>
#include <QRegion>
#include <QResizeEvent>
#include <QHideEvent>
#include <QPainterPath>

EmojiPanel::EmojiPanel(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Popup)
{
    // 关键修复：不用 WA_TranslucentBackground + QGraphicsDropShadowEffect。
    // Windows/MinGW 上，半透明 + 图形阴影特效的 Qt::Popup 会让分层窗口的脏区带
    // 负偏移且超出窗口边界，触发 UpdateLayeredWindowIndirect failed（参数错误），
    // 表现为面板渲染异常 / 残影 / 调试台刷屏。改为不透明窗口 + 圆角遮罩，稳定无报错。
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#ffffff"));
    setPalette(pal);
    buildUi();
}

QString EmojiPanel::emo(char32_t cp)
{
    return QString::fromUcs4(&cp, 1);
}

QString EmojiPanel::emo(char32_t cp, char32_t vs)
{
    char32_t seq[2] = { cp, vs };
    return QString::fromUcs4(seq, 2);
}

void EmojiPanel::buildUi()
{
    // 数据：分类名 -> emoji 列表（用 code point，规避源码编码；需彩色的补 VS16）
    QVector<QPair<QString, QVector<QString>>> cats = {
        { "笑脸", {
            emo(0x1F600), emo(0x1F601), emo(0x1F602), emo(0x1F923), emo(0x1F60A), emo(0x1F607),
            emo(0x1F642), emo(0x1F609), emo(0x1F60D), emo(0x1F618), emo(0x1F617), emo(0x1F61A),
            emo(0x1F60B), emo(0x1F61B), emo(0x1F61C), emo(0x1F92A), emo(0x1F61D), emo(0x1F917),
            emo(0x1F914), emo(0x1F610), emo(0x1F634), emo(0x1F60E), emo(0x1F929), emo(0x1F973),
            emo(0x1F60F), emo(0x1F62E), emo(0x1F631), emo(0x1F62D), emo(0x1F605), emo(0x1F606),
            emo(0x1F621), emo(0x1F92C), emo(0x1F480), emo(0x1F47B), emo(0x1F916), emo(0x1F4A9),
        }},
        { "手势", {
            emo(0x1F44D), emo(0x1F44E), emo(0x1F44C), emo(0x1F44F), emo(0x1F64C), emo(0x1F64F),
            emo(0x1F4AA), emo(0x1F44B), emo(0x1F91D), emo(0x1F91E), emo(0x1F919), emo(0x1F448),
            emo(0x1F449), emo(0x1F446), emo(0x1F447), emo(0x1F44A), emo(0x1F918), emo(0x1F91F),
            emo(0x270C, 0xFE0F),
        }},
        { "动物自然", {
            emo(0x1F436), emo(0x1F431), emo(0x1F42D), emo(0x1F439), emo(0x1F430), emo(0x1F98A),
            emo(0x1F43B), emo(0x1F43C), emo(0x1F428), emo(0x1F42F), emo(0x1F981), emo(0x1F42E),
            emo(0x1F437), emo(0x1F438), emo(0x1F435), emo(0x1F414), emo(0x1F427), emo(0x1F426),
            emo(0x1F984), emo(0x1F41D), emo(0x1F98B), emo(0x1F338), emo(0x1F339), emo(0x1F33B),
            emo(0x1F340), emo(0x1F31F), emo(0x2B50), emo(0x1F525), emo(0x1F4A7), emo(0x1F308),
        }},
        { "食物", {
            emo(0x1F34E), emo(0x1F34A), emo(0x1F34B), emo(0x1F349), emo(0x1F347), emo(0x1F353),
            emo(0x1F352), emo(0x1F351), emo(0x1F96D), emo(0x1F34D), emo(0x1F965), emo(0x1F34C),
            emo(0x1F354), emo(0x1F35F), emo(0x1F355), emo(0x1F32D), emo(0x1F37F), emo(0x1F369),
            emo(0x1F36A), emo(0x1F382), emo(0x1F370), emo(0x2615), emo(0x1F375), emo(0x1F37A),
            emo(0x1F377), emo(0x1F964),
        }},
        { "活动", {
            emo(0x26BD), emo(0x1F3C0), emo(0x1F3C8), emo(0x26BE), emo(0x1F3BE), emo(0x1F3D0),
            emo(0x1F3B1), emo(0x1F3D3), emo(0x1F3F8), emo(0x1F3AF), emo(0x1F3B2), emo(0x1F3AE),
            emo(0x1F3B8), emo(0x1F941), emo(0x1F3AC), emo(0x1F389), emo(0x1F38A), emo(0x1F381),
            emo(0x1F3C6), emo(0x1F947), emo(0x1F4AF),
        }},
        { "旅行", {
            emo(0x1F697), emo(0x1F695), emo(0x1F699), emo(0x1F68C), emo(0x1F3CE, 0xFE0F), emo(0x2708, 0xFE0F),
            emo(0x1F680), emo(0x1F681), emo(0x26F5), emo(0x1F6A2), emo(0x1F3E0), emo(0x1F3E2),
            emo(0x1F3EB), emo(0x1F3E5), emo(0x1F3F0), emo(0x1F30D), emo(0x1F5FA, 0xFE0F), emo(0x26F0, 0xFE0F),
            emo(0x1F30B), emo(0x1F303), emo(0x1F3A1), emo(0x1F3A2), emo(0x1F3A0),
        }},
        { "符号", {
            emo(0x2764, 0xFE0F), emo(0x1F9E1), emo(0x1F49B), emo(0x1F49A), emo(0x1F499), emo(0x1F49C),
            emo(0x1F5A4), emo(0x1F494), emo(0x1F495), emo(0x1F496), emo(0x1F497), emo(0x1F493),
            emo(0x1F49E), emo(0x1F498), emo(0x1F49D), emo(0x2705), emo(0x274C), emo(0x2753),
            emo(0x2757), emo(0x26A1), emo(0x1F514), emo(0x1F4A1), emo(0x1F4F1), emo(0x1F4BB),
            emo(0x231A), emo(0x1F4F7), emo(0x1F4DA), emo(0x2709, 0xFE0F), emo(0x1F4CC), emo(0x1F512),
            emo(0x1F511), emo(0x1F3B5), emo(0x1F4B0), emo(0x1F48E), emo(0x1F6D2), emo(0x260E, 0xFE0F),
        }},
    };

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(2, 2, 2, 2);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("emoji_tabs");
    tabs->setStyleSheet(R"(
        QTabWidget::pane { border: none; background: #ffffff; border-radius: 10px; }
        QTabBar::tab {
            height: 26px; min-width: 46px; padding: 0 4px;
            background: #f3f3f3; color: #666;
            border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px;
        }
        QTabBar::tab:selected { background: #ffffff; color: #ff7a45; font-weight: bold; }
        QTabBar::tab:hover { color: #ff7a45; }
    )");
    root->addWidget(tabs);

    for (auto &cat : cats) {
        tabs->addTab(buildCategoryPage(cat.second), cat.first);
    }
    setFixedSize(384, 300);
}

QWidget* EmojiPanel::buildCategoryPage(const QVector<QString> &emojis)
{
    auto *page = new QWidget;
    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea{border:none;background:#ffffff;}");
    auto *gridWidget = new QWidget;
    auto *grid = new QGridLayout(gridWidget);
    grid->setSpacing(4);
    grid->setContentsMargins(6, 6, 6, 6);
    const int cols = 8;
    for (int i = 0; i < emojis.size(); ++i) {
        auto *btn = new QPushButton(emojis[i], gridWidget);
        btn->setFixedSize(36, 36);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFont(QFont("Segoe UI Emoji", 18));
        btn->setStyleSheet("QPushButton{border:none;border-radius:6px;background:#ffffff;}"
                           "QPushButton:hover{background:#fff0e9;}");
        connect(btn, &QPushButton::clicked, this, [this, e = emojis[i]]() {
            emit sig_emoji_selected(e);
        });
        grid->addWidget(btn, i / cols, i % cols);
    }
    scroll->setWidget(gridWidget);
    auto *pl = new QVBoxLayout(page);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->addWidget(scroll);
    return page;
}

void EmojiPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);
    p.fillPath(path, QColor("#ffffff"));
    // 1px 浅灰细边，替代原先的投影，弱化边缘
    p.setPen(QPen(QColor("#e3e3e3"), 1));
    p.drawPath(path);
}

void EmojiPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 不透明窗口用圆角遮罩裁出圆角（不走分层窗口，规避 UpdateLayeredWindow 报错）
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void EmojiPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit sig_panel_hidden();
}
