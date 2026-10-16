#ifndef UPLOADMANAGER_H
#define UPLOADMANAGER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <functional>
#include <memory>

/**
 * 图片上传管理器
 *
 * 走 GateServer 的 POST /upload_image，body 是 {uid, file_name, data(base64)}，
 * 返回 {error, url, unique_name, size, md5}。
 *
 * 【为什么不用断点续传】教程 day39 用的是独立的 ResourceServer + 分片上传 + 进度回调。
 * 那是给真实网络环境（大文件、弱网）用的，这里简化成一次性上传，
 * 但【字段命名对齐教程】（url / unique_name / size / md5），
 * 以后要换成 ResourceServer，只需要替换本文件的实现，调用方不用改。
 *
 * 用法：
 *   UploadManager::instance()->uploadImage(uid, localPath,
 *       [ok, urlPath, errMsg](bool ok, const QString& urlPath, const QString& errMsg) { ... });
 */
class UploadManager : public QObject
{
    Q_OBJECT
public:
    static std::shared_ptr<UploadManager> instance();

    UploadManager() = default;

    // 把本地图片文件上传，返回服务端上的相对路径
    void uploadImage(int uid, const QString& localPath,
                     std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb);

    // 把内存里的图片（如裁剪后的头像）上传，fmt 传 "png"/"jpg"
    void uploadImageData(int uid, const QImage& image, const QString& fileName,
                         std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb);

    // 把本地任意文件（文档/压缩包/图片等）上传，走 /upload_file。
    // 内部读的是原始字节 + base64，与图片上传同一套逻辑，只是大小上限和路由不同。
    void uploadFile(int uid, const QString& localPath,
                    std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb);

    // 限制大小：超过这个字节数直接失败，不浪费带宽
    static constexpr qint64 MAX_IMAGE_SIZE = 10 * 1024 * 1024;   // 10 MB
    static constexpr qint64 MAX_FILE_SIZE  = 20 * 1024 * 1024;   // 20 MB（与服务端 20MB 上限对齐）

    // 构造开放：instance() 用 make_shared 创建它

private:
    // 构造要开放：instance() 用 make_shared 创建它，私有构造会导致编译错误
    // endpoint: "/upload_image"（图片，默认）或 "/upload_file"（任意文件）
    void doUpload(int uid, const QString& fileName, const QByteArray& rawBytes,
                  std::function<void(bool, const QString&, const QString&)> cb,
                  const QString& endpoint = "/upload_image");
};

#endif // UPLOADMANAGER_H