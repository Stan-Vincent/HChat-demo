#include "httpmgr.h"

HttpMgr::~HttpMgr()
{

}

HttpMgr::HttpMgr() {
    //连接http请求和完成信号，信号槽机制保证队列消费
    connect(this,&HttpMgr::sig_http_finish,this,&HttpMgr::slot_http_finish);
}

void HttpMgr::PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules mod)
{
    //创建一个HTTP POST请求，并设置请求头和请求体
    QByteArray data = QJsonDocument(json).toJson();

    //通过url构造请求
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader,QByteArray::number(data.length()));

    //获取自己的智能指针，构造伪闭包并增加智能指针引用计数
    auto self = shared_from_this();
    //发送post请求，等待网关处理并处理响应
    QNetworkReply *reply = _manager.post(request,data);

    //设置信号和槽等待发送完成
    QObject::connect(reply ,&QNetworkReply::finished ,[self,reply ,req_id ,mod](){
        //处理错误情况
        if(reply->error() != QNetworkReply::NoError){
            qDebug() << reply->errorString();
            //发送失败信号
            emit self->sig_http_finish(req_id, "", ErrorCodes::ERR_NETWORK ,mod);
            reply->deleteLater();
            return ;
        }

        //无错误
        QString res = reply->readAll();
        //发送成功信号 --> 调用HttpMgr::slot_http_finish
        emit self->sig_http_finish(req_id, res, ErrorCodes::SUCCESS ,mod);
        reply->deleteLater();
        return ;
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
}


