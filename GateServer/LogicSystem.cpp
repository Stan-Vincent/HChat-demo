#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VarifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"
//★ 图片上传用到。注意 GateServer 是【纯 C++ 项目，工程里没有 Qt 依赖】，
//  所以这里只能用标准库 + boost::filesystem（beast 本身就带 boost）。
#include <boost/filesystem.hpp>
#include <array>
#include <fstream>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>

// base64 解码。Qt 的 QByteArray::fromBase64 在这里用不了（没有 Qt）。
static std::string base64Decode(const std::string& in) {
    static const std::string tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<int, 256> rev{};
    rev.fill(-1);
    for (int i = 0; i < 64; ++i) {
        rev[static_cast<unsigned char>(tbl[i])] = i;
    }
    std::string out;
    out.reserve(in.size() * 3 / 4);
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        // 跳过换行/空格等非 base64 字符（传输途中可能被折行）
        if (c == '=' ) break;
        int d = rev[c];
        if (d < 0) {
            // ★ 只允许空白字符（base64 在传输途中可能被折行）。
            //   其他任何非法字符都直接判定解码失败 —— 否则 "!!!not-base64!!!"
            //   会被跳过 ! 后把 notbase64 正常解出来，导致垃圾数据被当图片存盘。
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                return std::string();
            }
            continue;
        }
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// 轻量 FNV-1a 哈希，取十六进制。仅用于给上传文件生成唯一名，不用于安全校验。
static std::string contentHash(const std::string& data) {
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

LogicSystem::~LogicSystem()
{
}

///typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;
void LogicSystem::RegGet(std::string url, HttpHandler handler) {
    _get_handlers.insert(make_pair(url, handler));
}
void LogicSystem::RegPost(std::string url, HttpHandler handler) {
    _post_handlers.insert(make_pair(url, handler));
}

LogicSystem::LogicSystem() {

    RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->_response.body()) << "receive get_test req."<<std::endl;
        int i = 0;
        for (auto& elem : connection->_get_params) {
            i++;
            beast::ostream(connection->_response.body()) << "param(" << i << ")'s key is "<<elem.first<<"\n";
            beast::ostream(connection->_response.body()) << "param(" << i << ")'s value is " << elem.second <<"\n" << std::endl;
        }
    });

    RegPost("/get_varifycode", [](std::shared_ptr<HttpConnection> connection) {
        ///读出请求体 _request.body() 是 dynamic_body（multi_buffer）。
        //buffers_to_string 把它转成 std::string
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;

        //设置响应类型为Json
        connection->_response.set(http::field::content_type, "text/json");

        //解析Json
        Json::Value root;   
        Json::Reader reader;    //负责解析
        Json::Value src_root;   //解析后的根对象
        bool parse_success = reader.parse(body_str, src_root);  //表示是否成功

        if (!parse_success) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true; 
        }

        if (!src_root.isMember("email")) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //从Json中提取出email
        auto email = src_root["email"].asString();

        //调用 gRPC 服务,和 VarifyServer 通过 gRPC 通信
        GetVarifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVarifyCode(email);

        //把 gRPC 返回的错误码和 email 写进 JSON,序列化后写进响应 body。
        std::cout << "email is " << email << std::endl;
        root["error"] = rsp.error();
        root["email"] = src_root["email"];
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

    //注册用户逻辑
    RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {

        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;

        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;

        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["passwd"].asString();
        auto confirm = src_root["confirm"].asString();
        auto icon = src_root["icon"].asString();

        if (pwd != confirm) {
            std::cout << "password err " << std::endl;
            root["error"] = ErrorCodes::PasswdErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //先查找redis中email对应的验证码是否合理
        std::string  varify_code;
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), varify_code);
        if (!b_get_varify) {
            std::cout << " get varify code expired" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        if (varify_code != src_root["varifycode"].asString()) {
            std::cout << " varify code error" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //查找数据库判断用户是否存在
        int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd, icon);
        // 0: name or email already exists   -1: database error (schema/connection/transaction)
        if (uid == -1) {
            std::cout << " reg user failed, mysql error" << std::endl;
            root["error"] = ErrorCodes::DbErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        if (uid == 0) {
            std::cout << " user or email exist" << std::endl;
            root["error"] = ErrorCodes::UserExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        root["error"] = 0;
        root["uid"] = uid;
        root["email"] = email;
        root["user"] = name;
        // root["passwd"] = pwd;   
        // root["confirm"] = confirm;  
        root["icon"] = icon;
        std::cout << " register success, uid = " << uid << std::endl;
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });

    //重置回调逻辑
    RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        //do NOT print the whole body here, it carries the new password

        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;

        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["passwd"].asString();
        std::cout << "reset_pwd request, user = " << name << ", email = " << email << std::endl;

        //先查找redis中email对应的验证码是否合理
        std::string  varify_code;
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), varify_code);
        if (!b_get_varify) {
            std::cout << " get varify code expired" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        if (varify_code != src_root["varifycode"].asString()) {
            std::cout << " varify code error" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //查询数据库判断用户名和邮箱是否匹配
        bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
        if (!email_valid) {
            std::cout << " user email not match" << std::endl;
            root["error"] = ErrorCodes::EmailNotMatch;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //更新密码为最新密码
        bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
        if (!b_up) {
            std::cout << " update pwd failed" << std::endl;
            root["error"] = ErrorCodes::PasswdUpFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        std::cout << "succeed to update password, user = " << name << std::endl;
        root["error"] = 0;
        root["email"] = email;
        root["user"] = name;
        //root["passwd"] = pwd;   //never echo the new password back to the client
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });

    //用户登录逻辑
    RegPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {

        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;

        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;

        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        auto pwd = src_root["passwd"].asString();

        UserInfo userInfo;

        //查询数据库判断用户名和密码是否匹配
        bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
        if (!pwd_valid) {
            std::cout << " user pwd not match" << std::endl;
            root["error"] = ErrorCodes::PasswdInvalid;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //查询StatusServer找到合适的连接
        auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
        if (reply.error()) {
            std::cout << " grpc get chat server failed, error is " << reply.error() << std::endl;
            root["error"] = ErrorCodes::RPCFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        std::cout << "succeed to load userinfo uid is " << userInfo.uid << std::endl;
        root["error"] = 0;
        root["email"] = email;
        root["uid"] = userInfo.uid;
        root["token"] = reply.token();
        root["host"] = reply.host();
        root["port"] = reply.port();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });

    //★ 图片上传：body 是 {uid, file_name, data(base64)}，返回 {error, url, unique_name, size, md5}
    //
    //  【为什么不是教程里的 ResourceServer】教程 day39 用的是独立资源服务器 + 断点续传 +
    //  gRPC 通知 ChatServer。这里先用 GateServer 承担这个角色，但字段命名
    //  （unique_name / md5 / total_size）已经对齐教程的 MsgInfo，
    //  以后换成 ResourceServer 时客户端只需改 URL 前缀。
    RegPost("/upload_image", [](std::shared_ptr<HttpConnection> connection) {

        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Value src_root;
        Json::Reader reader;

        if (!reader.parse(body_str, src_root)) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "json parse failed";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        auto file_name = src_root["file_name"].asString();
        auto b64 = src_root["data"].asString();
        if (b64.empty()) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "empty data";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        // ★ base64 解码函数内部已经会跳过换行/空格（传输途中可能被折行）
        auto raw = base64Decode(b64);
        if (raw.empty()) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "base64 decode failed";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        // 只认常见图片后缀，不让客户端决定服务端存什么类型
        std::string ext = ".jpg";
        auto dot = file_name.rfind('.');
        if (dot != std::string::npos) {
            auto e = file_name.substr(dot);
            std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) {
                return static_cast<char>(::tolower(c));
            });
            if (e == ".png" || e == ".gif" || e == ".bmp"
                || e == ".webp" || e == ".jpg" || e == ".jpeg") {
                ext = e;
            }
        }

        // unique_name = 时间戳_内容哈希.后缀
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "%lld", (long long)now);
        auto unique_name = std::string(stamp) + "_" + contentHash(raw) + ext;

        // 存到【工程目录】下的 upload/ 文件夹（用配置文件所在目录推算 exe 位置）
        auto cfg_path = boost::filesystem::current_path() / "config.ini";
        auto base_dir = boost::filesystem::absolute(cfg_path).parent_path();
        auto dir_path = base_dir / "upload";
        boost::system::error_code ec;
        boost::filesystem::create_directories(dir_path, ec);
        if (ec) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = std::string("cannot create upload dir: ") + ec.message();
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        auto abs_path = dir_path / unique_name;
        {
            // ★ 用 wstring()：Windows 上 path.string() 返回窄字符(ANSI)，
            //   路径含中文时 ofstream 会打不开文件
            std::ofstream ofs(abs_path.wstring(), std::ios::binary);
            if (!ofs) {
                root["error"] = ErrorCodes::Error_Json;
                root["msg"] = "cannot write " + abs_path.string();
                beast::ostream(connection->_response.body()) << root.toStyledString();
                return true;
            }
            ofs.write(raw.data(), static_cast<std::streamsize>(raw.size()));
            ofs.flush();
            if (!ofs) {
                root["error"] = ErrorCodes::Error_Json;
                root["msg"] = "write failed " + abs_path.string();
                beast::ostream(connection->_response.body()) << root.toStyledString();
                return true;
            }
        }

        root["error"] = ErrorCodes::Success;
        root["url"] = std::string("upload/") + unique_name;  // 相对路径，客户端自己拼 host
        root["unique_name"] = unique_name;
        root["size"] = (Json::Int64)raw.size();
        root["md5"] = contentHash(raw);
        std::cout << "upload_image ok: " << unique_name
                  << " size=" << raw.size() << std::endl;
        beast::ostream(connection->_response.body()) << root.toStyledString();
        return true;
        });

    //★ 更新用户资料：body 是 {uid, nick, desc, sex, icon}
    //  name（用户名）是登录标识，不允许修改，所以只按 uid 更新。
    RegPost("/update_userinfo", [](std::shared_ptr<HttpConnection> connection) {

        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Value src_root;
        Json::Reader reader;

        if (!reader.parse(body_str, src_root)) {
            root["error"] = ErrorCodes::Error_Json;
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        auto uid = src_root["uid"].asInt();
        if (uid <= 0) {
            root["error"] = ErrorCodes::UidInvalid;
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        auto nick = src_root["nick"].asString();
        auto desc = src_root["desc"].asString();
        auto sex = src_root["sex"].asInt();
        auto icon = src_root["icon"].asString();

        // 昵称限长 21（与注册页一致），个性签名限长 200
        // 用 std::string 的 substr，不是 QString::left —— 本工程没有 Qt 依赖。
        if (nick.length() > 21) {
            nick = nick.substr(0, 21);
        }
        if (desc.length() > 200) {
            desc = desc.substr(0, 200);
        }

        if (!MysqlMgr::GetInstance()->UpdateUserInfo(uid, nick, desc, sex, icon)) {
            root["error"] = ErrorCodes::EmailNotMatch;
            root["msg"] = "update failed";
        }
        else {
            root["error"] = ErrorCodes::Success;
            // 把最新的资料回给客户端，让界面能立刻刷新
            root["uid"] = uid;
            root["nick"] = nick;
            root["desc"] = desc;
            root["sex"] = sex;
            root["icon"] = icon;
            std::cout << "update_userinfo ok, uid=" << uid << std::endl;
        }
        beast::ostream(connection->_response.body()) << root.toStyledString();
        return true;
        });

}

//★ 静态资源前缀：上传的图片放在 exe 同级的 upload/ 下，
//  通过 GET /upload/<文件名> 取回。文件是动态名字，没法用 RegGet 精确匹配，
//  所以在这里做前缀匹配（而不是在 HttpConnection 的通用分发层里改）。
static const std::string UPLOAD_PREFIX = "/upload/";
static std::string uploadDir() {
    return (boost::filesystem::absolute(boost::filesystem::current_path() / "config.ini")
        .parent_path() / "upload").string();
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {
    // ★ 先处理静态资源：GET /upload/<file_name>
    //   ★ 安全：必须挡住 ".."，否则 /upload/../../config.ini 能把配置文件读出去。
    if (path.rfind(UPLOAD_PREFIX, 0) == 0) {
        auto file_name = path.substr(UPLOAD_PREFIX.size());
        if (file_name.find("..") != std::string::npos
            || file_name.find('/') != std::string::npos
            || file_name.find('\\') != std::string::npos
            || file_name.empty()) {
            con->_response.result(http::status::bad_request);
            beast::ostream(con->_response.body()) << "bad file name";
            return true;
        }

        auto abs_path = boost::filesystem::path(uploadDir()) / file_name;
        boost::system::error_code ec;
        if (!boost::filesystem::exists(abs_path, ec) || ec) {
            con->_response.result(http::status::not_found);
            beast::ostream(con->_response.body()) << "file not found";
            return true;
        }

        auto file_size = boost::filesystem::file_size(abs_path, ec);
        if (ec) {
            con->_response.result(http::status::internal_server_error);
            beast::ostream(con->_response.body()) << "cannot stat file";
            return true;
        }

        con->_response.result(http::status::ok);
        // ★ boost::filesystem 没有自由函数 extension()，
        //   它是 path 的成员函数，所以要先构造一个 path。
        auto ext = boost::filesystem::path(file_name).extension();
        if (ext == ".png") {
            con->_response.set(http::field::content_type, "image/png");
        }
        else if (ext == ".jpg" || ext == ".jpeg") {
            con->_response.set(http::field::content_type, "image/jpeg");
        }
        else if (ext == ".gif") {
            con->_response.set(http::field::content_type, "image/gif");
        }
        else if (ext == ".webp") {
            con->_response.set(http::field::content_type, "image/webp");
        }
        else {
            con->_response.set(http::field::content_type, "application/octet-stream");
        }
        con->_response.content_length(file_size);

        // 二进制文件读进 HttpConnection 的成员变量。
        //   ★ 不能用局部变量：beast 的 body 只是保存指针，
        //   handler 一返回数据就没了，客户端会收到乱码。
        std::ifstream ifs(abs_path.wstring(), std::ios::binary);
        if (!ifs) {
            con->_response.result(http::status::internal_server_error);
            beast::ostream(con->_response.body()) << "cannot open file";
            return true;
        }
        con->_file_body.assign((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
        if (con->_file_body.empty()) {
            con->_response.result(http::status::internal_server_error);
            beast::ostream(con->_response.body()) << "read file failed";
            return true;
        }

        // ★ 往 body 里写二进制：Boost 1.92 的 dynamic_body 就是 multi_buffer，
        //   它没有 append(ConstBufferSequence) / net_assign 这些 1.66 时代的老 API。
        //
        // ★★ prepare() 分配的是【可写】空间，光 buffer_copy 拷进去还不算完 ——
        //   必须再 commit(n) 把可写区转成【可读】，否则 body().size() 仍是 0，
        //   WriteResponse() 里的 content_length(body().size()) 就会写成 0，
        //   客户端收到一个长度为 0 的响应。
        auto& mb = con->_response.body();
        mb.clear();
        {
            auto dst = mb.prepare(con->_file_body.size());
            boost::asio::buffer_copy(dst, boost::asio::buffer(con->_file_body));
            mb.commit(con->_file_body.size());   // ★ 关键：可写 -> 可读
        }
    }

    // ★ 必须 return true，不能 fallthrough 到下面的 _get_handlers.find(path)。
    //   上传文件名是动态的（时间戳_哈希.png），永远不在 _get_handlers 里，
    //   find 一定失败 -> return false，HandleReq 于是把结果覆盖成 404，
    //   再往同一个 body 追加 "url not found"（body 没被 clear），
    //   客户端拿到的就是「404 + 图片 + 错误文本」，图片也废了。
    return true;

    if (_get_handlers.find(path) == _get_handlers.end()) {
        return false;
    }
    _get_handlers[path](con);
    return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con)
{
    if (_post_handlers.find(path) == _post_handlers.end()) {
        return false;
    }
    _post_handlers[path](con);
    return true;
}
