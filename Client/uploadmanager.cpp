#include "uploadmanager.h"
#include "httpmgr.h"
#include "global.h"
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

std::shared_ptr<UploadManager> UploadManager::instance()
{
    // 故意用静态局部而不是全局对象：避免静态初始化顺序问题
    static std::shared_ptr<UploadManager> mgr = std::make_shared<UploadManager>();
    return mgr;
}

void UploadManager::uploadImage(int uid, const QString& localPath,
                               std::function<void(bool, const QString&, const QString&)> cb)
{
    QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        cb(false, "", QString("文件不存在: %1").arg(localPath));
        return;
    }
    if (fi.size() > MAX_IMAGE_SIZE) {
        cb(false, "", QString("图片太大(%1 KB)，上限 %2 MB")
             .arg(fi.size() / 1024).arg(MAX_IMAGE_SIZE / 1024 / 1024));
        return;
    }

    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        cb(false, "", QString("无法读取文件: %1").arg(f.errorString()));
        return;
    }
    QByteArray raw = f.readAll();
    f.close();
    if (raw.isEmpty()) {
        cb(false, "", "文件是空的");
        return;
    }

    doUpload(uid, fi.fileName(), raw, std::move(cb));
}

void UploadManager::uploadImageData(int uid, const QImage& image, const QString& fileName,
                                   std::function<void(bool, const QString&, const QString&)> cb)
{
    if (image.isNull()) {
        cb(false, "", "图片无效");
        return;
    }
    QBuffer buf;
    if (!buf.open(QIODevice::WriteOnly)) {
        cb(false, "", "无法创建内存缓冲");
        return;
    }
    // 一律存成 png：体积小、无损、支持透明
    if (!image.save(&buf, "PNG")) {
        cb(false, "", "PNG 编码失败");
        return;
    }
    QByteArray raw = buf.data();
    if (raw.isEmpty()) {
        cb(false, "", "PNG 编码结果为空");
        return;
    }
    QString name = fileName.isEmpty() ? QString("image.png") : fileName;
    if (!name.endsWith(".png", Qt::CaseInsensitive)) {
        name += ".png";
    }
    doUpload(uid, name, raw, std::move(cb));
}

void UploadManager::doUpload(int uid, const QString& fileName, const QByteArray& rawBytes,
                             std::function<void(bool, const QString&, const QString&)> cb)
{
    QJsonObject json;
    json["uid"] = uid;
    json["file_name"] = fileName;
    // 服务端用base64Decode 解码（能跳过折行），这里用 Compact 不带换行
    json["data"] = QString::fromLatin1(rawBytes.toBase64());

    // ★★ 这里【不能】走 HttpMgr 的 sig_upload_mod_finish 信号。
    //   那个信号对所有上传请求共用，而它只带一个 ReqId（全都等于
    //   ID_UPLOAD_IMAGE_REQ），服务端回包也不带能区分请求的标识。
    //   结果就是：连发两张图时，第一个回包会同时唤醒第 1 个和第 2 个回调，
    //   两张图都被当成第 1 个的结果 —— 第 2 张图的消息带着错的 url 发出去。
    //
    //   用 PostHttpReqRaw：按 reply 对象分发，一请求一回调，天然一一对应。
    HttpMgr::GetInstance()->PostHttpReqRaw(
        QUrl(gate_url_prefix + "/upload_image"), json,
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
            qDebug() << "upload ok:" << url << obj.value("size").toInt() << "bytes";
            cb(true, url, "");
        },
        // ctx 传 UploadManager 自己：manager 是 make_shared 创建的，
        // 只要它还在，连接就在；它没了连接自动断开，不会回调已销毁的对象。
        this);

    qDebug() << "upload_image requested:" << fileName << rawBytes.size() << "bytes";
}