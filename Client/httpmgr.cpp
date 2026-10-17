#include "httpmgr.h"
#include "logcategory.h"

HttpMgr::~HttpMgr()
{

}

void HttpMgr::PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules mod)
{
    //创建一个HTTP POST请求，并设置请求头和请求体
    QByteArray data = QJsonDocument(json).toJson();
    //通过url构造请求
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.length()));
    //发送请求，并处理响应, 获取自己的智能指针，构造伪闭包并增加智能指针引用计数
    auto self = shared_from_this();
    QNetworkReply * reply = _manager.post(request, data);
    //设置信号和槽等待发送完成
    QObject::connect(reply, &QNetworkReply::finished, [reply, self, req_id, mod](){
        //处理错误的情况
        if(reply->error() != QNetworkReply::NoError){
            qCDebug(lcNetwork) << reply->errorString();
            //发送信号通知完成
            emit self->sig_http_finish(req_id, "", ErrorCodes::ERR_NETWORK, mod);
            reply->deleteLater();
            return;
        }

        //无错误则读回请求
        QString res = reply->readAll();

        //发送信号通知完成
        emit self->sig_http_finish(req_id, res, ErrorCodes::SUCCESS,mod);
        reply->deleteLater();
        return;
    });
}

HttpMgr::HttpMgr()
{
    //连接http请求和完成信号，信号槽机制保证队列消费
    connect(this, &HttpMgr::sig_http_finish, this, &HttpMgr::slot_http_finish);
}

void HttpMgr::PostHttpReqRaw(QUrl url, const QJsonObject& json,
                             std::function<void(bool, const QString&, const QString&)> cb,
                             QObject* ctx,
                             std::function<void(qint64 sent, qint64 total)> progressCb)
{
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.length()));

    //★ 挂在 _manager（HttpMgr 的成员）上，跟着 HttpMgr 一起活，不用每次 new
    QNetworkReply *reply = _manager.post(request, data);
    // ctx 为空时退化成 this，至少保证不会在 HttpMgr 析构后回调
    QObject* guard = (ctx != nullptr) ? ctx : static_cast<QObject*>(this);

    // 上传进度：大文件（base64 后 20MB+）时让 UI 能显示进度条。
    // total 在连接建立前可能是 -1，调用方要能兼容。
    if (progressCb) {
        QObject::connect(reply, &QNetworkReply::uploadProgress, guard,
                         [progressCb](qint64 sent, qint64 total) {
                             progressCb(sent, total);
                         });
    }

    QObject::connect(reply, &QNetworkReply::finished, guard,
                     [reply, cb]()
    {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcNetwork) << "http raw failed:" << reply->errorString();
            cb(false, "", reply->errorString());
            return;
        }
        cb(true, QString::fromUtf8(reply->readAll()), "");
    });
}

void HttpMgr::PostHttpReqRawBody(QUrl url, QIODevice* body, qint64 size,
                                 const QString& contentType,
                                 std::function<void(bool ok, const QString& res, const QString& errMsg)> cb,
                                 QObject* ctx,
                                 std::function<void(qint64 sent, qint64 total)> progressCb)
{
    QNetworkRequest request(url);
    // 不能是 application/json：服务端见到 json 会当成"老的 base64 信封"直接拒。
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (size >= 0) {
        request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(size));
    }

    //★ 挂在 _manager（HttpMgr 的成员）上，跟着 HttpMgr 一起活，不用每次 new
    QNetworkReply *reply = _manager.post(request, body);
    QObject* guard = (ctx != nullptr) ? ctx : static_cast<QObject*>(this);

    // body 的所有权在这里交割：reply 析构时一定 deleteLater。
    // QNAM 不接管 QIODevice，不接这一句就是泄漏（大文件场景每次传都漏一个 QFile）。
    QObject::connect(reply, &QObject::destroyed, body, &QObject::deleteLater);

    // 上传进度：有了确切的 Content-Length，total 就是真实文件大小，
    // 不再是 -1，UI 可以直接做百分比进度条。
    //
    // ★ 过滤 Qt 的收尾脉冲：一次上传完成后，QNAM 还会再发一次
    //   uploadProgress(0, 0)。50MB 上传实测序列：
    //     (16384/52428800) ... (52428800/52428800) (0/0)
    //   透传这个 (0,0) 的后果在 UI 侧很具体：
    //     · ChatPage::refreshUploadBar 靠 t.second > 0 判断"total 已知"，
    //       (0,0) 会让它退化成不确定模式 —— 进度条刚到 100% 又跳回走马灯；
    //     · 多任务并发时，已完成任务的 total 从分母里消失，
    //       百分比不升反降（2 个 50MB：一个传完后 75% -> 50%）。
    //   (0,0) 对进度条没有任何信息量，一律丢掉。
    if (progressCb) {
        QObject::connect(reply, &QNetworkReply::uploadProgress, guard,
                         [progressCb](qint64 sent, qint64 total) {
                             if (sent == 0 && total == 0) {
                                 return;
                             }
                             progressCb(sent, total);
                         });
    }

    QObject::connect(reply, &QNetworkReply::finished, guard,
                     [reply, cb]()
    {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcNetwork) << "http raw body failed:" << reply->errorString();
            cb(false, "", reply->errorString());
            return;
        }
        cb(true, QString::fromUtf8(reply->readAll()), "");
    });
}

void HttpMgr::slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod)
{
    if(mod == Modules::REGISTERMOD){
        //发送信号通知指定模块http响应结束
        emit sig_reg_mod_finish(id, res, err);
    }

    if(mod == Modules::RESETMOD){
        //发送信号通知指定模块http响应结束
        emit sig_reset_mod_finish(id, res, err);
    }

    if(mod == Modules::LOGINMOD){
        emit sig_login_mod_finish(id, res, err);
    }

    if(mod == Modules::UPLOADMOD){
        emit sig_upload_mod_finish(id, res, err);
    }

    if(mod == Modules::USERINFOMOD){
        emit sig_userinfo_mod_finish(id, res, err);
    }
}
