#include "avatarutil.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>

namespace AvatarUtil {

QPixmap MakeLetterAvatar(const QString& text, int px, int seed)
{
    QPixmap pix(px, px);
    pix.fill(Qt::transparent);

    // 取第一个"字"：中文天然是一个字符；英文/数字取首字母并大写
    QString letter = text.trimmed().isEmpty() ? QStringLiteral("?") : text.trimmed().left(1);
    if (!letter.isEmpty() && letter.at(0).isLetter() && letter.at(0).isLower()) {
        letter = letter.toUpper();
    }

    // 8 色低饱和色板（去"玩具感"，P1 设计系统）；seed 决定取哪色
    static const QColor kPalette[] = {
        QColor(0x8A, 0xA1, 0xD1),   // 雾蓝
        QColor(0xA3, 0xB1, 0x8A),   // 苔绿
        QColor(0xD4, 0xA5, 0xA5),   // 藕粉
        QColor(0xB5, 0xA8, 0xC9),   // 薰衣草
        QColor(0xC9, 0xAD, 0xA7),   // 奶茶
        QColor(0x9F, 0xB8, 0xB2),   // 青灰
        QColor(0xCB, 0xB8, 0x9D),   // 沙金
        QColor(0xA0, 0xA8, 0xB0),   // 石板灰
    };
    const int count = int(sizeof(kPalette) / sizeof(kPalette[0]));
    int idx = (seed < 0 ? -seed : seed) % count;
    const QColor& bg = kPalette[idx];

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 圆角方块底色
    const int radius = qMax(2, px / 5);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, px, px), radius, radius);
    painter.fillPath(path, bg);

    // 居中白字，字号随边长缩放
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(qMax(8, int(px * 0.46)));
    painter.setFont(font);
    painter.setPen(QColor(0x2A, 0x2E, 0x35));  // 深墨字：低饱和浅底上白字对比不足
    painter.drawText(QRectF(0, 0, px, px), Qt::AlignCenter, letter);

    return pix;
}

} // namespace AvatarUtil
