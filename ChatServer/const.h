#pragma once
#include <functional>


enum ErrorCodes {
	Success = 0,
	// NOTE: 1012~1016 are reserved by GateServer (DbErr = 1012 etc.);
	// ChatServer-specific codes start at 1017 so the same numeric value
	// never means two different things across services.

	Error_Json = 1001,  //Json解析错误
	RPCFailed = 1002,  //RPC请求错误
	VarifyExpired = 1003, //验证码过期
	VarifyCodeErr = 1004, //验证码错误
	UserExist = 1005,       //用户已经存在
	PasswdErr = 1006,    //密码错误
	EmailNotMatch = 1007,  //邮箱不匹配
	PasswdUpFailed = 1008,  //更新密码失败
	PasswdInvalid = 1009,   //密码更新失败
	TokenInvalid = 1010,   //Token失效
	UidInvalid = 1011,  //uid无效
	CREATE_CHAT_FAILED = 1017, //创建聊天失败
	LOAD_CHAT_FAILED = 1018,
	Error_MySql = 1019, //database error //加载聊天失败
	CREATE_GROUP_FAILED = 1020,      //创建群聊失败
	ADD_GROUP_MEMBER_FAILED = 1021,  //添加群成员失败
};


// Defer类
class Defer {
public:
	// 接受一个lambda表达式或者函数指针
	Defer(std::function<void()> func) : func_(func) {}

	// 析构函数中执行传入的函数
	~Defer() {
		func_();
	}

private:
	std::function<void()> func_;
};

#define MAX_LENGTH  1024*2

// ★ 协议头长度字段是 2 字节，所以单个响应体上限 65535。
//   超过这个数就装不进协议了（会被截断成半截 JSON）。
#define MAX_MSG_BODY_LEN 65535
// 软上限：留一点余量给序列化开销的估算误差
#define MSG_BODY_SAFE 60000
//头部总长度
#define HEAD_TOTAL_LEN 4
//头部id长度
#define HEAD_ID_LEN 2
//头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000


enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登陆
	MSG_CHAT_LOGIN_RSP = 1006, //用户登陆回包
	ID_SEARCH_USER_REQ = 1007, //用户搜索请求
	ID_SEARCH_USER_RSP = 1008, //搜索用户回包
	ID_ADD_FRIEND_REQ = 1009, //申请添加好友请求
	ID_ADD_FRIEND_RSP  = 1010, //申请添加好友回复
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,  //通知用户添加好友申请
	ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求
	ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015, //通知用户认证好友申请
	ID_TEXT_CHAT_MSG_REQ = 1017, //文本聊天信息请求
	ID_TEXT_CHAT_MSG_RSP = 1018, //文本聊天信息回复
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息
	ID_NOTIFY_OFF_LINE_REQ = 1021, //通知用户下线
	ID_HEART_BEAT_REQ = 1023,      //心跳请求
	ID_HEARTBEAT_RSP = 1024,       //心跳回复
	ID_LOAD_CHAT_THREAD_REQ = 1025, //加载聊天线程请求
	ID_LOAD_CHAT_THREAD_RSP = 1026, //加载聊天线程回复
	ID_CREATE_PRIVATE_CHAT_REQ = 1027, //创建私聊请求
	ID_CREATE_PRIVATE_CHAT_RSP = 1028, //创建私聊回复

	ID_LOAD_CHAT_MSG_REQ = 1029,      //加载聊天消息
	ID_LOAD_CHAT_MSG_RSP = 1030,
	ID_UPDATE_FRIEND_BACK_REQ = 1031, //modify friend remark request
	ID_UPDATE_FRIEND_BACK_RSP = 1032, //modify friend remark response
	// ---- group chat ----
	ID_CREATE_GROUP_CHAT_REQ = 1033,  //创建群聊请求
	ID_CREATE_GROUP_CHAT_RSP = 1034,  //创建群聊回复
	ID_ADD_GROUP_MEMBER_REQ = 1035,   //添加群成员请求
	ID_ADD_GROUP_MEMBER_RSP = 1036,   //添加群成员回复
	ID_NOTIFY_ADD_GROUP_REQ = 1037,  //群成员变更实时通知（建群/加人后推给被邀请成员）
      //加载聊天消息
};

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"

//用户资料缓存的有效期（秒）。到点自动过期，避免改了数据库后缓存挡住新值。
const int USER_BASE_INFO_TTL_SEC = 3600;
#define LOGIN_COUNT  "logincount"
#define NAME_INFO  "nameinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"

//分布式锁的持有时间
#define LOCK_TIME_OUT 10
//分布式锁的重试时间
#define ACQUIRE_TIME_OUT 5


