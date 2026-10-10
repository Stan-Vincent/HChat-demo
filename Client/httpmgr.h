#ifndef HTTPMGR_H
#define HTTPMGR_H
#include "singleton.h"
#include <QString>
#include <QUrl>
#include <QObject>
#include <QNetworkAccessManager>
#include "global.h"


class HttpMgr:public QObject, public Singleton<HttpMgr>,
        public std::enable_shared_from_this<HttpMgr>
{
    Q_OBJECT

public:
    ~HttpMgr();
    void PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules mod);

    // ★ 按 reply 对象分发的 POST。
    //
    // 【为什么需要它】PostHttpReq 走的是「信号 + ReqId」两级分发，但 ReqId 只能
    // 区分【模块】，区分不了【同一模块的第几次请求】。上传头像、提交资料都可能出现
    // 连续多次调用，于是：
    //   ① 每次调用挂一个永久 lambda -> 连接无限累积；
    //   ② 第 1 个回包会唤醒全部累积的回调 -> 弹出 N 个对话框 / 把第 2 张图
    //      当成第 1 张的结果。
    //
    // 这个版本直接在自己的 reply 上连finished，一请求一回调，天然一一对应。
    // ctx 是生命周期守卫：ctx 被销毁时 Qt 自动断开连接，回调不会再被调用。
    void PostHttpReqRaw(QUrl url, const QJsonObject& json,
                        std::function<void(bool ok, const QString& res, const QString& errMsg)> cb,
                        QObject* ctx);
private:
    friend class Singleton<HttpMgr>;
    HttpMgr();
    QNetworkAccessManager _manager;
public slots:
    void slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);
signals:
    void sig_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);
    //注册模块http相关请求完成发送此信号
    void sig_reg_mod_finish(ReqId id, QString res, ErrorCodes err);
    void sig_reset_mod_finish(ReqId id, QString res, ErrorCodes err);
    void sig_login_mod_finish(ReqId id, QString res, ErrorCodes err);
    //上传模块（图片上传到 GateServer）
    void sig_upload_mod_finish(ReqId id, QString res, ErrorCodes err);
    //个人资料模块（更新昵称/签名/头像）
    void sig_userinfo_mod_finish(ReqId id, QString res, ErrorCodes err);
};

#endif // HTTPMGR_H
