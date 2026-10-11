#include "LogicSystem.h"
#include "logger.h"
#include "HttpConnection.h"
#include "VarifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"
//?? ????????????? GateServer ????? C++ ?????????????? Qt ????????
//  ????????????????? + boost::filesystem??beast ??????? boost????
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

// base64 ????Qt ?? QByteArray::fromBase64 ???????ò??????? Qt????
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
        // ????????/????? base64 ???????????п???????У?
        if (c == '=' ) break;
        int d = rev[c];
        if (d < 0) {
            // ?? ?????????????base64 ???????п???????У???
            //   ?????κη???????????ж???????? ???? ???? "!!!not-base64!!!"
            //   ?????? ! ??? notbase64 ?????????????????????????????????
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

// ???? FNV-1a ???????????????????????????????Ψ?????????????У?顣
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
        ///?????????? _request.body() ?? dynamic_body??multi_buffer????
        //buffers_to_string ??????? std::string
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

        //????????????Json
        connection->_response.set(http::field::content_type, "text/json");

        //????Json
        Json::Value root;   
        Json::Reader reader;    //???????
        Json::Value src_root;   //????????????
        bool parse_success = reader.parse(body_str, src_root);  //????????

        if (!parse_success) {
            LOG_ERROR("JSON 解析失败");
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true; 
        }

        if (!src_root.isMember("email")) {
            LOG_ERROR("JSON 解析失败");
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //??Json???????email
        auto email = src_root["email"].asString();

        //???? gRPC ????,?? VarifyServer ??? gRPC ???
        GetVarifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVarifyCode(email);

        //?? gRPC ??????????? email д?? JSON,???л???д????? body??
        LOG_INFO("邮箱: {}", email);
        root["error"] = rsp.error();
        root["email"] = src_root["email"];
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

    //?????????
    RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {

        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;

        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            LOG_ERROR("JSON 解析失败");
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
            LOG_ERROR("密码错误");
            root["error"] = ErrorCodes::PasswdErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //?????redis??email????????????????
        std::string  varify_code;
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), varify_code);
        if (!b_get_varify) {
            LOG_ERROR("验证码已过期");
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        if (varify_code != src_root["varifycode"].asString()) {
            LOG_ERROR("验证码错误");
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //??????????ж??????????
        int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd, icon);
        // 0: name or email already exists   -1: database error (schema/connection/transaction)
        if (uid == -1) {
            LOG_ERROR("注册失败：数据库错误");
            root["error"] = ErrorCodes::DbErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        if (uid == 0) {
            LOG_INFO("用户名或邮箱已存在");
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
        LOG_INFO("注册成功，uid: {}", uid);
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });

    //?????????
    RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        //do NOT print the whole body here, it carries the new password

        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;

        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            LOG_ERROR("JSON 解析失败");
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["passwd"].asString();
        LOG_INFO("重置密码请求: 用户名={}, 邮箱={}", name, email);

        //?????redis??email????????????????
        std::string  varify_code;
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), varify_code);
        if (!b_get_varify) {
            LOG_ERROR("验证码已过期");
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        if (varify_code != src_root["varifycode"].asString()) {
            LOG_ERROR("验证码错误");
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //?????????ж??????????????????
        bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
        if (!email_valid) {
            LOG_ERROR("邮箱不匹配");
            root["error"] = ErrorCodes::EmailNotMatch;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //?????????????????
        bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
        if (!b_up) {
            LOG_ERROR("更新密码失败");
            root["error"] = ErrorCodes::PasswdUpFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        LOG_INFO("更新密码成功，用户名: {}", name);
        root["error"] = 0;
        root["email"] = email;
        root["user"] = name;
        //root["passwd"] = pwd;   //never echo the new password back to the client
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });

    //?????????
    RegPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {

        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

        connection->_response.set(http::field::content_type, "text/json");

        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;

        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            LOG_ERROR("JSON 解析失败");
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        auto pwd = src_root["passwd"].asString();

        UserInfo userInfo;

        //?????????ж??????????????????
        bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
        if (!pwd_valid) {
            LOG_INFO("密码不匹配");
            root["error"] = ErrorCodes::PasswdInvalid;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //???StatusServer????????????
        auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
        if (reply.error()) {
            LOG_ERROR("gRPC 获取聊天服务器失败: {}", reply.error());
            root["error"] = ErrorCodes::RPCFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        LOG_INFO("加载用户信息成功，uid: {}", userInfo.uid);
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

    //?? ???????body ?? {uid, file_name, data(base64)}?????? {error, url, unique_name, size, md5}
    //
    //  ?????????????? ResourceServer????? day39 ????????????????? + ??????? +
    //  gRPC ?? ChatServer?????????? GateServer ?е??????????????????
    //  ??unique_name / md5 / total_size???????????? MsgInfo??
    //  ???? ResourceServer ?????????? URL ????
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

        // ?? base64 ??????????????????????/????????п???????У?
        auto raw = base64Decode(b64);
        if (raw.empty()) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "base64 decode failed";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        // ?? ???????????????δ?С??
        //   ????? UploadManager ?? 5MB ?????????????
        //   ??? POST ??????? ???? ????????????????д???????????
        //   ?????????ж??base64 ?? 4/3 ???????
        //   raw.size() ?????????????????????
        const std::size_t MAX_UPLOAD_BYTES = 5 * 1024 * 1024;   // 5MB
        if (raw.size() > MAX_UPLOAD_BYTES) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "file too large (max 5MB)";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        // ????????????????????????????????????
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

        // unique_name = ????_??????.???
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "%lld", (long long)now);
        auto unique_name = std::string(stamp) + "_" + contentHash(raw) + ext;

        // ?浽????????????? upload/ ????У???????????????????? exe λ???
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
            // ?? ?? wstring()??Windows ?? path.string() ????????(ANSI)??
            //   ·????????? ofstream ???????
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
        root["url"] = std::string("upload/") + unique_name;  // ???·????????????? host
        root["unique_name"] = unique_name;
        root["size"] = (Json::Int64)raw.size();
        root["md5"] = contentHash(raw);
        LOG_INFO("上传图片成功: 文件名={}, 大小={}", unique_name, raw.size());
        beast::ostream(connection->_response.body()) << root.toStyledString();
        return true;
        });

    //?? ????????????body ?? {uid, nick, desc, sex, icon}
    //  name??????????????????????????????????? uid ?????
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

        // ?????? 21???????????????????????? 200
        // ?? std::string ?? substr?????? QString::left ???? ????????? Qt ??????
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
            // ?????????????????????????????????
            root["uid"] = uid;
            root["nick"] = nick;
            root["desc"] = desc;
            root["sex"] = sex;
            root["icon"] = icon;
            LOG_INFO("更新用户信息成功，uid: {}", uid);

            // [cache coherence] On login ChatServer reads base info from Redis key
            // ubaseinfo_<uid> (GetBaseInfo hits cache, TTL 3600s) but we only updated
            // MySQL above. Delete the key so next login reloads fresh data from MySQL,
            // otherwise the user sees the old nick/desc/sex/icon ("update not saved").
            std::string base_key = "ubaseinfo_" + std::to_string(uid);
            RedisMgr::GetInstance()->Del(base_key);
        }
        beast::ostream(connection->_response.body()) << root.toStyledString();
        return true;
        });

}

//?? ????????????????????? exe ????? upload/ ???
//  ??? GET /upload/<?????> ????????????????????? RegGet ??????
//  ????????????????????????? HttpConnection ????÷??????????
static const std::string UPLOAD_PREFIX = "/upload/";
static std::string uploadDir() {
    return (boost::filesystem::absolute(boost::filesystem::current_path() / "config.ini")
        .parent_path() / "upload").string();
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {
    // ?? ?????????????GET /upload/<file_name>
    //   ?? ?????????? ".."?????? /upload/../../config.ini ?????????????????
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
        // ?? boost::filesystem ?????????? extension()??
        //   ???? path ??????????????????????? path??
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

        // ????????????? HttpConnection ??????????
        //   ?? ??????????????beast ?? body ?????????
        //   handler ???????????????????????????
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

        // ?? ?? body ??д???????Boost 1.92 ?? dynamic_body ???? multi_buffer??
        //   ????? append(ConstBufferSequence) / net_assign ??Щ 1.66 ??????? API??
        //
        // ??? prepare() ??????????д??????? buffer_copy ????????????? ????
        //   ?????? commit(n) ???д????ɡ???????????? body().size() ???? 0??
        //   WriteResponse() ??? content_length(body().size()) ???д?? 0??
        //   ???????????????? 0 ???????
        auto& mb = con->_response.body();
        mb.clear();
        {
            auto dst = mb.prepare(con->_file_body.size());
            boost::asio::buffer_copy(dst, boost::asio::buffer(con->_file_body));
            mb.commit(con->_file_body.size());   // ?? ???????д -> ???
        }
    }

    // ?? ???? return true?????? fallthrough ??????? _get_handlers.find(path)??
    //   ???????????????????_???.png??????????? _get_handlers ??
    //   find ?????? -> return false??HandleReq ???????????? 404??
    //   ???????? body ??? "url not found"??body ??? clear????
    //   ??????????????404 + ?? + ???????????????????
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
