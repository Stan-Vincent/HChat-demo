# -*- coding: utf-8 -*-
"""
HChat 后端日志中文化 + 清理 std::cout 机械转换残留的重复参数。
- 按文件探测编码读取（BOM->utf-8-sig, utf-8 试解, 否则 gbk）
- 统一写回无 BOM GBK（后端主体是 GBK，统一后 MSVC 按系统代码页正确解析中文，
  fmt 输出的中文字节在 GBK 控制台也能正常显示）
- STR:  格式串替换（参数不变）
- FULL: 完整语句替换（含参数去重）
- Redis 的 "Execut command [ CMD ..." 系列用正则统一重写
用法：python cn_logs.py [--apply]   (不带 --apply 只预览命中统计)
"""
import glob
import re
import sys

APPLY = '--apply' in sys.argv

# ---------- 格式串替换（含引号，参数不变） ----------
STR = [
    # AsioIOServicePool / CServer
    ('"AsioIOServicePool destruct"', '"AsioIOServicePool 析构"'),
    ('"CServer Async_accept...\\n"', '"开始异步接受连接..."'),
    ('"Async_wait error.\\n\\n"', '"异步等待出错"'),
    ('"timer error:  {}"', '"定时器错误: {}"'),
    ('"~CSession destruct"', '"CSession 析构"'),
    # ConfigMgr
    ('"Config path:  {}"', '"配置文件路径: {}"'),
    ('"[section {}]"', '"[配置节 {}]"'),
    ('"{} = {}"', '"配置项 {} = {}"'),
    # GateServer 入口
    ('"received signal {}, shutting down..."', '"收到信号 {}，正在关闭..."'),
    ('"GateServer started, listening on port {}"', '"网关服务已启动，监听端口 {}"'),
    ('"GateServer exited gracefully"', '"网关服务正常退出"'),
    ('"GateServer fatal error: {}"', '"网关服务致命错误: {}"'),
    # HttpConnection
    ('"HttpConnection Async_read...\\n"', '"开始异步读取 HTTP 请求..."'),
    ('"request body too large (>=8MB), rejected"', '"请求体过大（>=8MB），已拒绝"'),
    ('"Http read err is  {}"', '"HTTP 读取错误: {}"'),
    ('"HttpConnection Exception is  {}"', '"HTTP 连接异常: {}"'),
    ('"HttpConnection::HandleReq() success...\\n\\n"', '"处理 GET 请求成功"'),
    ('"HttpConnection::HandlePost() success...\\n\\n"', '"处理 POST 请求成功"'),
    ('"Async_write...(HttpConnection:Start())\\n\\n"', '"异步写（连接启动）"'),
    # GateServer LogicSystem（登录注册/重置密码/用户信息）
    ('"Failed to parse JSON data!"', '"JSON 解析失败"'),
    ('"email is  {}"', '"邮箱: {}"'),
    ('"password err "', '"密码错误"'),
    ('" get varify code expired"', '"验证码已过期"'),
    ('" varify code error"', '"验证码错误"'),
    ('" reg user failed, mysql error"', '"注册失败：数据库错误"'),
    ('" user or email exist"', '"用户名或邮箱已存在"'),
    ('" register success, uid =  {}"', '"注册成功，uid: {}"'),
    ('" user email not match"', '"邮箱不匹配"'),
    ('" update pwd failed"', '"更新密码失败"'),
    ('"succeed to update password, user =  {}"', '"更新密码成功，用户名: {}"'),
    ('" user pwd not match"', '"密码不匹配"'),
    ('" grpc get chat server failed, error is  {}"', '"gRPC 获取聊天服务器失败: {}"'),
    ('"succeed to load userinfo uid is  {}"', '"加载用户信息成功，uid: {}"'),
    ('"update_userinfo ok, uid= {}"', '"更新用户信息成功，uid: {}"'),
    # MysqlDao
    ('"mysql pool init failed, error is  {}"', '"MySQL 连接池初始化失败: {}"'),
    ('"mysql pool init threw:  {}"', '"MySQL 连接池初始化异常: {}"'),
    ('"mysql pool init threw unknown exception"', '"MySQL 连接池初始化抛出未知异常"'),
    ('"Error keeping connection alive:  {}"', '"保活连接出错: {}"'),
    ('"mysql connection reconnect success"', '"MySQL 连接重连成功"'),
    ('"Reconnect failed, error is  {}"', '"重连失败: {}"'),
    ('"Result:  {}"', '"查询结果: {}"'),
    ('"select id from user_id failed"', '"查询 user_id 自增表失败"'),
    ('"newuser insert into user success"', '"新用户写入 user 表成功"'),
    ('"Check Email:  {}"', '"校验邮箱: {}"'),
    ('"Updated rows:  {}"', '"更新行数: {}"'),
    ('"UpdateUserInfo rows:  {}"', '"更新用户信息行数: {}"'),
    ('"verifying password for email={} (origin pwd hidden)"', '"校验邮箱 {} 的密码（原始密码已隐藏）"'),
    ('"verifying password (origin pwd hidden)"', '"校验密码（原始密码已隐藏）"'),
    ('"uid:  {}"', '"uid: {}"'),
    ('"name:  {}"', '"用户名: {}"'),
    ('"check email exists: {}"', '"邮箱 {} 已存在"'),
    ('"addfriend insert message success"', '"好友申请消息写入成功"'),
    ('"addfriend insert friends success"', '"好友关系写入成功"'),
    # CSession（Chat/Chat2）
    ('"handle read failed, error is  {}"', '"读取失败: {}"'),
    ('"Exception code is  {}"', '"异常: {}"'),
    ('"msg_id is  {}"', '"消息ID: {}"'),
    ('"invalid msg_id is  {}"', '"非法消息ID: {}"'),
    ('"msg_len is  {}"', '"消息长度: {}"'),
    ('"invalid data length is  {}"', '"非法数据长度: {}"'),
    ('"handle write failed, error is  {}"', '"写入失败: {}"'),
    ('"heartbeat expired, session id is   {}"', '"心跳超时，会话ID: {}"'),
    # 服务入口
    ('"RPC Server listening on  {}"', '"RPC 服务已监听: {}"'),
    ('"ChatServer exited gracefully"', '"聊天服务正常退出"'),
    ('"ChatServer2 exited gracefully"', '"聊天服务2正常退出"'),
    ('"Server listening on  {}"', '"服务已监听: {}"'),
    ('"Shutting down server..."', '"正在关闭服务..."'),
    ('"StatusServer fatal error: {}"', '"状态服务致命错误: {}"'),
    ('"StatusServer exited gracefully"', '"状态服务正常退出"'),
    # LogicSystem（Chat/Chat2）
    ('"chat login request, uid={}, token=[redacted]"', '"聊天登录请求: uid={}, token=[已隐藏]"'),
    ('"user SearchInfo uid is"', '"搜索用户信息"'),
    ('"SearchInfo: empty search string"', '"搜索字符串为空"'),
    ('"content is  {}"', '"消息内容: {}"'),
    ('"unique_id is  {}"', '"unique_id: {}"'),
    ('"msg_type is  {}"', '"消息类型: {}"'),
    ('"receive heart beat msg, uid is  {}"', '"收到心跳，uid: {}"'),
    ('"GetUserByEmail: not found, email is  {}"', '"按邮箱查询未找到: {}"'),
    ('"get uid  threads   {}"', '"获取uid: {}"'),
]

# ---------- 完整语句替换（含参数去重） ----------
FULL = [
    # GateServer LogicSystem：reset_pwd 重复参数
    ('LOG_INFO("reset_pwd request, user =  {} {} {} {} {} {}", name, ", email = ", email, name, ", email = ", email);',
     'LOG_INFO("重置密码请求: 用户名={}, 邮箱={}", name, email);'),
    # GateServer LogicSystem：upload_image 重复参数
    ('LOG_INFO("upload_image ok:  {} {} {} {} {} {}", unique_name, " size=", raw.size(), unique_name, " size=", raw.size());',
     'LOG_INFO("上传图片成功: 文件名={}, 大小={}", unique_name, raw.size());'),
    # GateServer MysqlDao：email/name exist 重复参数
    ('LOG_INFO("email  {} {} {} {}", email, " exist", email, " exist");',
     'LOG_INFO("邮箱 {} 已存在", email);'),
    ('LOG_INFO("name  {} {} {} {}", name, " exist", name, " exist");',
     'LOG_INFO("用户名 {} 已存在", name);'),
    # GateServer MysqlDao：UpdateUserInfo 重复参数
    ('LOG_INFO("UpdateUserInfo: uid= {} {} {} {}", uid, " exists, values unchanged -> success", uid, " exists, values unchanged -> success");',
     'LOG_INFO("更新用户信息: uid={}，值未变化视为成功", uid);'),
    # 连接池耗尽（4 处重复参数）
    ('LOG_ERROR("mysql pool exhausted, wait for connection timeout ( {} {} {} {}", POOL_WAIT_TIMEOUT_SEC, "s)", POOL_WAIT_TIMEOUT_SEC, "s)");',
     'LOG_ERROR("MySQL 连接池耗尽，等待连接超时（{}s）", POOL_WAIT_TIMEOUT_SEC);'),
    ('LOG_ERROR("redis pool exhausted, wait for connection timeout ( {} {} {} {}", POOL_WAIT_TIMEOUT_SEC, "s)", POOL_WAIT_TIMEOUT_SEC, "s)");',
     'LOG_ERROR("Redis 连接池耗尽，等待连接超时（{}s）", POOL_WAIT_TIMEOUT_SEC);'),
    ('LOG_ERROR("grpc pool exhausted, wait for connection timeout ( {} {} {} {}", POOL_WAIT_TIMEOUT_SEC, "s)", POOL_WAIT_TIMEOUT_SEC, "s)");',
     'LOG_ERROR("gRPC 连接池耗尽，等待连接超时（{}s）", POOL_WAIT_TIMEOUT_SEC);'),
    # user login uid（14 参重复）—— ChatGrpcClient / ChatServiceImpl / LogicSystem
    ('LOG_INFO("user login uid is   {}       {} {} {} {} {} {} {} {} {} {} {} {} {}", userinfo->uid, " name  is ", userinfo->name, " pwd is ", userinfo->pwd, " email is ", userinfo->email, userinfo->uid, " name  is ", userinfo->name, " pwd is ", userinfo->pwd, " email is ", userinfo->email);',
     'LOG_INFO("用户登录成功: uid={}, name={}, email={}", userinfo->uid, userinfo->name, userinfo->email);'),
    # LogicSystem：applyfriend 重复参数
    ('LOG_INFO("user login uid is   {} {} {} {} {} {} {} {} {} {} {} {} {} {}", uid, " applydesc  is ", desc, " bakname is ", bakname, " touid is ", touid, uid, " applydesc  is ", desc, " bakname is ", bakname, " touid is ", touid);',
     'LOG_INFO("添加好友申请: uid={}, 备注={}, 昵称={}, 目标uid={}", uid, desc, bakname, touid);'),
    # LogicSystem：user info (name/pwd/email/icon) 重复参数
    ('LOG_INFO("user  uid is   {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}", uid, " name  is ", name, " pwd is ", pwd, " email is ", email, " icon is ", icon, uid, " name  is ", name, " pwd is ", pwd, " email is ", email, " icon is ", icon);',
     'LOG_INFO("用户信息: uid={}, name={}, email={}, icon={}", uid, name, email, icon);'),
    # LogicSystem：user info (name/pwd/email) 重复参数
    ('LOG_INFO("user  uid is   {} {} {} {} {} {} {} {} {} {} {} {} {} {}", uid, " name  is ", name, " pwd is ", pwd, " email is ", email, uid, " name  is ", name, " pwd is ", pwd, " email is ", email);',
     'LOG_INFO("用户信息: uid={}, name={}, email={}", uid, name, email);'),
    # LogicSystem：GetUserByEmail hit 重复参数
    ('LOG_INFO("GetUserByEmail hit, uid is  {}   {} {} {} {} {}", user_info->uid, " name is ", user_info->name, user_info->uid, " name is ", user_info->name);',
     'LOG_INFO("按邮箱查询命中: uid={}, name={}", user_info->uid, user_info->name);'),
    # LogicSystem：msg id handler not found 重复参数
    ('LOG_INFO("msg id [ {}  {} {} {}", msg_node->_recvnode->_msg_id, "] handler not found", msg_node->_recvnode->_msg_id, "] handler not found");',
     'LOG_INFO("消息ID {} 无对应处理器", msg_node->_recvnode->_msg_id);'),
    # LogicSystem：auth friend 重复参数
    ('LOG_INFO("from  {} {} {} {} {} {}", uid, " auth friend to ", touid, uid, " auth friend to ", touid);',
     'LOG_INFO("用户 {} 验证好友 {}", uid, touid);'),
    # CSession：send que fulled 重复参数
    ('LOG_INFO("session:  {} {} {} {} {} {}", _session_id, " send que fulled, size is ", MAX_SENDQUE, _session_id, " send que fulled, size is ", MAX_SENDQUE);',
     'LOG_INFO("会话 {} 发送队列已满，容量 {}", _session_id, MAX_SENDQUE);'),
    # CSession：read length not match 重复参数（total_len / HEAD_TOTAL_LEN）
    ('LOG_INFO("read length not match, read [ {} {} {} {} {} {} {} {}", bytes_transfered, "] , total [", total_len, "]", bytes_transfered, "] , total [", total_len, "]");',
     'LOG_INFO("读长度不匹配: 已读 {}，期望 {}", bytes_transfered, total_len);'),
    ('LOG_INFO("read length not match, read [ {} {} {} {} {} {} {} {}", bytes_transfered, "] , total [", HEAD_TOTAL_LEN, "]", bytes_transfered, "] , total [", HEAD_TOTAL_LEN, "]");',
     'LOG_INFO("读长度不匹配: 已读 {}，期望头长度 {}", bytes_transfered, HEAD_TOTAL_LEN);'),
    # Redis GET（GateServer 版）
    ('LOG_INFO("[ GET  {} {} {} {}", key, " ] failed (reply null)", key, " ] failed (reply null)");',
     'LOG_INFO("GET {} 失败（回复为空）", key);'),
    ('LOG_INFO("[ GET  {} {} {} {}", key, " ] not found (expired or never requested)", key, " ] not found (expired or never requested)");',
     'LOG_INFO("GET {} 未命中（已过期或未写入）", key);'),
    ('LOG_INFO("[ GET  {} {} {}  {} {} {} {} {}", key, " ] failed (unexpected reply type: ", reply->type, ")", key, " ] failed (unexpected reply type: ", reply->type, ")");',
     'LOG_INFO("GET {} 失败（意外类型: {}）", key, reply->type);'),
    ('LOG_INFO("Succeed to execute command [ GET  {} {} {} {}", key, " ]", key, " ]");',
     'LOG_INFO("GET {} 成功", key);'),
    # Redis GET（Chat/Chat2/Status 版）
    ('LOG_INFO("[ GET   {} {} {} {}", key, " ] failed", key, " ] failed");',
     'LOG_INFO("GET {} 失败", key);'),
    ('LOG_INFO("Succeed to execute command [ GET  {} {} {} {}", key, "  ]", key, "  ]");',
     'LOG_INFO("GET {} 成功", key);'),
    # Redis Not Found / Found（GateServer 版）
    ('LOG_ERROR("Not Found [ Key  {} {} {} {}", key, " ]", key, " ]");',
     'LOG_ERROR("键 {} 未找到", key);'),
    ('LOG_INFO("Found [ Key  {} {} {} {}", key, " ] exists", key, " ] exists");',
     'LOG_INFO("键 {} 已存在", key);'),
    # Redis Not Found / Found（Chat/Chat2/Status 版）
    ('LOG_ERROR("Not Found [ Key  {} {} {} {}", key, " ]  ! ", key, " ]  ! ");',
     'LOG_ERROR("键 {} 未找到", key);'),
    ('LOG_INFO(" Found [ Key  {} {} {} {}", key, " ] exists ! ", key, " ] exists ! ");',
     'LOG_INFO("键 {} 已存在", key);'),
]

# ---------- Redis "Execut command [ CMD ..." 正则重写 ----------
CMD_RE = re.compile(
    r'^(\s*)LOG_(INFO|ERROR)\("Execut command \[ (\w+)\s+[^\"]*",\s*(.*?)\);\s*$'
)


def rebuild_cmd(line):
    m = CMD_RE.match(line)
    if not m:
        return line
    indent, lvl, cmd, args = m.groups()
    tokens = [t.strip() for t in args.split(',')]
    half = tokens[:len(tokens) // 2]
    real = [t for t in half if not t.startswith('"')]
    result_tok = half[-1] if half else ''
    if 'success' in result_tok:
        res = '成功'
    elif 'failure' in result_tok:
        res = '失败'
    else:
        res = '结果'
    ph = ' '.join('{}' for _ in real)
    fmt = '"Redis %s %s %s"' % (cmd.upper(), ph, res)
    new_args = ', '.join(real)
    return '%sLOG_%s(%s, %s);' % (indent, lvl, fmt, new_args)


def read_src(path):
    b = open(path, 'rb').read()
    if b[:3] == b'\xef\xbb\xbf':
        return b.decode('utf-8-sig')
    try:
        return b.decode('utf-8')
    except UnicodeDecodeError:
        return b.decode('gbk')


def process(path, stats):
    txt = read_src(path)
    orig = txt
    for old, new in STR:
        if old in txt:
            stats['str'][old] = stats['str'].get(old, 0) + txt.count(old)
            txt = txt.replace(old, new)
    for old, new in FULL:
        if old in txt:
            stats['full'][old] = stats['full'].get(old, 0) + txt.count(old)
            txt = txt.replace(old, new)
    lines = txt.split('\n')
    rebuilt = [rebuild_cmd(l) for l in lines]
    for o, n in zip(lines, rebuilt):
        if o != n:
            stats['redis'] += 1
    txt = '\n'.join(rebuilt)
    if txt != orig and APPLY:
        open(path, 'wb').write(txt.encode('gbk'))
    return txt != orig


def main():
    stats = {'str': {}, 'full': {}, 'redis': 0}
    files = []
    for d in ['GateServer', 'ChatServer', 'ChatServer2', 'StatusServer']:
        files += sorted(glob.glob(d + '/*.cpp'))
    changed = 0
    for f in files:
        if process(f, stats):
            changed += 1
    print('changed files: %d / %d' % (changed, len(files)))
    print('STR hits: %d rules' % len(stats['str']))
    for k, v in stats['str'].items():
        print('   [%d] %s' % (v, k[:60]))
    print('FULL hits: %d rules' % len(stats['full']))
    for k, v in stats['full'].items():
        print('   [%d] %s' % (v, k[:60]))
    print('Redis cmd rewrites: %d' % stats['redis'])


if __name__ == '__main__':
    main()
