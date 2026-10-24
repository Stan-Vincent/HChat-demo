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

    // 8 色色板：中间饱和度，深浅主题都不违和；seed 决定取哪色
    static const QColor kPalette[] = {
        QColor(0x5B, 0x8F, 0xF9),   // 蓝
        QColor(0x5A, 0xD8, 0xA6),   // 绿
        QColor(0xF6, 0xBD, 0x16),   // 黄
        QColor(0xE8, 0x68, 0x4A),   // 橙红
        QColor(0x6D, 0xC8, 0xEC),   // 青
        QColor(0x92, 0x70, 0xCA),   // 紫
        QColor(0xFF, 0x9D, 0x4D),   // 橙
        QColor(0x26, 0x9A, 0x99),   // teal
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
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, px, px), Qt::AlignCenter, letter);

    return pix;
}

} // namespace AvatarUtil
