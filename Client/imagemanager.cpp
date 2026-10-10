#include "imagemanager.h"
#include "global.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDebug>

std::shared_ptr<ImageManager> ImageManager::instance()
{
    static std::shared_ptr<ImageManager> mgr = std::make_shared<ImageManager>();
    return mgr;
}

QString ImageManager::toFullUrl(const QString &serverPath)
{
    if (serverPath.isEmpty()) {
        return QString();
    }
    // 已经是完整 URL 就不再拼前缀
    if (serverPath.startsWith("http://") || serverPath.startsWith("https://")) {
        return serverPath;
    }
    // 服务端返回的是 "upload/xxx.png"，补上前导斜杠
    QString p = serverPath;
    if (!p.startsWith("/")) {
        p.prepend("/");
    }
    return gate_url_prefix + p;
}

QString ImageManager::cacheFilePath(const QString &fullUrl)
{
    QByteArray h = QCryptographicHash::hash(fullUrl.toUtf8(), QCryptographicHash::Md5).toHex();
    QString dir = QDir(QCoreApplication::applicationDirPath())
                      .absoluteFilePath("cache/images");
    QDir().mkpath(dir);
    return dir + "/" + h;
}

bool ImageManager::loadFromDiskCache(const QString &fullUrl, QPixmap &out)
{
    QString path = cacheFilePath(fullUrl);
    QFileInfo fi(path);
    if (!fi.exists() || fi.size() == 0) {
        return false;
    }
    QPixmap pm;
    if (!pm.load(path)) {
        return false;      // 文件损坏了，删掉让下次重新下
    }
    out = pm;
    return true;
}

bool ImageManager::saveToDiskCache(const QString &fullUrl, const QPixmap &pix)
{
    QString path = cacheFilePath(fullUrl);
    QDir dir(QFileInfo(path).absolutePath());
    // 超上限就整体清空。聊天软件的图片缓存不值得做 LRU，
    // 目录一大就清，逻辑简单且不会无限增长。
    if (dir.entryList(QDir::Files).size() > MAX_CACHE_FILES) {
        for (const QString &f : dir.entryList(QDir::Files)) {
            QFile::remove(dir.absoluteFilePath(f));
        }
    }
    if (pix.isNull()) {
        return false;
    }
    if (!pix.save(path, "PNG")) {
        qDebug() << "save image cache failed:" << path;
        return false;
    }
    return true;
}

void ImageManager::downloadImage(const QString &url,
                                 std::function<void(bool, const QPixmap &, const QString &)> cb)
{
    QString full = toFullUrl(url);
    if (full.isEmpty()) {
        cb(false, QPixmap(), "图片路径为空");
        return;
    }
    QUrl qurl(full);
    if (!qurl.isValid()) {
        cb(false, QPixmap(), QString("URL 非法: %1").arg(full));
        return;
    }

    // ---- 磁盘缓存 ----
    QPixmap cached;
    if (loadFromDiskCache(full, cached)) {
        cb(true, cached, "");
        return;
    }

    QNetworkRequest request(qurl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "HChatClient");

    // ★ 每个 reply 各自带一份回调，finished 时按对象自己分发 —— 天然支持并发，
    //   不需要 pending 表，也不会出现「第 1 个回包唤醒所有回调」。
    //
    // ★★ QNetworkAccessManager 必须【复用】。原来每次下载都 new 一个并挂在 this 上，
    //   但从来不释放：聊一天能攒几百个 QNAM，每个都带自己的连接池 —— 实打实的泄漏。
    //   这里改成惰性创建、全局共用一个。
    if (_manager == nullptr) {
        _manager = new QNetworkAccessManager(this);
    }
    QNetworkReply *r2 = _manager->get(request);

    QObject::connect(r2, &QNetworkReply::finished, this, [r2, cb, full]() {
        r2->deleteLater();
        if (r2->error() != QNetworkReply::NoError) {
            qDebug() << "download image failed:" << full << r2->errorString();
            cb(false, QPixmap(), r2->errorString());
            return;
        }
        QByteArray bytes = r2->readAll();
        if (bytes.isEmpty()) {
            cb(false, QPixmap(), "服务端返回了空内容");
            return;
        }
        QPixmap pix;
        if (!pix.loadFromData(bytes)) {
            cb(false, QPixmap(), "不是有效的图片数据");
            return;
        }
        // 存盘缓存（失败不影响本次显示）
        ImageManager::instance()->saveToDiskCache(full, pix);
        cb(true, pix, "");
    });
    qDebug() << "download image:" << full;
}