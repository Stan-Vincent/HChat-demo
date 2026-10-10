#ifndef PICTUREBUBBLE_H
#define PICTUREBUBBLE_H

#include "BubbleFrame.h"
#include <QHBoxLayout>
#include <QPixmap>
#include <QPointer>
class QLabel;

/**
 * 图片气泡
 *
 * 【为什么要拆成两步】收到的图片消息里只有一个服务端相对路径（如 upload/xxx.png），
 * 图片本身要异步下载。所以构造时先放一个"加载中"的占位label，
 * 下载回调到达后再调 SetPixmap() 填充。
 *
 * 【为什么要缓存】同一个文件可能被多条消息引用（自己发的 + 对方发的 + 历史记录），
 * 每次都重新下会卡住 UI。_pixmap_cache 以 url 为 key 全局共享一份，
 * 命中缓存时直接 SetPixmap，不发网络请求。
 */
class PictureBubble : public BubbleFrame
{
    Q_OBJECT
public:
    // 完整构造：已经有图了（本地选图后立刻显示）
    PictureBubble(const QPixmap &picture, ChatRole role, QWidget *parent = nullptr);

    // 异步构造：先占位，等调用者下完图后 SetPixmap()
    // 如果 url 已经下过，会立刻同步填充，调用方仍可安全地再调一次 SetPixmap()
    explicit PictureBubble(const QString &url, ChatRole role, QWidget *parent = nullptr);

    // 回填图片。会自动按 PIC_MAX_WIDTH/HEIGHT 等比缩放并重设气泡尺寸。
    void SetPixmap(const QPixmap &pix);

    // 全局缓存：key 是完整 url，value 是已下载好的原图
    static QHash<QString, QPixmap> _pixmap_cache;

private:
    void initEmpty(ChatRole role);
    void applyPixmap(const QPixmap &pix);

    QLabel *_lb = nullptr;
};

#endif // PICTUREBUBBLE_H