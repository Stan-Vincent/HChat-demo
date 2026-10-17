# Hcat 项目长期备忘（原名 HChat，2026-10-12 改名）

## 0. 铁律
1. **Edit/Write 会毁掉 GBK 文件**（按 UTF-8 读写 → 中文变 U+FFFD）。GBK 源文件只准用
   **Python 字节补丁**（`decode('gbk')`→改→`encode('gbk')`，注释 ASCII）。
   GBK：`GateServer/*`、`ChatServer*/{LogicSystem.cpp,const.h}`、`Client/` 下
   `applyfriend.h`、`chatdialog.h`、`userdata.h`、`usermgr.h`。**ChatServer* 是 CRLF**：
   补丁前 `\r\n`→`\n`，写完还原。自检 `open(f,'rb').read().decode('gbk')` 成功。
   Read 把 GBK 中文显示成 `?` 只是显示问题，判据看字节。
2. **补丁脚本铁律**：① 行级吞行自检（old 剥注释后每个代码行必须原样出现在 new 里，
   故意改写用 token 白名单豁免）；② 幂等——**先查 new 再查 old**（追加型替换对 old 是
   new 的前缀、永远还在，先查 old 会重复插入）。参考 `test/patch_upload_limits.py`。
   ③ 整段替换函数体时，END marker 若含 `}`，新文本必须补回来；函数体括号深度自检=0。
3. 改完**自验证**再交付；不留后台进程锁 exe。

## 1. 架构 / 端口
- `Client/` Qt6 → `GateServer/` Boost.Beast HTTP :8080 → MySQL 3308 / Redis 6380；
  `VarifyServer/` Node gRPC :50051（只写 Redis + 发信）；`StatusServer/` :50052；
  `ChatServer/` :8990(50055)、`ChatServer2/` :8991(50056)，共享 Redis 分布式。
- GateServer 工作目录必须是 `GateServer/`（ConfigMgr 从 cwd 读 config.ini）。
- 库名小写 `hcat`（"xxxl"已过时；`llfc` 是教程空库勿混）。表：`user`、`user_id`
  （手工自增）、`private_chat`、`chat_thread`、`chat_message`、`group_chat`、
  `group_chat_member`。
- 回包两级分发：`sig_http_finish` → 按 `Modules` 分流 → Dialog 内按 `ReqId` 查 `_handlers`
  （`QMap<ReqId, std::function>`）。**Modules 决定发给谁，ReqId 决定用哪个函数。**
- `/reset_pwd` 只有 error/email/user/varifycode，**无 uuid/uid**；`CheckEmail` 只返回 bool。
- `RegPost/RegGet` 用 `map::insert` → **同 URL 注册两次后注册者静默失效**，改前 grep。
- 错误码：`Error_Json=1001`、`UidInvalid=1011`、`Error_MySql=1014`（**只有 ChatServer/const.h
  有**）、`CREATE_GROUP_FAILED=1015`、`ADD_GROUP_MEMBER_FAILED=1016`。
  1015 在客户端是 NotifyAddFriend 号，别混。

## 2. 构建
- **Client**：Qt 6.5.3 + **Qt 自带 MinGW 11.2**（`export PATH=/d/QT6/Tools/mingw1120_64/bin:$PATH`，
  **禁用 MSYS2 gcc**，混编出 `__emutls_v._ZSt11__once_call`）。构建目录
  `Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug`，`mingw32-make -j4`。
  结尾 `bin/Hcat.exe Error 127` **无害**（post-link cmd `copy` 在 bash 下找不到，链接已成功）
  → 判据是 `bin/Hcat.exe` 时间戳刷新。重生成 Makefile 必须
  `qmake -config debug -o Makefile Hcat.pro`；**不要手删 `.qmake.stash`/`Makefile*`**。
  `rc.qrc` 引用不存在的文件 → 整构建挂。改 `.qss` 后手动 `rm -f debug/Hcat_resource_res.o`。
  教程复制来的文件时间戳旧 → make 跳过 → undefined reference，`touch` 全量重编。
  exe 同级需 MinGW 3 DLL + Qt6 4 DLL + `platforms/ styles/ imageformats/ config.ini`。
- **GateServer / ChatServer（VS2022）**：必须生成 **Debug|x64**（已改 Release 运行时+无优化）；
  `start.bat` 起的是 `x64\Debug\*.exe`。exe 同级需 `plugin\`（拷自连接器 `lib64\plugin`），
  否则 `mysql_native_password` 加载失败。**本环境 VS 构建被安全策略拦截 → 只能用户自己生成。**
  两台 ChatServer 的 `Name` 自洽勿对调。
- **服务端语法自查**（编不了也能查类型）：`bash test/syntax_check.sh` → g++ `-fsyntax-only`
  过 8 个文件（MysqlDao/MysqlMgr/LogicSystem/ChatServiceImpl × 两台）。关键：必须
  `-include condition_variable -include thread -include mutex -include chrono`
  （MSVC 传递包含、MinGW 不）；`-I` 需 vcpkg/include、vcpkg/include/hiredis、
  `mysql-connector-c++-26.7.0-winx64/include`、`boost_1_92_0`、`../common`、
  `../common/third_party/spdlog/include`。

## 3. Client 运行时坑
- **`_handlers[id](...)` 前必须 `contains(id)`**：`QMap::operator[]` 插空 function → `bad_function_call` 崩。
- **成员先使用后初始化**：案例 `ChatUserWid` 构造第 19 行 `ShowRedPoint(false)` →
  `_unread_lb->hide()`，而 `_unread_lb` 第 23 行才 new → 必崩。
- 换账号/登出三层：**清数据 + 清 UI + 登录重灌**：`UserMgr::Clear()`、
  `ChatDialog::clearSessionState()/applyLoginSession()`、`ContactUserList::removeFriendItems()`、
  `ChatPage::resetForLogout()`、`ApplyFriendPage::clearAll()`、`UserInfoPage::refreshUserInfo()`。
  常驻单例只在首次登录构造，换号必须显式重新赋值。
- **崩溃定位**：gdb 在 `D:\QT6\Tools\mingw1120_64\bin\gdb.exe`；Git Bash 里 `-ex "a b"` 会被
  拆词 → 写进 `gdbcmd.txt` 用 `-x` 传 + Windows 终端跑。自动复现：`bin/login.ini`
  `auto_login=true`（**用完改回 false**）。判据 `bin/crash.log` 有无新增 + 存活 20s
  （后台进程随 bash 命令结束被杀，"进程没了"≠崩溃）。

## 4. UI 规则
- **.ui 收尾顺序**：`</layout>` → `</widget>` → `<customwidgets>` → `<resources/>` →
  `<connections/>` → `</ui>`（顶层 `</widget>` 放 customwidgets 后 → "Unexpected element
  customwidgets"）。改完 `uic x.ui -o /tmp/t.h` 校验；删控件前 grep `ui->控件名`。
- 各页是 `QStackedWidget` 页面，尺寸改在 `MainWindow`；进聊天页前
  `setMaximumSize(QWIDGETSIZE_MAX,...)` 清掉登录页 360 宽上限；每个切页入口按目标对话框
  `minimumSize()/maximumSize()` 重设窗口。
- 无边框：`MainWindow`(Frameless+WA_TranslucentBackground) → 留白容器 → `main_widget`
  (圆角白底 + `QGraphicsDropShadowEffect`) → [标题栏][stacked]。圆角 `addRoundedRect`+`setMask`；
  最大化时去圆角/阴影/留白；拖动在 `mousePress/MoveEvent` 且跳过交互控件；本构建无
  `startSystemResize`，自己按 `Qt::Edges` 调 `setGeometry`。尺寸 = 内容 + `2*kShadowMargin`
  (+ 标题栏时 `kTitleBarHeight`)。小窗浮"最小化+关闭"按钮（`QTimer::singleShot(0)` 定位 +
  `resizeEvent` 跟随）。切页/退出登录**先更新 `_ui_status` 再 `updateWindowControls()`**。
- 按钮体系：qss §9 单一来源，`role` 动态属性 + 属性选择器 → `primary`(实心橙)/`ghost`(描边)/
  `link`(橙字)；`ui->x->setProperty("role",...); reload_qss(ui->x);`（setupUi 之后）。
  `send_btn`/`receive_btn` 是 `ClickedBtn`，走 `[state=...]` 自绘，**不纳入 role**。
  ★ `reg_btn` 绝不能进 §8 实心主按钮组，只能由 §9 `QPushButton#reg_btn` 管控。

## 5. 断线自动重连（2026-10-16）
- TcpMgr：`_last_server_info` 复用首次 host/port；退避 1/2/4/8/8s × 5 次；单次 connect 4s
  看门狗；成功后**自己重放 CHAT_LOGIN** 并发 `sig_recon_success`（**不能**发
  `sig_swich_chatdlg`，否则重放动画 + 重复加载会话列表）。
- ★ token 唯一来源 = **GateServer HTTP 登录回包**（LoginDialog 里 `UserMgr::SetToken`）。
  ChatServer 的 CHAT_LOGIN_RSP **不下发 token**，别再读（会覆盖成空）。
- ★ disconnected 里**先 `StartReconnect()` 再 `emit sig_connection_closed()`**：emit 是同步
  调用，顺序反了槽函数查 `IsReconnect()` 永远 false。
- ★ 心跳定时器（ChatDialog 构造的 `_timer`）**必须判空 `user_info`**：`offlineLogin()` 会
  `UserMgr::Clear()` 而 ChatDialog 常驻 → 回登录页 10 秒内必崩。
- 断线期间 `ChatPage::on_send_btn_clicked` 要查 `TcpMgr::IsConnected()`，否则 write 静默成功。
- 测试：`test/test_reconnect_e2e.sh`（成功）/ `test_reconnect_fail.sh`（重试耗尽）。

## 6. 文件上传 = 流式方案 B（2026-10-17 落地）
历史方案（base64 塞 JSON 整包 POST）**已废弃**：300MB ⇒ 客户端峰值 1.5–2.7GB、服务端每连接
~1.1GB。放大链：原文件 →base64 ×4/3 →`QString::fromLatin1` UTF-16 翻倍 → QJsonValue 深拷贝
→ toJson。仅改常量会 OOM，唯一正解是流式。
- **新协议**：`POST /upload_image|/upload_file?uid=&file_name=` + **请求体就是原始字节**
  （无 base64、无 JSON 信封）。响应形状不变 `{error,url,unique_name,size,md5}` → 客户端不用改解析。
- 服务端四件套（GBK+LF+中文注释）：`HttpConnection.h/.cpp`（`buffer_body` + 64KB scratch +
  排空 + 巡检）、`LogicSystem.h/.cpp`（`PrepareUpload`/`FinishUpload`）。补丁脚本
  `test/patch_logicsystem_cpp.py`、`patch_httpconn_{h,cpp}.py`、`patch_httpconn_cn.py`（幂等）。
- 三层上限（`HttpConnection.h`）：`kMaxHttpBodyBytes=1GB`（**解析器兜底，故意放宽**，让被拒
  上传还能排空 body）、`kMaxJsonBodyBytes=1MB`（非上传路由真实上限，补回 Beast 默认）、
  `kMaxUploadBytes=300MB`（上传真实上限，声明式 + 流式双校验）。
- ★ **Beast body_limit**：解析请求头时就和**声明的 Content-Length** 比较，是**递减预算**
  不是累计值；必须 `async_read` 前设置；默认请求只有 1MB。
- ★ **deadline 必须周期巡检**（5s 一次，空闲 ≥60s 才关）：只读回调刷新不够——卡死的对端
  根本不产生读完成事件（MinGW 原型 A/B 实测）。旧代码 accept 时定 60s 且无
  `expires_from_now` → 传输 >60s 必被静默掐断。
- ★ **拒绝上传先排空 body 再响应**（`_drain_mode`）：立刻回响应会和还在传的客户端赛跑，
  它大概率只读到 RST 拿不到 JSON。
- 失败路径统一 `WriteJsonError()` = **HTTP 200 + JSON 里的 error 码**（客户端一直解析这形状）。
- 落盘：`<stamp>_<seq>.part` → FNV-1a 边收边算 → 定名 `<stamp>_<hash><ext>` → rename；
  扩展名保留但可执行/脚本类中和成 `.bin`；断连由析构删 `.part`。
  `std::ofstream ofs(path.wstring(),...)` **构造形式** MSVC 可用；`open(wstring)` 在
  libstdc++ 没有，MinGW 只接受 `std::filesystem::path` 重载。
- 客户端（UTF-8）：`httpmgr` 新增 `PostHttpReqRawBody(QUrl, QIODevice*, size, ...)`；
  `uploadmanager` 从 `QFile` 直发（不再 `readAll()`）；`MessageTextEdit` 硬编码 20MB 改引用常量。
  ★ `QNAM::post(request, QIODevice*)` **不接管设备所有权** → 必须
  `connect(reply, &QObject::destroyed, body, &QObject::deleteLater)`，否则每次上传漏一个 QFile。
- ★★ **Qt `uploadProgress` 收尾脉冲**：传完 QNAM 还发一次 `(0,0)`。透传会让
  `ChatPage::refreshUploadBar` 因 `t.second > 0` 不成立退化为"不确定模式"（100% 又跳回走马灯），
  并发时已完成任务分母消失 → 百分比**不升反降**（2×50MB：75% → 50%）。已在
  `PostHttpReqRawBody` 过滤 `sent==0 && total==0`。

## 7. 上传限制运行时可配 + 防慢速（2026-10-17 晚）
- `config.ini` `[Upload]` 段：`MaxBytes` / `MinBytesPerSec` / `MaxDurationSec`。
  服务端 `InitUploadLimits()`（main 里 ConfigMgr 之后）读入；客户端 `main.cpp` 用 QSettings
  读 `Upload/MaxBytes` 再 `UploadManager::setMaxBytes()`。`MAX_IMAGE_SIZE`/`MAX_FILE_SIZE`
  已由 `constexpr` 改为运行时 static + setter。改上限不用重编译。
- ★ `InitUploadLimits()` 对 `max_bytes <= kMaxHttpBodyBytes` 硬约束：超了 Beast 在解析头就
  掐断、body 不排空 → 客户端只看到 reset（= 旧病复发）。
- ★ 客户端和服务端**各有一份 config.ini，必须配成同一个值**。
- ★ `ConfigMgr::operator[](section)` **按值**返回 SectionInfo（不是引用）：
  `auto& cfg = ConfigMgr::Inst()["X"]` → C2440，必须 `auto cfg = ...`。section 不存在返回空对象。
- ★★ **"上限"挡不住"慢慢传"**：稳定 20KB/s 的对端每 5s 都有数据，永不触发 idle 超时，
  300MB 能拖 4 小时（连接 + 句柄 + .part 全程占着）。故巡检另加两道闸门：
  ② **最低速率** `kMinUploadBytesPerSec=64KB/s`，连续 `kSlowPatrolLimit=3` 次巡检低于它才掐
  （判一次就杀太脆，抖动会误杀）；③ **总时长** `kMaxUploadDurationSec=600s`。
  ★ ③ 实际精度 = 巡检间隔（配 3s 实测 5.0s 才掐，因为检查只在巡检里做）。
- ★ config.ini 一律写 **ASCII 注释**：走 `boost::property_tree::read_ini`，本环境跑不了
  GateServer，无法验证中文注释会不会让它起不来，不冒险。

## 8. 测试与验证台
- **一键入口 `bash test/run_all.sh`**：[1] protocol_test(28) + [2] gate_e2e(22) +
  [3] upload_big(4) + [4] group_chat(9)。带端口预检、逐套执行、汇总表、退出码。
  `--start` 缺什么起什么并在结束时关掉自己起的；`SKIP_XXX=1` 跳某套；
  `GATE_URL=...` 把 [3] 打到 mock；`RUN_MOCK=1` 加跑 beast_proto 套件。日志落 `test/_logs/`。
  **不要用 run_test.bat**：结尾 pause 非交互会挂住，且只跑 [1]。
- ★ **protocol_test 不只依赖 ChatServer**：它先用 GateServer `/user_login` 换 token，
  而 `/user_login` 内部 gRPC 调 StatusServer 分聊天服。缺 StatusServer/GateServer 时
  **表现是登录失败（1009/1002）、28 个用例一行都跑不到**，容易误判成 ChatServer 崩了。
- `test/dbconf.py`：MySQL 信息统一从 `GateServer/config.ini` 的 `[Mysql]` 读。
  **真实口令 `Hcat@2026Secure`**，不是 123456。★ 教训：写死口令不是报错而是**静默降级**
  （pick_uid 退回 2、备份失败、退出不还原，测试照样"全绿"但啥也没验）。同理
  protocol_test 原来写死 `test@test.com`，而库里 uid=2 实际是 **`0@1.com`** → 全挂。
  账号/口令一律问库要。（`HCHAT_PORT=8991` 整套打 ChatServer2；`HCAT_TEST_EMAIL` 可覆盖。）
- `test/protocol_test.py`：默认 8990；uid=2（邮箱由 `test_email()` 查库），thread 1 = (2,3)，
  peer=3，局外人 uid=6。★ "只发不收"的用例会把回包留在 TCP 缓冲排队，`wait_rsp` 必须带
  字段匹配（fromuid/thread_id），否则回包**错位一条**，把"已拒绝"读成"已接受"。
- `test/gate_e2e_test.py`：22 项，已迁移到流式原始字节（`post_raw`），含 `.exe→.bin`
  中和、"老 base64/JSON 协议必须被拒"；退出前还原 uid 资料 + 清理服务端落盘文件。
- `test/upload_big_test.py`：4 项（19MB 成功+GET 回读 / 301MB 可读 JSON 错误 / 旧 JSON 信封
  被拒 / 缺 file_name）。`GATE_URL=http://127.0.0.1:18080` 可打 mock。
- **验证台**（本环境编不了 GateServer，靠它验契约）：`test/beast_proto/mock_gate.cpp` =
  PrepareUpload/FinishUpload 的 1:1 MinGW 移植（drain、扩展名中和、FNV-1a、`GET /upload/<file>`
  与 `..` 穿越防护、巡检 deadline）。编译
  `g++ -std=c++17 -O2 -I/d/software/boost_1_92_0 mock_gate.cpp -o mock_gate.exe -lws2_32
  -lmswsock -lpsapi`（**缺 `-lmswsock` 报 AcceptEx undefined**）。argv: port, out_dir,
  max_upload, min_bps, max_dur。`mock_smoke.py` 8 条；`upload_probe/` 链接**真实**
  `uploadmanager.cpp`+`httpmgr.cpp` 的 Qt 探针 7 条；`slow_guard_test.py` 3 条。全通过。
  实测：50MB 上传客户端峰值 22MB、服务端 10.6MB；301MB 拿可读 JSON 错误（2.9s，非断连）。

## 8.5 群聊支持（2026-10-17 已实现，等服务端重新编译）
**用户选定口径**：先打通收发；群元数据新开 `group_chat` 表；群消息 `recv_id` 存 0。

**DB**：`group_chat(thread_id PK, name, owner_id, created_at)` + 已有
`group_chat_member(thread_id,user_id)`。测试群 thread_id=17「Hcat 测试群」成员 2/3/6。

**服务端**（ChatServer 与 ChatServer2 **逐字节一致**，改完必须 diff 校验）：
- `MysqlDao/Mgr` 新增：`GetThreadType` / `GetGroupMembers` / `CreateGroupChat`（一个事务写
  chat_thread+group_chat+group_chat_member，owner 排第一并去重）/ `AddGroupMembers`
  （先查 type='group' 才允许，`INSERT IGNORE` 跳过已存在）。
- `LogicSystem::DealChatTextMsg` 重写：先 `GetThreadType` 分支；private 走原「两成员」校验；
  group 走成员列表校验、排除 sender 收集 `recipients`、`touid=0`（recv_id 也 0）、
  `rtvalue["thread_type"]`；投递改 `for (int ruid : recipients)`，每人一份 notify（只改
  touid），本地 session 或 gRPC。
- ★ 顺手修既有 bug：跨服 gRPC 原来**不传** `msg_type/total_size/md5`（`TextChatData` proto
  本来就有这三个字段只是没人填）→ 图片跨服被当纯文本。现在 LogicSystem 填、
  `ChatServiceImpl::NotifyTextChatMsg` 取。
- `data.h` `ChatThreadInfo` 加 `_name`；`GetUserThreads` 的 group 分支 `LEFT JOIN group_chat`
  取群名（private 分支 `'' AS name`）；会话列表回包多一个 `name`。
- 新 ID：`ID_CREATE_GROUP_CHAT_REQ/RSP=1033/1034`、`ID_ADD_GROUP_MEMBER_REQ/RSP=1035/1036`；
  新错误码 `1015/1016`。建群上限 200 人；加人只允许**现有成员**发起（否则猜 thread_id 就能进群）。

**客户端**：`userdata.h/.cpp` 加 `_name` / `_is_group` / `SetGroupInfo()` / `IsGroup()` /
`GetGroupMembers()`；`chatdialog.cpp` 删 `continue`，群会话 `ChatThreadData(0,thread_id,0)`
+ `SetGroupInfo`；`chatuserwid.cpp` 群聊分支用群名（兜底「群聊 <thread_id>」）；
`chatpage.cpp` 切群会话不再提前 return（原来列表不清/草稿不还原/上一段私聊气泡残留），
`AppendChatMsg` 群聊兜底「用户 <uid>」；`tcpmgr.cpp` 两条收消息链路按
`IsGroup()` 决定 `ChatFormType`（原来写死 PRIVATE）。

**★ 协议上没动 proto**：群/私聊由客户端按 thread_id 查本地会话列表区分；notify 的
`touid` = 每个接收者自己的 uid（发送者自己的 RSP 才是 0）。

**踩坑**：① `patch_group_logic.py` 第一版把 `DealChatTextMsg` **结尾的 `}` 吞了**（END marker
吃了 `}` 而新文本没补）→ 花括号配平从 0 变 1。② `userdata.h` 的 `int _user2_id;` 后面跟了
行尾中文注释，不能用 `"int _user2_id;\n"` 当锚点，改 `insert_after_line()` 按行插入。

**测试**：`test/group_chat_test.py`（9 条：建群/群名/成员发消息/recv_id=0/扇出/发送者不回环/
局外人发消息被拒/拉人/局外人拉人被拒），`run_all.sh` 里是第 [4] 套，`SKIP_GROUP=1` 可跳过。
需两台 ChatServer 都活着（不同号可能被分到不同机器，顺带覆盖跨服 gRPC 扇出）。

## 9. 客户端日志
- `bin/client.log`：`main.cpp` 装 `qInstallMessageHandler`，全部落盘（GUI 程序 stdout 重定向
  无效，这是唯一日志源），超 2MB 自动重开。
- ★ **`qCDebug` 分类输出默认被过滤**，必须 `QT_LOGGING_RULES='*.debug=true'`；`qDebug()` 不受影响。
- 实验前 `taskkill //F //IM Hcat.exe`：多实例会让 client.log 被后启动者 rm 重建、旧实例写已
  删除 inode，出现"日志只有 2 行"的假象。

## 10. 协作约定（用户明确授权）
- **编译前可直接杀进程**：`taskkill //F //IM Hcat.exe`（用户常忘关客户端，锁 exe →
  Permission denied）。
- **后端同理**：VS 生成报 `MSB3073 xcopy ... 退出码 4` = 生成后事件要拷 `config.ini`/`plugin\`
  而旧实例还在跑、DLL 被锁。链接其实已成功，taskkill 后重启即可。
- 用户 C++ 后端方向、中文交流、要直接给结论和证据；反感反复编译失败循环，改完自验证再交付。

## 11. 编译前自查清单（客户端）
1. 查杀 Hcat.exe；2. 用 Qt 自带 MinGW 的 PATH；3. `mingw32-make -j4`，只看 `error:` /
   `undefined reference`；4. 确认 `bin/Hcat.exe` 时间戳刷新（Error 127 可忽略）。
