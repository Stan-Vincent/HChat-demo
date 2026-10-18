#ifndef TCPMGR_H
#define TCPMGR_H
#include <QTcpSocket>
#include <QTimer>
#include "singleton.h"
#include "global.h"
#include <functional>
#include <QObject>
#include "userdata.h"
#include <QJsonArray>
#include <memory>

// ★ 自定义 TCP 协议：2 字节大端 msg_id + 2 字节大端 msg_len + body
//   与服务端 MsgNode.cpp 的 SendNode 保持一致（host_to_network_short = 大端）。
static const int HEAD_TOTAL_LEN = 4;
// 消息体长度上限。服务端 CSession 的 _data 是 MAX_LENGTH(2048)，
// 但历史消息一页 10 条很容易超过 2048 字节，这里放宽到 64KB
// （正好是 2 字节长度字段能表示的最大值 65535）。
static const int MAX_MSG_BODY_LEN = 65535;

// ---- 断线自动重连参数 ----
// 网络抖动 / 服务端重启时用户不该被踢回登录页。被动断开后自动重连，
// 指数退避（1s/2s/4s/8s/8s）最多试 kMaxReconnectAttempts 次，
// 全失败才让界面回登录页。单次 connectToHost 若 kConnectWatchdogMs 内
// 没有 connected 也没有报错，就放弃这次重试（防 Connecting 卡死）。
static const int kMaxReconnectAttempts = 5;
static const int kConnectWatchdogMs  = 4000;

class TcpMgr:public QObject, public Singleton<TcpMgr>,
        public std::enable_shared_from_this<TcpMgr>
{
    Q_OBJECT
public:
   ~ TcpMgr();
    void CloseConnection();
    // ---- 断线自动重连 ----
    // 被动断开（服务端掐线 / 网络抖动）时由 disconnected 处理器自动调用；
    // 重连成功会重放一次聊天登录握手（token 还在 UserMgr 里）。
    void StartReconnect();
    void StopReconnect();
    bool IsReconnecting() const;
    // 供 UI 判断"现在能不能发消息"（断线期间发送只会静默丢失）
    bool IsConnected() const;
private:
    friend class Singleton<TcpMgr>;
    TcpMgr();
    void initHandlers();
    void handleMsg(ReqId id, int len, QByteArray data);
    // 退避到下一次重连；attempt 已达上限则发 sig_reconnect_failed
    void scheduleNextAttempt();
    QTcpSocket _socket;
    QString _host;
    uint16_t _port;
    QByteArray _buffer;
    bool _b_recv_pending;
    // ★ 主动关闭标记：CloseConnection() 置 true，disconnected 信号据此决定是否
    //   转发 sig_connection_closed（心跳超时提示框）。用来区分"用户主动退出登录"
    //   和"服务端真的把连接掐了"这两种断开。
    bool _b_intentional_close;
    quint16 _message_id;
    quint16 _message_len;
    QMap<ReqId, std::function<void(ReqId id, int len, QByteArray data)>> _handlers;
    // ---- 重连状态 ----
    // 首次连接用的 ServerInfo 原样留着，重连时直接复用（不用再问 StatusServer）
    ServerInfo _last_server_info;
    bool _b_reconnecting;
    int  _reconnect_attempt;
    QTimer *_reconnect_timer;    // 退避计时器（单次）
    QTimer *_connect_watchdog;   // 单次 connectToHost 的超时看门狗
public slots:
    void slot_tcp_connect(ServerInfo);
    void slot_send_data(ReqId reqId, QByteArray data);
    // 退避时间到：真正发起第 N 次连接
    void slot_reconnect_timeout();
    // 看门狗超时：这次连接卡死了，放弃并排下一次
    void slot_connect_watchdog_timeout();
signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(ReqId reqId, QByteArray data);
    void sig_swich_chatdlg();
    void sig_load_apply_list(QJsonArray json_array);
    void sig_login_failed(int);
    void sig_user_search(std::shared_ptr<SearchInfo>);
    // 建群回包（ID_CREATE_GROUP_CHAT_RSP=1034）：error=0 成功并带 thread_id/name，
    // 失败带错误码（1011 uid 无效 / 1015 建群失败），thread_id 为 0。
    void sig_create_group_rsp(int error, int thread_id, QString name);
    void sig_friend_apply(std::shared_ptr<AddFriendApply>);
    void sig_add_auth_friend(std::shared_ptr<AuthInfo>);
    void sig_auth_rsp(std::shared_ptr<AuthRsp>);
    void sig_text_chat_msg(std::vector<std::shared_ptr<TextChatData>> msg_list);
    void sig_notify_offline();
    void sig_connection_closed();
    // ---- 重连相关信号 ----
    // 每次真正发起重试前发出（attempt 从 1 开始，total = kMaxReconnectAttempts）
    void sig_reconnect_retry(int attempt, int total);
    // 重连 + 重新登录握手成功：界面只提示"已恢复"并刷新数据，
    // 【不要】重放 sig_swich_chatdlg（那会重放登录切页动画、重复加载会话列表）
    void sig_recon_success();
    // 重试耗尽 / 重连时登录被拒：调用方应回登录页
    void sig_reconnect_failed();
    void sig_load_chat_thread(bool load_more, int last_thread_id, 
        std::vector<std::shared_ptr<ChatThreadInfo>> chat_list);
    void sig_create_private_chat(int uid, int other_id, int thread_id);
    void sig_update_friend_back(bool ok, int friend_uid, QString back);
    void sig_load_chat_msg(int thread_id, int message_id, bool load_more,
        std::vector<std::shared_ptr<TextChatData>> msg_list);

    void sig_chat_msg_rsp(int thread_id, std::vector<std::shared_ptr<TextChatData>> msg_list);
    // 聊天消息回包整体失败（err != SUCCESS）时发出，供界面把待回执消息标记成发送失败
    void sig_chat_msg_rsp_error(int err);
};

#endif // TCPMGR_H
