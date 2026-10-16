# Hcat 项目长期备忘（原名 HChat，2026-10-12 改名）

## 架构 / 端口
- `Client/` Qt6 客户端 → `GateServer/` C++ Boost.Beast HTTP（8080）→ MySQL 3308、Redis 6380；
  `VarifyServer/` Node gRPC（50051，只写 Redis + 发信，不连 MySQL）；`StatusServer/` 50052；
  `ChatServer/` 8990(50055)、`ChatServer2/` 8991(50056)，共享 Redis 做分布式。
- GateServer 工作目录必须是 `GateServer/`（ConfigMgr 从 cwd 读 config.ini）。
- 库名小写 `hcat`（旧记"xxxl"已过时；`llfc` 是教程空库，勿混用）。
  表 `user(id,uid,name,email,pwd,nick,icon)`、`user_id(id)`（手工自增）、`private_chat`、`chat_thread`、`chat_message`。
- 回包两级分发：`sig_http_finish` → `slot_http_finish` → 按 `Modules` 分流 → Dialog 内按 `ReqId`
  查 `_handlers`（`QMap<ReqId, std::function>`）回调。**Modules 决定发给谁，ReqId 决定用哪个函数。**
- `/reset_pwd` 只返回 error/email/user/varifycode，**无 uuid/uid**；`CheckEmail` 只返回 bool。
- `LogicSystem::RegPost/RegGet` 用 `map::insert` → **同 URL 注册两次后注册者静默失效**，改前先 grep。

## 源码编码（改前必看）
- GBK：`GateServer/*`、`ChatServer*/LogicSystem.cpp`、`ChatServer*/const.h`、`Client/` 下的
  `applyfriend.h`、`chatdialog.h`、`userdata.h`、`usermgr.h`。其余 UTF-8。
- 改 GBK 文件用 Python 字节补丁（`decode('gbk')` → 改 → `encode('gbk')`），注释一律 ASCII；
  **ChatServer* 是 CRLF**，补丁前先 `\r\n`→`\n`，写完再还原。

## 构建：Client（Qt 6.5.3 + Qt 自带 MinGW 11.2.0）
- 构建目录 `Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug`，
  `export PATH=/d/QT6/Tools/mingw1120_64/bin:$PATH`（**不要用 MSYS2 gcc**，混编会出
  `__emutls_v._ZSt11__once_call` 链接/运行期错）后 `mingw32-make -j4`。
- 结尾 `Makefile.Debug:388: bin/Hcat.exe Error 127` **无害**：post-link 是 cmd 语法（`copy`），
  bash 下找不到；**链接其实已成功**，DLL 早就在 bin 里。判据是 `bin/Hcat.exe` 时间戳刷新。
- 重生成 Makefile 必须 `qmake -config debug -o Makefile Hcat.pro`（裸跑会把默认目标翻成 release）。
- **不要手删 `.qmake.stash`/`Makefile*`**：qmake 在 bash 沙箱里全新探测编译器会挂
  （`QMAKE_CXX.COMPILER_MACROS is not defined`）。要重建就在 Qt Creator 里 Build。
- `rc.qrc` 引用不存在的文件 → `No rule to make target` 整构建挂，删 orphan `<file>` 行。
- 改 `.qss` 等资源后 make 不重编 `debug/Hcat_resource_res.o` → 手动 `rm -f` 它再编。
- 教程复制来的文件时间戳很旧 → make 跳过编译/moc → undefined reference。解决：`touch` 全量重编。
- exe 同级必须有 MinGW 3 DLL + Qt6 4 DLL + `platforms/ styles/ imageformats/ config.ini`（post-link 拷）。
- 分层：`main.cpp`（读 config.ini 拼 `gate_url_prefix`）→ `MainWindow`(setCentralWidget 切页) →
  Login/Register/Reset Dialog → `HttpMgr` → GateServer。

## 构建：GateServer / ChatServer（VS2022）
- 改源码后必须在 VS 生成 **Debug|x64**（该配置已改成 Release 运行时+无优化，可连 MySQL 可断点）；
  `start.bat` 起的就是 `x64\Debug\*.exe`，生成 Release 等于白改。
- exe 同级需 `plugin\`（拷自连接器 `lib64\plugin`），否则 `mysql_native_password` 加载失败。
- Debug 目录 DLL 必须与 CRT 一致（vcpkg 的 abseil/cares/libssl/re2/jsoncpp Debug/Release 同名），
  残留 Debug 版已移到 `x64/Debug/stale_debug_dlls/`。
- 两台 ChatServer 的 `Name` 自洽，不要对调。VS 构建工具在本环境被安全策略拦截 → 只能用户自己生成。

## Client 运行时坑（血泪）
- **`_handlers[id](...)` 前必须 `contains(id)`**：`QMap::operator[]` 会插入空 function → `bad_function_call` 崩。
- **成员先使用后初始化**：改写"兼容包装函数"时要检查它是否被子控件构造函数调用、是否摸了运行时才
  new 的裸指针。案例：2026-10-15 `ChatUserWid` 构造第 19 行 `ShowRedPoint(false)` → `clearUnread()`
  → `_unread_lb->hide()`，而 `_unread_lb` 第 23 行才 new（初值 nullptr）→ 登录加载会话列表必崩。
- 换账号/登出要"清数据 + 清 UI + 登录时重灌"三层：`UserMgr::Clear()`、`ChatDialog::clearSessionState()`
  /`applyLoginSession()`、`ContactUserList::removeFriendItems()`、`ChatPage::resetForLogout()`、
  `ApplyFriendPage::clearAll()`、`UserInfoPage::refreshUserInfo()`。
- 单例/常驻页面（`UserMgr`、`ChatDialog`、`UserInfoPage`、`ApplyFriendPage`）只在首次登录构造，
  换号必须显式重新赋值，否则停在旧账号数据。

## 崩溃定位（Qt MinGW）
- gdb 在 `D:\QT6\Tools\mingw1120_64\bin\gdb.exe`。**Git Bash 里 `-ex "a b"` 会被拆词** → 把命令写进
  `gdbcmd.txt` 用 `-x` 传，并用 Windows 终端跑（输出重定向到文件再读）。
- 让崩溃自动复现：`bin/login.ini` 的 `auto_login` 临时改 `true`（DPAPI 存密码）→ 无需人工点登录。
  **用完必须改回 false。**
- 判据用 `bin/crash.log` 有无新增条目 + 存活 20s；后台起的 exe 会在 bash 命令结束/沙箱清理时被杀，
  不能单凭"进程没了"判定崩溃。

## UI 规则
- **.ui 收尾顺序**：`</layout>` → `</widget>` → `<customwidgets>` → `<resources/>` → `<connections/>`
  → `</ui>`。顶层 `</widget>` 放到 customwidgets 之后会报 "Unexpected element customwidgets"。
  改完跑 `uic x.ui -o /tmp/t.h` 校验。删 .ui 控件前先 grep `ui->控件名` 确认无引用。
- 登录/注册/重置/聊天都是 `QStackedWidget` 页面（非顶层窗口）：尺寸改在 `MainWindow`，
  且进聊天页前 `setMaximumSize(QWIDGETSIZE_MAX,...)` 清掉登录页 360 宽上限。
  所有切页入口都要按目标对话框 `minimumSize()/maximumSize()` 重设窗口尺寸。
- 无边框：`MainWindow`(Frameless+WA_TranslucentBackground) → 中央留白容器 → `main_widget`
  (圆角+白底+`QGraphicsDropShadowEffect`) → [标题栏][stacked]。圆角要 `addRoundedRect`+`setMask`；
  最大化时去圆角/阴影/留白；拖动在 `mousePress/MoveEvent` 且要跳过交互控件；
  本构建无 `startSystemResize`，自己按 `Qt::Edges` 调 `setGeometry`。
  尺寸 = 内容 + `2*kShadowMargin`（+ 有标题栏时 `kTitleBarHeight`）。
  小窗不显示标题栏，浮"最小化+关闭"两颗按钮（`QTimer::singleShot(0)` 延迟定位 + `resizeEvent` 跟随）。
  切页/退出登录**先更新 `_ui_status` 再 `updateWindowControls()`**。
- 按钮体系：qss §9 单一来源，`role` 动态属性 + 属性选择器 → `primary`(实心橙)/`ghost`(描边)/`link`(橙字)；
  用法 `ui->x->setProperty("role",...); reload_qss(ui->x);`（setupUi 之后）。
  `send_btn`/`receive_btn` 是 `ClickedBtn`，走 `[state=...]` 自绘，**不纳入 role**。
  ★ `reg_btn` 绝不能进 §8 实心主按钮组，只能由 §9 的 `QPushButton#reg_btn`（透明底+橙字）管控。

## 测试链路
- `test/run_test.bat` → `test/protocol_test.py`（ChatServer 协议级，判据是"服务端存活"）。
  默认打 8990；**`HCHAT_PORT=8991` 可整套用例打到 ChatServer2**。
  登录账号 `test@test.com`(uid 2)，thread 1 = (2,3)，peer=3，局外人 uid=6。
- `test/gate_e2e_test.py`：GateServer HTTP 接口 20 项回归。
- `test/upload_big_test.py`：大文件上传 3 档（19MB 成功 / 21MB 明确拒绝 / 35MB HTTP 层拒绝）。
- 服务端错误码（回包里）：`UidInvalid=1011`、`Error_MySql=1014`（**只有 ChatServer/const.h 有
  Error_MySql**，GateServer/const.h 里没这个名，别混）。
- ★ 协议测试的坑：那些"只发不收"的用例会把回包留在 TCP 缓冲里排队，`wait_rsp` 必须带
  字段匹配（fromuid/thread_id），否则回包**错位一条**，把"已拒绝"读成"已接受"。

## 客户端日志与调试
- `bin/client.log`：`main.cpp` 装了 `qInstallMessageHandler`，所有日志落盘（GUI 程序 stdout
  重定向无效，这是唯一的日志来源）。超 2MB 自动重开。
- ★ **`qCDebug` 的分类输出默认被过滤**，必须 `QT_LOGGING_RULES='*.debug=true'` 才看得到；
  `qDebug()`（default 分类）不受影响。测试脚本顶部统一 export。
- 实验前先 `taskkill //F //IM Hcat.exe`：多实例会让 `client.log` 被后启动者 rm 重建、
  旧实例写已删除的 inode，出现"日志只有 2 行"的假象。
- bash 命令结束会清理该命令启动的后台进程 → "进程没了"≠崩溃，判据用 crash.log 有无新增。

## 断线自动重连（2026-10-16 实现）
- TcpMgr 状态机：`_last_server_info` 复用首次连接的 host/port；退避 1/2/4/8/8s × 5 次；
  单次 connect 有 4s 看门狗；重连成功后**自己重放 CHAT_LOGIN** 并发 `sig_recon_success`
  （**不能**发 `sig_swich_chatdlg`，否则重放登录动画 + 重复加载会话列表）。
- ★ token 唯一来源 = **GateServer HTTP 登录回包**（LoginDialog 里 `UserMgr::SetToken`）。
  ChatServer 的 CHAT_LOGIN_RSP **不下发 token**，别再从那里读（会把有效 token 覆盖成空）。
- ★ disconnected 里必须**先 `StartReconnect()` 再 `emit sig_connection_closed()`**：
  emit 是同步调用，顺序反了槽函数查 `IsReconnect()` 永远是 false，重连等于没接上。
- ★ 心跳定时器（ChatDialog 构造里的 `_timer`）必须判空 `user_info`：`offlineLogin()` 会
  `UserMgr::Clear()`，而 ChatDialog 常驻、定时器不停 → 回登录页 10 秒内必崩（nullptr）。
- 断线期间 `ChatPage::on_send_btn_clicked` 要查 `TcpMgr::IsConnected()`，否则 write 静默成功。
- 测试：`test/test_reconnect_e2e.sh`（成功路径）、`test/test_reconnect_fail.sh`（重试耗尽路径）。

## 协作约定（用户明确授权）
- **编译前可直接杀进程**：`tasklist //FI "IMAGENAME eq Hcat.exe"` → 有就 `taskkill //F //IM Hcat.exe`，
  不用再问（用户常忘关客户端，会锁住 exe 导致 `Permission denied`）。
- **后端同理**：VS 生成报 `MSB3073 xcopy ... 退出码 4` = 生成后事件要往 `x64\Debug\` 拷
  `config.ini`/MySQL `plugin\`，而 `ChatServer.exe` 等旧实例还在运行、DLL 已加载被锁（Sharing violation）。
  链接其实已成功。直接 `taskkill //F //IM ChatServer.exe`（start.bat/stop.bat 管理，重启即可）。
- 用户是 C++ 后端方向、中文交流、要直接给结论和证据；反感反复编译失败循环，改完要自验证再交付。

## 编译前自查清单（客户端）
1. 查杀 Hcat.exe；2. 用 Qt 自带 MinGW 的 PATH；3. `mingw32-make -j4`，只看 `error:`/`undefined reference`；
4. 确认 `bin/Hcat.exe` 时间戳刷新（Error 127 可忽略）。
