#ifndef UPLOADMANAGER_H
#define UPLOADMANAGER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <functional>
#include <memory>

/**
 * 图片/文件上传管理器
 *
 * 走 GateServer 的 POST /upload_image 或 /upload_file，【请求体就是原始字节】
 * （无 base64、无 JSON 信封），元信息放查询串，返回 {error, url, unique_name, size, md5}。
 *
 * 【为什么改成流式】2026-10-17 之前是 base64 塞 JSON 整包 POST：原文件 →base64 ×4/3
 * →QString::fromLatin1 UTF-16 翻倍 →QJsonValue 深拷贝，300MB 的文件客户端峰值要
 * 1.5–2.7GB，服务端每连接还要 1.1GB，纯粹是在拿内存换代码简洁。
 * 服务端同步换成了 http::buffer_body 边收边写盘，两端都按 64KB 块流动，
 * 300MB 文件客户端峰值内存只有几十 MB（QFile 缓冲 + 一个 chunk）。
 *
 * 【为什么不用断点续传】教程 day39 用的是独立的 ResourceServer + 分片上传 + 进度回调。
 * 那是给真实网络环境（大文件、弱网）用的，这里简化成一次性上传，
 * 但【字段命名对齐教程】（url / unique_name / size / md5），
 * 以后要换成 ResourceServer，只需要替换本文件的实现，调用方不用改。
 *
 * 用法：
 *   UploadManager::instance()->uploadImage(uid, localPath,
 *       [ok, urlPath, errMsg](bool ok, const QString& urlPath, const QString& errMsg) { ... });
 *
 * 【上传进度】每个接口都带一个可选的 progressCb(sent, total)：
 *   流式上传带了确切的 Content-Length，所以 total 就是真实文件大小（不再是 -1），
 *   UI 可以直接做百分比进度条。进度回调在 ctx 对象销毁后自动断开。
 */
class UploadManager : public QObject
{
    Q_OBJECT
public:
    static std::shared_ptr<UploadManager> instance();

    UploadManager() = default;

    // 上传进度回调：已发送字节 / 总字节（total 可能是 -1 = 未知）
    using ProgressCb = std::function<void(qint64 sent, qint64 total)>;

    // 把本地图片文件上传，返回服务端上的相对路径
    void uploadImage(int uid, const QString& localPath,
                     std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb,
                     ProgressCb progressCb = nullptr);

    // 把内存里的图片（如裁剪后的头像）上传，fmt 传 "png"/"jpg"
    void uploadImageData(int uid, const QImage& image, const QString& fileName,
                         std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb,
                         ProgressCb progressCb = nullptr);

    // 把本地任意文件（文档/压缩包/图片等）上传，走 /upload_file。
    // 与图片上传同一套流式逻辑，只有路由不同；服务端会保留原始扩展名
    // （可执行/脚本类中和成 .bin）。
    void uploadFile(int uid, const QString& localPath,
                    std::function<void(bool ok, const QString& urlPath, const QString& errMsg)> cb,
                    ProgressCb progressCb = nullptr);

    // 限制大小：超过这个字节数直接失败，不浪费带宽。
    // ★ 2026-10-17 先从 10MB/20MB 提到 300MB 与服务端对齐，再改成
    //   【运行时可配】：main.cpp 启动时从 config.ini 的 [Upload] MaxBytes 读进来
    //   （走 setMaxBytes）。改上限不用重新编译。
    //   流式改造之后内存不再是瓶颈，所以上限就直接等于服务端上限。
    //
    // ⚠ 客户端和服务端各有各的 config.ini，两边必须配成同一个值。
    //   客户端配大了 = 用户传了几百 MB 才被服务端告知"太大"；
    //   客户端配小了 = 服务端明明能收，客户端却先拦下来。
    static qint64 MAX_IMAGE_SIZE;   // 默认 300MB，由 config.ini 覆盖
    static qint64 MAX_FILE_SIZE;    // 默认 300MB，由 config.ini 覆盖

    // 启动时从 config.ini 设置上限（main.cpp 调用）。
    // 传 <=0 表示"没配或配错了"，保持默认值不变。
    static void setMaxBytes(qint64 bytes);

    // 构造开放：instance() 用 make_shared 创建它

private:
    // endpoint: "/upload_image"（图片，默认）或 "/upload_file"（任意文件）
    // ★ body 必须是 new 出来且已 open(ReadOnly) 的 QIODevice，所有权交给
    //   HttpMgr::PostHttpReqRawBody（它挂在 reply 的 destroyed 上 deleteLater）。
    //   size 传确切字节数以填 Content-Length，服务端据此在收到第一个字节前就拒绝超限文件。
    void doUpload(int uid, const QString& fileName, QIODevice* body, qint64 size,
                  std::function<void(bool, const QString&, const QString&)> cb,
                  const QString& endpoint = "/upload_image",
                  ProgressCb progressCb = nullptr);
};

#endif // UPLOADMANAGER_H