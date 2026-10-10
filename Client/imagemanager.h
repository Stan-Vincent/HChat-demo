#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QHash>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * 图片下载管理器
 *
 * 消息里的 content 只是服务端相对路径（如 "upload/1_abc.png"），
 * 真正显示前要向 GateServer 的 GET /upload/<file> 把图拉下来。
 *
 * 【为什么不复用 HttpMgr】HttpMgr 的回包统一按 QString 走JSON 解析，
 * 而图片回包是二进制。塞进去会被 UTF-8 编解码破坏。所以这里自带一个
 * 独立的 QNetworkAccessManager，按 reply 对象做多路分发。
 *
 * 【为什么按 reply 分发】同一个 url 可能同时被多个气泡请求。
 * 用 lambda 捕获 QNetworkReply*，finished 时直接查自己那份，
 * 天然做到"谁请求谁收"，不需要维护 pending 表。
 *
 * 用法：
 *   ImageManager::instance()->downloadImage("upload/1_abc.png",
 *       [](bool ok, const QPixmap& pix, const QString& err) { ... });
 */
class ImageManager : public QObject
{
    Q_OBJECT
public:
    static std::shared_ptr<ImageManager> instance();
    ImageManager() = default;

    // 把服务端相对路径补成完整 URL。
    // 已经带http:// 或 https:// 的原样返回。
    static QString toFullUrl(const QString &serverPath);

    // 异步下载。cb 在主线程被调用（QNetworkAccessManager 本身就在主线程）。
    // url 可以是相对路径，也可以是完整 URL。
    void downloadImage(const QString &url,
                       std::function<void(bool ok, const QPixmap &pix, const QString &errMsg)> cb);

    // 磁盘缓存目录（exe 同级的 cache/images），key 是 url 的 md5
    static QString cacheFilePath(const QString &fullUrl);
    // 缓存命中上限，超过就整体清空，避免无限涨
    static constexpr int MAX_CACHE_FILES = 500;

private:
    // 命中磁盘缓存时直接返回 true
    bool loadFromDiskCache(const QString &fullUrl, QPixmap &out);
    bool saveToDiskCache(const QString &fullUrl, const QPixmap &pix);

    // ★ 所有下载共用这一个 QNetworkAccessManager。
    //   之前是每次下载都 new 一个（挂在this 上、从不释放），
    //   聊一天下来能攒几百个 QNAM，每个都带自己的连接池和线程状态 —— 货真价实的泄漏。
    QNetworkAccessManager *_manager = nullptr;
};

#endif // IMAGEMANAGER_H