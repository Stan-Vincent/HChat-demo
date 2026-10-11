#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VarifyGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"
//�� ͼƬ�ϴ��õ���ע�� GateServer �ǡ��� C++ ��Ŀ��������û�� Qt ��������
//  ��������ֻ���ñ�׼�� + boost::filesystem��beast �����ʹ� boost����
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

// base64 ���롣Qt �� QByteArray::fromBase64 �������ò��ˣ�û�� Qt����
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
        // ��������/�ո�ȷ� base64 �ַ�������;�п��ܱ����У�
        if (c == '=' ) break;
        int d = rev[c];
        if (d < 0) {
            // �� ֻ�����հ��ַ���base64 �ڴ���;�п��ܱ����У���
            //   �����κηǷ��ַ���ֱ���ж�����ʧ�� ���� ���� "!!!not-base64!!!"
            //   �ᱻ���� ! ��� notbase64 ����������������������ݱ���ͼƬ���̡�
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

// ���� FNV-1a ��ϣ��ȡʮ�����ơ������ڸ��ϴ��ļ�����Ψһ���������ڰ�ȫУ�顣
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
        ///���������� _request.body() �� dynamic_body��multi_buffer����
        //buffers_to_string ����ת�� std::string
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;

        //������Ӧ����ΪJson
        connection->_response.set(http::field::content_type, "text/json");

        //����Json
        Json::Value root;   
        Json::Reader reader;    //�������
        Json::Value src_root;   //������ĸ�����
        bool parse_success = reader.parse(body_str, src_root);  //��ʾ�Ƿ�ɹ�

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
        //��Json����ȡ��email
        auto email = src_root["email"].asString();

        //���� gRPC ����,�� VarifyServer ͨ�� gRPC ͨ��
        GetVarifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVarifyCode(email);

        //�� gRPC ���صĴ������ email д�� JSON,���л���д����Ӧ body��
        std::cout << "email is " << email << std::endl;
        root["error"] = rsp.error();
        root["email"] = src_root["email"];
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

    //ע���û��߼�
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

        //�Ȳ���redis��email��Ӧ����֤���Ƿ����
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

        //�������ݿ��ж��û��Ƿ����
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

    //���ûص��߼�
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

        //�Ȳ���redis��email��Ӧ����֤���Ƿ����
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
        //��ѯ���ݿ��ж��û����������Ƿ�ƥ��
        bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
        if (!email_valid) {
            std::cout << " user email not match" << std::endl;
            root["error"] = ErrorCodes::EmailNotMatch;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //��������Ϊ��������
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

    //�û���¼�߼�
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

        //��ѯ���ݿ��ж��û����������Ƿ�ƥ��
        bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
        if (!pwd_valid) {
            std::cout << " user pwd not match" << std::endl;
            root["error"] = ErrorCodes::PasswdInvalid;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //��ѯStatusServer�ҵ����ʵ�����
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

    //�� ͼƬ�ϴ���body �� {uid, file_name, data(base64)}������ {error, url, unique_name, size, md5}
    //
    //  ��Ϊʲô���ǽ̳���� ResourceServer���̳� day39 �õ��Ƕ�����Դ������ + �ϵ����� +
    //  gRPC ֪ͨ ChatServer���������� GateServer �е������ɫ�����ֶ�����
    //  ��unique_name / md5 / total_size���Ѿ�����̵̳� MsgInfo��
    //  �Ժ󻻳� ResourceServer ʱ�ͻ���ֻ��� URL ǰ׺��
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

        // �� base64 ���뺯���ڲ��Ѿ�����������/�ո񣨴���;�п��ܱ����У�
        auto raw = base64Decode(b64);
        if (raw.empty()) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "base64 decode failed";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        // �� ����˱����Լ���һ�δ�С��
        //   �ͻ��� UploadManager �� 5MB ����ֻ�ǿͻ��˵ģ�
        //   ֱ�� POST �����ƹ� ���� ����������������д�������ļ���
        //   �ڽ���֮���жϣ�base64 �� 4/3 ���ͣ���
        //   raw.size() ��������Ҫ���̵��ֽ�����
        const std::size_t MAX_UPLOAD_BYTES = 5 * 1024 * 1024;   // 5MB
        if (raw.size() > MAX_UPLOAD_BYTES) {
            root["error"] = ErrorCodes::Error_Json;
            root["msg"] = "file too large (max 5MB)";
            beast::ostream(connection->_response.body()) << root.toStyledString();
            return true;
        }

        // ֻ�ϳ���ͼƬ��׺�����ÿͻ��˾�������˴�ʲô����
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

        // unique_name = ʱ���_���ݹ�ϣ.��׺
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "%lld", (long long)now);
        auto unique_name = std::string(stamp) + "_" + contentHash(raw) + ext;

        // �浽������Ŀ¼���µ� upload/ �ļ��У��������ļ�����Ŀ¼���� exe λ�ã�
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
            // �� �� wstring()��Windows �� path.string() ����խ�ַ�(ANSI)��
            //   ·��������ʱ ofstream ��򲻿��ļ�
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
        root["url"] = std::string("upload/") + unique_name;  // ���·�����ͻ����Լ�ƴ host
        root["unique_name"] = unique_name;
        root["size"] = (Json::Int64)raw.size();
        root["md5"] = contentHash(raw);
        std::cout << "upload_image ok: " << unique_name
                  << " size=" << raw.size() << std::endl;
        beast::ostream(connection->_response.body()) << root.toStyledString();
        return true;
        });

    //�� �����û����ϣ�body �� {uid, nick, desc, sex, icon}
    //  name���û������ǵ�¼��ʶ���������޸ģ�����ֻ�� uid ���¡�
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

        // �ǳ��޳� 21����ע��ҳһ�£�������ǩ���޳� 200
        // �� std::string �� substr������ QString::left ���� ������û�� Qt ������
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
            // �����µ����ϻظ��ͻ��ˣ��ý���������ˢ��
            root["uid"] = uid;
            root["nick"] = nick;
            root["desc"] = desc;
            root["sex"] = sex;
            root["icon"] = icon;
            std::cout << "update_userinfo ok, uid=" << uid << std::endl;

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

//�� ��̬��Դǰ׺���ϴ���ͼƬ���� exe ͬ���� upload/ �£�
//  ͨ�� GET /upload/<�ļ���> ȡ�ء��ļ��Ƕ�̬���֣�û���� RegGet ��ȷƥ�䣬
//  ������������ǰ׺ƥ�䣨�������� HttpConnection ��ͨ�÷ַ�����ģ���
static const std::string UPLOAD_PREFIX = "/upload/";
static std::string uploadDir() {
    return (boost::filesystem::absolute(boost::filesystem::current_path() / "config.ini")
        .parent_path() / "upload").string();
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con) {
    // �� �ȴ�����̬��Դ��GET /upload/<file_name>
    //   �� ��ȫ�����뵲ס ".."������ /upload/../../config.ini �ܰ������ļ�����ȥ��
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
        // �� boost::filesystem û�����ɺ��� extension()��
        //   ���� path �ĳ�Ա����������Ҫ�ȹ���һ�� path��
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

        // �������ļ����� HttpConnection �ĳ�Ա������
        //   �� �����þֲ�������beast �� body ֻ�Ǳ���ָ�룬
        //   handler һ�������ݾ�û�ˣ��ͻ��˻��յ����롣
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

        // �� �� body ��д�����ƣ�Boost 1.92 �� dynamic_body ���� multi_buffer��
        //   ��û�� append(ConstBufferSequence) / net_assign ��Щ 1.66 ʱ������ API��
        //
        // ��� prepare() ������ǡ���д���ռ䣬�� buffer_copy ����ȥ�������� ����
        //   ������ commit(n) �ѿ�д��ת�ɡ��ɶ��������� body().size() ���� 0��
        //   WriteResponse() ��� content_length(body().size()) �ͻ�д�� 0��
        //   �ͻ����յ�һ������Ϊ 0 ����Ӧ��
        auto& mb = con->_response.body();
        mb.clear();
        {
            auto dst = mb.prepare(con->_file_body.size());
            boost::asio::buffer_copy(dst, boost::asio::buffer(con->_file_body));
            mb.commit(con->_file_body.size());   // �� �ؼ�����д -> �ɶ�
        }
    }

    // �� ���� return true������ fallthrough ������� _get_handlers.find(path)��
    //   �ϴ��ļ����Ƕ�̬�ģ�ʱ���_��ϣ.png������Զ���� _get_handlers �
    //   find һ��ʧ�� -> return false��HandleReq ���ǰѽ�����ǳ� 404��
    //   ����ͬһ�� body ׷�� "url not found"��body û�� clear����
    //   �ͻ����õ��ľ��ǡ�404 + ͼƬ + �����ı�����ͼƬҲ���ˡ�
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
