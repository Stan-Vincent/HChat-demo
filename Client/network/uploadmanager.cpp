#include "uploadmanager.h"
#include "httpmgr.h"
#include "global.h"
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include "logcategory.h"

std::shared_ptr<UploadManager> UploadManager::instance()
{
    // 故意用静态局部而不是全局对象：避免静态初始化顺序问题
    static std::shared_ptr<UploadManager> mgr = std::make_shared<UploadManager>();
    return mgr;
}

// 静态成员定义：默认 300MB，main.cpp 读 config.ini 后可调 setMaxBytes 覆盖。
// 这里是唯一的定义点——头文件里只声明，避免每个包含它的 .cpp 各持一份副本。
qint64 UploadManager::MAX_IMAGE_SIZE = 300 * 1024 * 1024;
qint64 UploadManager::MAX_FILE_SIZE  = 300 * 1024 * 1024;

void UploadManager::setMaxBytes(qint64 bytes)
{
    if (bytes <= 0) {
        // 没配、配错、配成 0：保持默认值，不要把上限压成 0 让所有上传都失败
        qCWarning(lcNetwork) << "config.ini 的 [Upload] MaxBytes 无效:" << bytes
                             << "，保持默认" << MAX_FILE_SIZE;
        return;
    }
    MAX_IMAGE_SIZE = bytes;
    MAX_FILE_SIZE  = bytes;
    qCDebug(lcNetwork) << "上传上限(from config.ini):" << bytes << "bytes";
}

void UploadManager::uploadImage(int uid, const QString& localPath,
                               std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb,
                               ProgressCb progressCb)
{
    QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        cb(false, "", QString("文件不存在: %1").arg(localPath));
        return;
    }
    if (fi.size() > MAX_IMAGE_SIZE) {
        cb(false, "", QString("图片太大(%1 MB)，上限 %2 MB")
             .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 1).arg(MAX_IMAGE_SIZE / 1024 / 1024));
        return;
    }

    // ★ 流式：不再 readAll() 把整个文件搬进内存，直接把打开的 QFile 交给
    //   HttpMgr，QNAM 会按块读并按块写 socket。文件句柄的所有权也一并交割。
    QFile* f = new QFile(localPath);
    if (!f->open(QIODevice::ReadOnly)) {
        QString err = f->errorString();
        delete f;
        cb(false, "", QString("无法读取文件: %1").arg(err));
        return;
    }
    if (f->size() == 0) {
        delete f;
        cb(false, "", "文件是空的");
        return;
    }

    doUpload(uid, fi.fileName(), f, f->size(), std::move(cb), QString("/upload_image"), std::move(progressCb));
}

void UploadManager::uploadImageData(int uid, const QImage& image, const QString& fileName,
                                   std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb,
                                   ProgressCb progressCb)
{
    if (image.isNull()) {
        cb(false, "", "图片无效");
        return;
    }
    QByteArray raw;
    QBuffer buf(&raw);
    if (!buf.open(QIODevice::WriteOnly)) {
        cb(false, "", "无法创建内存缓冲");
        return;
    }
    // 一律存成 png：体积小、无损、支持透明
    if (!image.save(&buf, "PNG")) {
        cb(false, "", "PNG 编码失败");
        return;
    }
    buf.close();
    if (raw.isEmpty()) {
        cb(false, "", "PNG 编码结果为空");
        return;
    }
    if (raw.size() > MAX_IMAGE_SIZE) {
        cb(false, "", QString("图片太大(%1 MB)，上限 %2 MB")
             .arg(raw.size() / 1024.0 / 1024.0, 0, 'f', 1).arg(MAX_IMAGE_SIZE / 1024 / 1024));
        return;
    }
    QString name = fileName.isEmpty() ? QString("image.png") : fileName;
    if (!name.endsWith(".png", Qt::CaseInsensitive)) {
        name += ".png";
    }

    // 内存里的图片没有文件可读，只能造一个 QBuffer 当"设备"。
    // 这里用 setData 让 QBuffer 自己拷贝一份：raw 是栈上的，请求还没发完就可能
    // 出了作用域，指针指向栈内存就是 use-after-free。
    QBuffer* dev = new QBuffer();
    dev->setData(raw);
    if (!dev->open(QIODevice::ReadOnly)) {
        QString err = dev->errorString();
        delete dev;
        cb(false, "", QString("无法读取内存缓冲: %1").arg(err));
        return;
    }

    doUpload(uid, name, dev, raw.size(), std::move(cb), QString("/upload_image"), std::move(progressCb));
}

// 通用文件上传：与 uploadImage 完全同构（流式直发原始字节），
// 区别只有一点：路由走 /upload_file。
// 服务端已把上传接口泛化，会保留原始扩展名（可执行/脚本类改存 .bin）。
void UploadManager::uploadFile(int uid, const QString& localPath,
                               std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb,
                               ProgressCb progressCb)
{
    QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        cb(false, "", QString("文件不存在: %1").arg(localPath));
        return;
    }
    if (fi.size() > MAX_FILE_SIZE) {
        cb(false, "", QString("文件太大(%1 MB)，上限 %2 MB")
             .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 1).arg(MAX_FILE_SIZE / 1024 / 1024));
        return;
    }

    QFile* f = new QFile(localPath);
    if (!f->open(QIODevice::ReadOnly)) {
        QString err = f->errorString();
        delete f;
        cb(false, "", QString("无法读取文件: %1").arg(err));
        return;
    }
    if (f->size() == 0) {
        delete f;
        cb(false, "", "文件是空的");
        return;
    }

    doUpload(uid, fi.fileName(), f, f->size(), std::move(cb), QString("/upload_file"), std::move(progressCb));
}

void UploadManager::doUpload(int uid, const QString& fileName, QIODevice* body, qint64 size,
                             std::function<void(bool, const QString&, const QString&)> cb,
                             const QString& endpoint,
                             ProgressCb progressCb)
{
    // ★ 元信息走查询串，请求体只放文件字节。
    //   服务端就是这么设计的（见 GateServer 流式上传那段的注释）：
    //   它在解析请求头时就能拿到 file_name 和 Content-Length，
    //   可以在写第一个字节之前就拒绝超限文件，而不是传完 300MB 再告诉你不行。
    //
    //   QUrlQuery 会自己做百分号编码，所以文件名里带空格、中文、#、? 都不会把
    //   查询串结构搞坏（原来直接字符串拼接，一个 '#' 就能把后面的参数全废掉）。
    QUrl url(gate_url_prefix + endpoint);
    QUrlQuery query;
    query.addQueryItem("uid", QString::number(uid));
    query.addQueryItem("file_name", fileName);
    url.setQuery(query);

    // ★★ 这里【不能】走 HttpMgr 的 sig_upload_mod_finish 信号。
    //   那个信号对所有上传请求共用，而它只带一个 ReqId（全都等于
    //   ID_UPLOAD_IMAGE_REQ），服务端回包也不带能区分请求的标识。
    //   结果就是：连发两张图时，第一个回包会同时唤醒第 1 个和第 2 个回调，
    //   两张图都被当成第 1 个的结果 —— 第 2 张图的消息带着错的 url 发出去。
    //
    //   用 PostHttpReqRawBody：按 reply 对象分发，一请求一回调，天然一一对应。
    HttpMgr::GetInstance()->PostHttpReqRawBody(
        url, body, size, QString("application/octet-stream"),
        [cb](bool ok, const QString& res, const QString& errMsg) {
            if (!ok) {
                cb(false, "", errMsg.isEmpty() ? QString("网络请求失败") : errMsg);
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(res.toUtf8());
            if (doc.isNull() || !doc.isObject()) {
                cb(false, "", "服务端返回的不是合法 JSON");
                return;
            }
            QJsonObject obj = doc.object();
            if (obj.value("error").toInt() != 0) {
                cb(false, "", obj.value("msg").toString("上传失败"));
                return;
            }
            QString url = obj.value("url").toString();
            if (url.isEmpty()) {
                cb(false, "", "服务端没返回文件地址");
                return;
            }
            qCDebug(lcNetwork) << "upload ok:" << url << obj.value("size").toInt() << "bytes";
            cb(true, url, "");
        },
        // ctx 传 UploadManager 自己：manager 是 make_shared 创建的，
        // 只要它还在，连接就在；它没了连接自动断开，不会回调已销毁的对象。
        this,
        // 上传进度透传给 UI（流式带上了确切 Content-Length，total 是真实大小）
        progressCb);

    qCDebug(lcNetwork) << "upload requested:" << endpoint << fileName << size << "bytes";
}
