#ifndef AVATARUTIL_H
#define AVATARUTIL_H

#include <QPixmap>
#include <QString>

// 默认头像工具：给"没有头像 URL"的会话（群聊、坏数据会话）画一个字母头像。
// 取显示名的第一个字符（中文取首字、英文取首字母），配一块按 seed 选色的圆角底色。
// 微信/QQ/飞书的群头像都是这个路子：零资源文件、每个群还能区分。
namespace AvatarUtil {

// px: 目标边长（正方形）；seed: 选色种子，传 thread_id 之类稳定值即可
QPixmap MakeLetterAvatar(const QString& text, int px, int seed = 0);

} // namespace AvatarUtil

#endif // AVATARUTIL_H
