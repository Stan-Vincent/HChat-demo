#include "tcpmgr.h"
#include <QAbstractSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include "usermgr.h"
#include "logcategory.h"

TcpMgr::TcpMgr():_host(""),_port(0),_b_recv_pending(false),_b_intentional_close(false),
    _b_reconnecting(false),_reconnect_attempt(0),_message_id(0),_message_len(0)
{
    _reconnect_timer = new QTimer(this);
    _reconnect_timer->setSingleShot(true);
    connect(_reconnect_timer, &QTimer::timeout, this, &TcpMgr::slot_reconnect_timeout);
    _connect_watchdog = new QTimer(this);
    _connect_watchdog->setSingleShot(true);
    connect(_connect_watchdog, &QTimer::timeout, this, &TcpMgr::slot_connect_watchdog_timeout);

    QObject::connect(&_socket, &QTcpSocket::connected, [&]() {
           _connect_watchdog->stop();
           qCDebug(lcNetwork) << "Connected to server!";
           if (_b_reconnecting) {
               // 重连场景：不发 sig_con_success（那是登录页流程用的），
               // 直接重放一次聊天登录握手 —— token 还在 UserMgr 里，
               // 服务端认它就能把 session 重新绑回这个 uid。
               qCDebug(lcNetwork) << "socket reconnected, redo chat login handshake";
               QJsonObject jsonObj;
               jsonObj["uid"] = UserMgr::GetInstance()->GetUid();
               jsonObj["token"] = UserMgr::GetInstance()->GetToken();
               QJsonDocument doc(jsonObj);
               emit sig_send_data(ID_CHAT_LOGIN, doc.toJson(QJsonDocument::Compact));
               return;
           }
           // 连接建立后发送消息
            emit sig_con_success(true);
       });

       QObject::connect(&_socket, &QTcpSocket::readyRead, [&]() {
           // 当有数据可读时，读取所有数据
           // 读取所有数据并追加到缓冲区
           _buffer.append(_socket.readAll());

           // ★★ 这里【不能】用一次性的 QDataStream 来解包。
           //   原来的写法是：在循环外构造 QDataStream stream(&_buffer)，
           //   循环内部又不断 `_buffer = _buffer.mid(...)` 重新赋值 buffer。
           //   两个问题：
           //   ① stream 的读游标跨迭代累积 —— 第二条消息会从偏移 4 开始读，
           //      把上一条消息体的前 4 字节当成消息头，直接解出错乱的 id/len。
           //   ② _buffer 重新赋值会让QByteArray 重新分配内存，
           //      stream 内部的 QBuffer 指向旧内存 —— 悬空指针，行为未定义。
           //
           //   TCP 是流式的，服务端完全可能把多个回包合并成一个包发过来
           //   （加载聊天列表时就是连续多个回包），所以必须正确处理粘包。
           //
           //   改成：不用QDataStream，直接按「2 字节大端 id + 2 字节大端 len」
           //   手工解析。服务端 SendNode 用的是 host_to_network_short，即大端。
           forever {
               // ---------- 1. 解析消息头 ----------
               if (!_b_recv_pending) {
                   if (_buffer.size() < static_cast<int>(HEAD_TOTAL_LEN)) {
                       return;            // 头部都不够，等更多数据
                   }
                   const uchar* p = reinterpret_cast<const uchar*>(_buffer.constData());
                   _message_id  = static_cast<quint16>((p[0] << 8) | p[1]);
                   _message_len = static_cast<quint16>((p[2] << 8) | p[3]);

                   // 小于头长或超过上限都说明流已经错位了，丢掉整个缓冲区，
                   // 否则会一直按错误的长度往下解，再也同步不回来。
                   if (_message_len <= 0 || _message_len > MAX_MSG_BODY_LEN) {
                       qCDebug(lcNetwork) << "protocol desync, illegal body len:" << _message_len
                                << " id:" << _message_id << ", drop buffer";
                       _buffer.clear();
                       _b_recv_pending = false;
                       return;
                   }

                   _buffer.remove(0, HEAD_TOTAL_LEN);
                   qCDebug(lcNetwork) << "Message ID:" << _message_id << ", Length:" << _message_len;
               }

               // ---------- 2. 解析消息体 ----------
               if (_buffer.size() < _message_len) {
                   _b_recv_pending = true;   // 还没收全，等下一包
                   return;
               }

               _b_recv_pending = false;
               QByteArray messageBody = _buffer.mid(0, _message_len);
               //qCDebug(lcNetwork) << "receive body msg is " << messageBody;

               _buffer.remove(0, _message_len);
               handleMsg(ReqId(_message_id), _message_len, messageBody);
           }
       });

       //5.15 之后版本
//       QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), [&](QAbstractSocket::SocketError socketError) {
//           Q_UNUSED(socketError)
//           qCDebug(lcNetwork) << "Error:" << _socket.errorString();
//       });

        // 处理错误：Qt 6.5 用 errorOccurred（QAbstractSocket::error() 已被移除）
        QObject::connect(&_socket,
                         QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                            [&](QAbstractSocket::SocketError socketError) {
               qCDebug(lcNetwork) << "Error:" << _socket.errorString() ;
               // ★ 重连中：任何错误都只意味着"这次没连上"，走退避重试，
               //   不再发 sig_con_success(false)（那是登录页弹错用的，
               //   重连时弹一次错就把用户吓回登录页了）。
               if (_b_reconnecting) {
                   qCDebug(lcNetwork) << "reconnect attempt failed:" << _socket.errorString();
                   _connect_watchdog->stop();
                   scheduleNextAttempt();
                   return;
               }
               switch (socketError) {
                   case QTcpSocket::ConnectionRefusedError:
                       qCDebug(lcNetwork) << "Connection Refused!";
                       emit sig_con_success(false);
                       break;
                   case QTcpSocket::RemoteHostClosedError:
                       qCDebug(lcNetwork) << "Remote Host Closed Connection!";
                       break;
                   case QTcpSocket::HostNotFoundError:
                       qCDebug(lcNetwork) << "Host Not Found!";
                       emit sig_con_success(false);
                       break;
                   case QTcpSocket::SocketTimeoutError:
                       qCDebug(lcNetwork) << "Connection Timeout!";
                       emit sig_con_success(false);
                       break;
                   case QTcpSocket::NetworkError:
                       qCDebug(lcNetwork) << "Network Error!";
                       break;
                   default:
                       qCDebug(lcNetwork) << "Other Error!";
                       break;
               }
         });

        // 处理连接断开
        QObject::connect(&_socket, &QTcpSocket::disconnected, [&]() {
            qCDebug(lcNetwork) << "Disconnected from server.";
            // 半开连接的残留数据必须丢掉：否则重连后 readyRead 会先读到
            // 上一个 session 的残包，按错位的长度解下去整条流都乱了。
            _buffer.clear();
            _b_recv_pending = false;
            if (_b_reconnecting) {
                // 重连过程中又断了（放弃本次连接时 abort() 也会走到这里）：
                // 重试节奏完全由 _reconnect_timer / 看门狗驱动，这里不排下一次，
                // 否则会和 errorOccurred 分支重复排。
                return;
            }
            // 主动 CloseConnection() 触发的断开（退出登录 / 被踢）不该再弹
            // "心跳超时下线" 提示框——那个框只在【服务端真的把连接掐了】时出现。
            // 用 _b_intentional_close 区分：主动关为 true，这里就不转发。
            if (!_b_intentional_close) {
                // ★ 顺序要紧：先启动重连，再通知界面。
                //   emit 是【同步】调用，MainWindow::slot_connection_closed 里要用
                //   IsReconnecting() 决定"显示正在重连横幅"还是"直接回登录页"；
                //   要是像以前那样先 emit 再 StartReconnect，槽函数查到的还是
                //   false，于是每次都走回登录页分支，自动重连等于没接上。
                // 已经登录过（token 在手）才值得自动重连；
                // 登录页阶段就断开的（比如选服务器时网络不通）保持老行为：回登录页。
                if (!UserMgr::GetInstance()->GetToken().isEmpty()) {
                    StartReconnect();
                }
                emit sig_connection_closed();
            }
            // 无论主动被动，处理完都复位成"非主动"，等待下一次连接
            _b_intentional_close = false;
        });
        //连接发送信号用来发送数据
        QObject::connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
        //注册消息
        initHandlers();
}

void TcpMgr::CloseConnection(){
    // 标记为主动关闭：断开后 disconnected 信号就不会再转发心跳超时提示
    _b_intentional_close = true;
    // 主动断开（退出登录 / 被踢）必须取消正在进行的重连，
    // 否则旧账号会被自动拉回来。
    StopReconnect();
    _socket.close();
}

// ============================================================
//  断线自动重连
// ============================================================
void TcpMgr::StartReconnect()
{
    if (_b_reconnecting) {
        return;                     // 已在重连中，避免重复启动
    }
    if (_last_server_info.Host.isEmpty() || _last_server_info.Port.isEmpty()) {
        qCDebug(lcNetwork) << "StartReconnect: no server info saved, skip";
        return;
    }
    if (UserMgr::GetInstance()->GetToken().isEmpty()) {
        qCDebug(lcNetwork) << "StartReconnect: not logged in (no token), skip";
        return;
    }

    _b_reconnecting = true;
    _reconnect_attempt = 0;
    qCDebug(lcNetwork) << "StartReconnect to" << _last_server_info.Host
                       << _last_server_info.Port;
    // 先通知界面一次（attempt=0 表示"即将开始第 1 次"）
    emit sig_reconnect_retry(0, kMaxReconnectAttempts);
    scheduleNextAttempt();
}

void TcpMgr::StopReconnect()
{
    _b_reconnecting = false;
    _reconnect_attempt = 0;
    if (_reconnect_timer) {
        _reconnect_timer->stop();
    }
    if (_connect_watchdog) {
        _connect_watchdog->stop();
    }
}

bool TcpMgr::IsReconnecting() const
{
    return _b_reconnecting;
}

bool TcpMgr::IsConnected() const
{
    return _socket.state() == QAbstractSocket::ConnectedState;
}

void TcpMgr::scheduleNextAttempt()
{
    if (!_b_reconnecting) {
        return;
    }
    // 退避计时器还挂着就复用，避免 errorOccurred 连发几条把退避时间反复重置
    if (_reconnect_timer->isActive()) {
        return;
    }
    if (_reconnect_attempt >= kMaxReconnectAttempts) {
        qCDebug(lcNetwork) << "reconnect attempts exhausted (" << _reconnect_attempt << ")";
        StopReconnect();
        emit sig_reconnect_failed();
        return;
    }
    // 指数退避：1s / 2s / 4s / 8s / 8s（本地服务重启一般第一次就回来了）
    static const int kDelays[] = {1000, 2000, 4000, 8000, 8000};
    const int idx = qMin(_reconnect_attempt,
                         static_cast<int>(sizeof(kDelays) / sizeof(kDelays[0])) - 1);
    _reconnect_timer->start(kDelays[idx]);
}

void TcpMgr::slot_reconnect_timeout()
{
    if (!_b_reconnecting) {
        return;
    }
    ++_reconnect_attempt;
    qCDebug(lcNetwork) << "reconnect attempt" << _reconnect_attempt
                       << "of" << kMaxReconnectAttempts;
    emit sig_reconnect_retry(_reconnect_attempt, kMaxReconnectAttempts);

    _socket.connectToHost(_last_server_info.Host,
                          static_cast<uint16_t>(_last_server_info.Port.toUInt()));
    // 看门狗：连上了会 stop，卡在 Connecting 里 4s 还没动静就放弃这次
    _connect_watchdog->start(kConnectWatchdogMs);
}

void TcpMgr::slot_connect_watchdog_timeout()
{
    if (!_b_reconnecting) {
        return;
    }
    qCDebug(lcNetwork) << "reconnect watchdog timeout, abort and retry";
    _socket.abort();      // 会触发 disconnected，但重连分支里什么都不做
    scheduleNextAttempt();
}

TcpMgr::~TcpMgr(){

}
void TcpMgr::initHandlers()
{
    //auto self = shared_from_this();
    _handlers.insert(ID_CHAT_LOGIN_RSP, [this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is "<< id ;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if(jsonDoc.isNull()){
           qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
           return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        //qCDebug(lcNetwork) << "data jsonobj is " << jsonObj ;

        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Login Failed, err is Json Parse Err" << err ;
            emit sig_login_failed(err);
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qCDebug(lcNetwork) << "Login Failed, err is " << err ;
            if (_b_reconnecting) {
                // 重连时登录被拒（token 失效 / 账号被踢）：重试没意义了，
                // 直接告诉界面回登录页，不要让用户干等 5 次退避。
                StopReconnect();
                emit sig_reconnect_failed();
                return;
            }
            emit sig_login_failed(err);
            return;
        }

        // ★ 重连登录：AppendFriendList / AppendApplyList 是【追加】语义，
        //   不清的话好友列表和申请列表会翻倍。token 要留着（刚用它登录成功）。
        if (_b_reconnecting) {
            QString token = UserMgr::GetInstance()->GetToken();
            UserMgr::GetInstance()->Clear();
            UserMgr::GetInstance()->SetToken(token);
        }

        auto uid = jsonObj["uid"].toInt();
        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        auto desc = jsonObj["desc"].toString();
        auto user_info = std::make_shared<UserInfo>(uid, name, nick, icon, sex,"",desc);

        UserMgr::GetInstance()->SetUserInfo(user_info);
        // ★ 服务端 CHAT_LOGIN_RSP 并不下发 token（token 是 GateServer 的 HTTP
        //   登录回包给的，LoginDialog 已经 SetToken 过了）。所以这里不能无条件
        //   SetToken(jsonObj["token"]) —— 那会把 UserMgr 里有效的 token 覆盖成
        //   空串，断线重连的"是否登录过"判断和重发握手就全失效了。
        {
            const QString rsp_token = jsonObj["token"].toString();
            if (!rsp_token.isEmpty()) {
                UserMgr::GetInstance()->SetToken(rsp_token);
            }
        }
        if(jsonObj.contains("apply_list")){
            UserMgr::GetInstance()->AppendApplyList(jsonObj["apply_list"].toArray());
        }

        //添加好友列表
        if (jsonObj.contains("friend_list")) {
            UserMgr::GetInstance()->AppendFriendList(jsonObj["friend_list"].toArray());
        }

        // ★ 重连登录成功：只发"已恢复"，绝不重放 sig_swich_chatdlg ——
        //   那会让 MainWindow 再走一遍淡入淡出切页 + applyLoginSession +
        //   loadChatList，表现为"登录动画又放了一遍、会话列表加载两遍"。
        if (_b_reconnecting) {
            qCDebug(lcNetwork) << "reconnect login ok, emit sig_recon_success";
            StopReconnect();
            emit sig_recon_success();
            return;
        }

        emit sig_swich_chatdlg();
    });


	_handlers.insert(ID_SEARCH_USER_RSP, [this](ReqId id, int len, QByteArray data) {
		Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
		// 将QByteArray转换为QJsonDocument
		QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

		// 检查转换是否成功
		if (jsonDoc.isNull()) {
			qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
			return;
		}

		QJsonObject jsonObj = jsonDoc.object();

		if (!jsonObj.contains("error")) {
			int err = ErrorCodes::ERR_JSON;
			qCDebug(lcNetwork) << "Login Failed, err is Json Parse Err" << err;

			emit sig_user_search(nullptr);
			return;
		}

		int err = jsonObj["error"].toInt();
		if (err != ErrorCodes::SUCCESS) {
			qCDebug(lcNetwork) << "Login Failed, err is " << err;
            emit sig_user_search(nullptr);
			return;
		}
       auto search_info =  std::make_shared<SearchInfo>(jsonObj["uid"].toInt(), jsonObj["name"].toString(),
            jsonObj["nick"].toString(), jsonObj["desc"].toString(),
               jsonObj["sex"].toInt(), jsonObj["icon"].toString());

        emit sig_user_search(search_info);
		});

	_handlers.insert(ID_NOTIFY_ADD_FRIEND_REQ, [this](ReqId id, int len, QByteArray data) {
		Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
		// 将QByteArray转换为QJsonDocument
		QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

		// 检查转换是否成功
		if (jsonDoc.isNull()) {
			qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
			return;
		}

		QJsonObject jsonObj = jsonDoc.object();

		if (!jsonObj.contains("error")) {
			int err = ErrorCodes::ERR_JSON;
			qCDebug(lcNetwork) << "Login Failed, err is Json Parse Err" << err;

			emit sig_user_search(nullptr);
			return;
		}

		int err = jsonObj["error"].toInt();
		if (err != ErrorCodes::SUCCESS) {
			qCDebug(lcNetwork) << "Login Failed, err is " << err;
			emit sig_user_search(nullptr);
			return;
		}

         int from_uid = jsonObj["applyuid"].toInt();
         QString name = jsonObj["name"].toString();
         QString desc = jsonObj["desc"].toString();
         QString icon = jsonObj["icon"].toString();
         QString nick = jsonObj["nick"].toString();
         int sex = jsonObj["sex"].toInt();

        auto apply_info = std::make_shared<AddFriendApply>(
                    from_uid, name, desc,
                      icon, nick, sex);

		emit sig_friend_apply(apply_info);
		});

    _handlers.insert(ID_NOTIFY_AUTH_FRIEND_REQ, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Auth Friend Failed, err is " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Auth Friend Failed, err is " << err;
            return;
        }

        int from_uid = jsonObj["fromuid"].toInt();
        QString name = jsonObj["name"].toString();
        QString nick = jsonObj["nick"].toString();
        QString icon = jsonObj["icon"].toString();
        int sex = jsonObj["sex"].toInt();

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            QString chat_time = data["chat_time"].toString();
            auto status = data["status"].toInt();
            // ★ msg_type 必须从回包里读，不能硬编码 TEXT。
            //   否则收到的图片消息会被当成文本渲染，气泡里直接显示 "upload/xxx.png"。
            //   服务端老版本没这个字段时 toInt() 返回 0，正好等于 TEXT，向后兼容。
            int msg_type = data["msg_type"].toInt();
            if (msg_type < 0 || msg_type > 2) { msg_type = 0; }
            auto chat_data = std::make_shared<TextChatData>(msg_id, thread_id, ChatFormType::PRIVATE,
                static_cast<ChatMsgType>(msg_type), msg_content, send_uid, status, chat_time);
            chat_datas.push_back(chat_data);
        }

        auto auth_info = std::make_shared<AuthInfo>(from_uid,name,
                                                    nick, icon, sex);

        auth_info->SetChatDatas(chat_datas);

        emit sig_add_auth_friend(auth_info);
        });

    _handlers.insert(ID_ADD_FRIEND_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Add Friend Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Add Friend Failed, err is " << err;
            return;
        }

         qCDebug(lcNetwork) << "Add Friend Success " ;
      });

    _handlers.insert(ID_UPDATE_FRIEND_BACK_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "UpdateFriendBack: invalid json";
            emit sig_update_friend_back(false, 0, QString());
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "UpdateFriendBack failed, err is " << err;
            emit sig_update_friend_back(false, 0, QString());
            return;
        }

        int friend_uid = jsonObj["friend_uid"].toInt();
        QString back = jsonObj["back"].toString();
        qCDebug(lcNetwork) << "UpdateFriendBack success, uid =" << friend_uid << "back =" << back;
        emit sig_update_friend_back(true, friend_uid, back);
    });


    _handlers.insert(ID_AUTH_FRIEND_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Auth Friend Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Auth Friend Failed, err is " << err;
            return;
        }

        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        auto uid = jsonObj["uid"].toInt();
        
        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            auto status = data["status"].toInt();
            int msg_type = data["msg_type"].toInt();
            if (msg_type < 0 || msg_type > 2) { msg_type = 0; }
            auto chat_data = std::make_shared<TextChatData>(msg_id, thread_id, ChatFormType::PRIVATE,
                static_cast<ChatMsgType>(msg_type), msg_content, send_uid, status);
            chat_datas.push_back(chat_data);
        }

        auto rsp = std::make_shared<AuthRsp>(uid, name, nick, icon, sex);
        rsp->SetChatDatas(chat_datas);
        emit sig_auth_rsp(rsp);

        qCDebug(lcNetwork) << "Auth Friend Success " ;
      });


    _handlers.insert(ID_TEXT_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Chat Msg Rsp Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Chat Msg Rsp Failed, err is " << err;
            emit sig_chat_msg_rsp_error(err);
            return;
        }

        qCDebug(lcNetwork) << "Receive Text Chat Rsp Success " ;
        //收到消息后转发给页面
        auto thread_id = jsonObj["thread_id"].toInt();
        auto sender = jsonObj["fromuid"].toInt();


        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {      
            auto msg_id = data["message_id"].toInt();
            auto unique_id = data["unique_id"].toString();
            auto msg_content = data["content"].toString();
            QString chat_time = data["chat_time"].toString();
            int status = data["status"].toInt();
            int msg_type = data["msg_type"].toInt();
            if (msg_type < 0 || msg_type > 2) { msg_type = 0; }
            auto chat_data = std::make_shared<TextChatData>(msg_id,unique_id, thread_id, ChatFormType::PRIVATE,
                static_cast<ChatMsgType>(msg_type), msg_content, sender, status, chat_time);
            chat_datas.push_back(chat_data);
        }

        //发送信号通知界面
        emit sig_chat_msg_rsp(thread_id, chat_datas);

      });

    _handlers.insert(ID_NOTIFY_TEXT_CHAT_MSG_REQ, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Notify Chat Msg Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Notify Chat Msg Failed, err is " << err;
            return;
        }

        qCDebug(lcNetwork) << "Receive Text Chat Notify Success " ;

        //收到消息后转发给页面
        auto thread_id = jsonObj["thread_id"].toInt();
        auto sender = jsonObj["fromuid"].toInt();


        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            auto msg_id = data["message_id"].toInt();
            auto unique_id = data["unique_id"].toString();
            auto msg_content = data["content"].toString();
            QString chat_time = data["chat_time"].toString();
            int status = data["status"].toInt();
            int msg_type = data["msg_type"].toInt();
            if (msg_type < 0 || msg_type > 2) { msg_type = 0; }
            auto chat_data = std::make_shared<TextChatData>(msg_id, unique_id, thread_id, ChatFormType::PRIVATE,
                static_cast<ChatMsgType>(msg_type), msg_content, sender, status, chat_time);
            chat_datas.push_back(chat_data);
        }


        emit sig_text_chat_msg(chat_datas);
      });

    _handlers.insert(ID_NOTIFY_OFF_LINE_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Notify Chat Msg Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Notify Chat Msg Failed, err is " << err;
            return;
        }

        auto uid = jsonObj["uid"].toInt();
        qCDebug(lcNetwork) << "Receive offline Notify Success, uid is " << uid ;
        //断开连接
        //并且发送通知到界面
        emit sig_notify_offline();

    });

    _handlers.insert(ID_HEARTBEAT_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "Heart Beat Msg Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "Heart Beat Msg Failed, err is " << err;
            return;
        }

        qCDebug(lcNetwork) << "Receive Heart Beat Msg Success" ;

    });


    _handlers.insert(ID_LOAD_CHAT_THREAD_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "chat thread json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "get chat thread rsp failed, error is " << err;
            // ★ 即使失败也要把信号发出去（load_more=false，列表为空）。
            //   ChatDialog 靠这个信号的到达给加载请求计数 -1；
            //   这里直接 return 的话计数永远减不到 0，模态 LoadingDlg
            //   就一直挡着窗口 —— 表现就是「无限加载 + 程序卡死 + 无法退出」。
            emit sig_load_chat_thread(false, 0, {});
            return;
        }

        qCDebug(lcNetwork) << "Receive chat thread rsp Success";

        auto thread_array = jsonObj["threads"].toArray();
        std::vector<std::shared_ptr<ChatThreadInfo>> chat_threads;
        for (const QJsonValue& value : thread_array) {
            auto cti = std::make_shared<ChatThreadInfo>();
            cti->_thread_id = value["thread_id"].toInt();
            cti->_type = value["type"].toString();
            cti->_user1_id = value["user1_id"].toInt();
            cti->_user2_id = value["user2_id"].toInt();
            chat_threads.push_back(cti);
        }

        bool load_more = jsonObj["load_more"].toBool();
        int next_last_id = jsonObj["next_last_id"].toInt();
        //发送信号通知界面
        emit sig_load_chat_thread(load_more, next_last_id, chat_threads);
    });


    _handlers.insert(ID_CREATE_PRIVATE_CHAT_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "parse create private chat json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "get create private chat failed, error is " << err;
            return;
        }

        qCDebug(lcNetwork) << "Receive create private chat rsp Success";

        int uid = jsonObj["uid"].toInt();
        int other_id = jsonObj["other_id"].toInt();
        int thread_id = jsonObj["thread_id"].toInt();

        //发送信号通知界面
        emit sig_create_private_chat(uid, other_id, thread_id);
        });



    _handlers.insert(ID_LOAD_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        //qCDebug(lcNetwork) << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qCDebug(lcNetwork) << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qCDebug(lcNetwork) << "parse create private chat json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qCDebug(lcNetwork) << "load chat msg rsp failed, error is " << err;
            // ★ 失败也要 emit，ChatDialog 靠信号到达给加载计数 -1。
            //   直接 return 的话计数永远减不到 0，模态 LoadingDlg 一直
            //   挡着窗口，用户点什么都无效、连关闭都不行。
            //   load_more 传 false 表示「到此为止」，不再继续翻页。
            emit sig_load_chat_msg(jsonObj["thread_id"].toInt(),
                                   jsonObj["last_message_id"].toInt(),
                                   false, {});
            return;
        }

        qCDebug(lcNetwork) << "Receive load chat msg rsp Success";

        int thread_id = jsonObj["thread_id"].toInt();
        int last_msg_id = jsonObj["last_message_id"].toInt();
        bool load_more = jsonObj["load_more"].toBool();

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            QString chat_time = data["chat_time"].toString();
            int status = data["status"].toInt();
            int msg_type = data["msg_type"].toInt();
            if (msg_type < 0 || msg_type > 2) { msg_type = 0; }
            auto chat_data = std::make_shared<TextChatData>(msg_id, thread_id, ChatFormType::PRIVATE,
                static_cast<ChatMsgType>(msg_type), msg_content, send_uid, status,chat_time);
            chat_datas.push_back(chat_data);
        }

        //发送信号通知界面
        emit sig_load_chat_msg(thread_id, last_msg_id, load_more, chat_datas);
        });
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
   auto find_iter =  _handlers.find(id);
   if(find_iter == _handlers.end()){
        qCDebug(lcNetwork) << "not found id ["<< id << "] to handle";
        return ;
   }

   find_iter.value()(id,len,data);
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qCDebug(lcNetwork) << "receive tcp connect signal";
    // 开始一次新连接，复位"主动关闭"标记，保证这次连接若被服务端掐掉能正常弹提示
    _b_intentional_close = false;
    // ★ 原样留着这次的目标地址：断线重连时直接复用它，
    //   不用再走一遍"问 StatusServer 要地址"的流程（那时候服务端可能也正抖）。
    _last_server_info = si;
    // 尝试连接到服务器
    qCDebug(lcNetwork) << "Connecting to server...";
    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _socket.connectToHost(si.Host, _port);
}

void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes)
{
    uint16_t id = reqId;

    // 计算长度（使用网络字节序转换）
    quint16 len = static_cast<quint16>(dataBytes.length());

    // 创建一个QByteArray用于存储要发送的所有数据
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    // 设置数据流使用网络字节序
    out.setByteOrder(QDataStream::BigEndian);

    // 写入ID和长度
    out << id << len;

    // 添加字符串数据
    block.append(dataBytes);

    // 发送数据
    _socket.write(block);
    //qCDebug(lcNetwork) << "tcp mgr send byte data is " << block ;
}


