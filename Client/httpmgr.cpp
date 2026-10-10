#include "httpmgr.h"

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
            qDebug() << reply->errorString();
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
                             QObject* ctx)
{
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.length()));

    //★ 挂在 _manager（HttpMgr 的成员）上，跟着 HttpMgr 一起活，不用每次 new
    QNetworkReply *reply = _manager.post(request, data);
    // ctx 为空时退化成 this，至少保证不会在 HttpMgr 析构后回调
    QObject* guard = (ctx != nullptr) ? ctx : static_cast<QObject*>(this);

    QObject::connect(reply, &QNetworkReply::finished, guard,
                     [reply, cb]()
    {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "http raw failed:" << reply->errorString();
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
