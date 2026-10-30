# Hcat 项目长期备忘（精简索引）

> **完整踩坑全集见 `docs/工程备忘.md`**（18KB，随项目走，改代码前先翻对应章节）。
> 配套：`docs/源码地图.md`（模块→文件:类→职责）。一次性的 patch/fix 脚本已归档 `test/archive/`。

## 铁律（三条，违反必炸）
1. **改源码前先跑编码体检**：`python test/encoding_audit.py`（UTF-8 390 / GBK 64 / ASCII 138 /
   腐蚀 0）。GBK 文件只准 Python 字节补丁，UTF-8 文件才可 Edit。**编码会漂移**
   （`usermgr.h`/`chatdialog.h` 已被腐蚀后于 10-18 还原为 UTF-8，别再当 GBK 处理）。
   ★ **分模块：Client 全 UTF-8**（Qt QString 按 UTF-8 解析，GBK 会乱码，10-18 已把最后 2 个转完）；
   **后端保持 GBK**（MSVC 按 936 解析本就正确，转 UTF-8 要加 /utf-8 且踩 spdlog/fmt）。
   ChatServer* 是 GBK+CRLF，补丁前后要 `\r\n`↔`\n`。详见 `docs/编码现状.md`。
2. 补丁脚本：行级吞行自检 + 幂等（先查 new 再查 old）+ 括号深度自检=0。
3. 改完自验证再交付；不留后台进程锁 exe（编译前可 `taskkill //F //IM Hcat.exe`）。

## 架构速查
`Client/` Qt6 → `GateServer/` Beast HTTP :8080 → MySQL 3308 / Redis 6380；`VarifyServer/` Node gRPC :50051；
`StatusServer/` :50052 分服；`ChatServer/` :8990(50055)、`ChatServer2/` :8991(50056) 共享 Redis。
**两台共用同一份源码**：`ChatServer2/` 无源码副本，靠 `..\ChatServer\SharedSources.props` 引用；
差异**只剩 config.ini**（Name/Port/RPCPort）+ 日志实例名由 `cfg["SelfServer"]["Name"]` 决定。
**新增源文件只改 SharedSources.props**；`syntax_check.sh` 只查 ChatServer 一份。
库 `hcat`。回包两级分发：**Modules 决定发给谁，ReqId 决定用哪个函数**。

## 目录结构（2026-10-18 重构）
`Client/` 源码按职责分目录：`ui/`(34h+34cpp+21ui) `widgets/`(12h+11cpp) `network/`(4+4) `model/`(2+2)
`common/`(4h+3cpp)，根留 `main.cpp`。`.pro` 用 `INCLUDEPATH` 铺平 → 源码 `#include "xxx.h"`
**不带目录前缀**、一行没改（GBK 零风险）。新增目录要同步加 INCLUDEPATH。

- **★ 服务端自验必须用 MSVC（10-21 解锁）**：`python test/msvc_syntax_check.py GateServer/RedisMgr.cpp ...`
  （内部走 vcvars64 + cl /c）。**别再只用 MinGW g++ -fsyntax-only 代替**——两者严格度不同，
  典型案例：`common/RedisConPool.h` 用 `std::this_thread::sleep_for` 但漏 `#include <chrono>`，
  GCC 通过 `<thread>` 间接带入不报错，MSVC 直接报 C2672 并级联出一片
  "不是 RedisConPool 的成员"，用户 VS 一编译全红。**给用户的版本必须先过 MSVC 检查。**
- **Git Bash 调 Windows 编译器的坑**：MSYS 会把 `/ID:\...` 参数当路径转换掉 → 走 python
  subprocess 最稳（`msvc_syntax_check.py` 已封装）。
## 构建
- Client：Qt 6.5.3 + **Qt 自带 MinGW 11.2**（`/d/QT6/Tools/mingw1120_64/bin`，禁用 MSYS2 gcc），
  `mingw32-make -j4`；结尾 `Error 127` 无害（`copy` 是 cmd 内建命令，bash 下找不到），判据是 `bin/Hcat.exe` 时间戳刷新。
- ★★ **`.pro` 里中文注释会吞掉紧随其后的一行**（qmake 解析 bug，静默无报错）：曾让
  `win32:LIBS += -ldbghelp/-lcrypt32` 一直失效，直到重新 qmake 才暴露成链接期 undefined reference。
  纪律：**中文注释与下一条语句之间必须空一行**（10-18 已给全文件加隔离空行，7 处）。
  同理：注释里别写 `#include "xxx.h"`（会报 Extra characters after test expression）。
  改完 `.pro` 后务必 `grep` Makefile 确认新设置真的进去了（LIBS/INCLUDEPATH 都查过）。
- 服务端：VS2022 Debug|x64，**本环境 MSBuild 被安全策略拦截 → 只能用户自己生成**；
  语法自查用 `bash test/syntax_check.sh`。
- 客户端：源码按职责物理分目录（见下）。**后端不做物理分目录**（文件少、MSVC include 苛刻、本环境编不了），
  改用 **`.vcxproj.filters` 虚拟分组**（VS 里按 入口/网络层/业务逻辑/gRPC/数据访问/会话与状态/配置与公共/数据模型/协议 显示），
  零风险；规则见 `test/archive/_make_backend_filters.py`，新增文件后重跑它。

- **错误码分段（10-21 定）**：1012~1016 归 GateServer 段（DbErr=1012）；
  ChatServer 专用码从 **1017** 起（CREATE_CHAT_FAILED/LOAD_CHAT_FAILED/Error_MySql/
  CREATE_GROUP_FAILED/ADD_GROUP_MEMBER_FAILED）。曾因 ChatServer 占用 1012 导致
  客户端把"创建会话失败"提示成"数据库错误"。4 份 ErrorCodes 定义**不合并**，
  靠 ============================================================
配置一致性校验
============================================================
  [OK] 上传上限一致: 314572800 bytes (300 MB)
  [OK] 12 个错误码在各模块数值一致
  [OK] 6 个监听端口无冲突
  [OK] [Mysql] 各服务一致: 127.0.0.1/3308/Hcat
  [OK] [Redis] 各服务一致: 127.0.0.1/6380
------------------------------------------------------------
全部通过 校验数值一致（已进 run_all.sh）。
  配置一致性（上传上限/端口/MySQL/Redis）也由该脚本一并校验。
- **Redis 连接池已共享（10-21）**：`common/RedisConPool.h`（header-only）是唯一实现，ChatServer/GateServer
  的 RedisMgr 都 include 它。特性：巡检默认 5s、命令超时 2s、补齐指数退避 1s→30s、坏连接丢弃由巡检补齐。
  **新增文件只改这里**（别再把实现复制回各服务）。GateServer 有 `GET /health`（探 Redis+MySQL，失败 503），
  测试 `test/redis_reconnect_test.py`（CLIENT PAUSE 模拟抖动）。⚠️ 需 VS 重编后才生效。
## 高频坑（详见工程备忘对应章节）
- Client：`_handlers[id]` 前必须 `contains(id)`（否则 `bad_function_call`）；换账号/登出要**清数据+清UI+登录重灌**三层。
- **动效只用几何/布局，禁新增 GraphicsEffect**（`_main_widget` 阴影祖先下嵌套必崩）。10-30 新增
  `AnimUtil::unfoldUp`（弹层向上展开）/`pressBounce`（按钮下压回弹）+ `widgets/spinner`（自绘加载环）。
  两个坑：①展开必须 `setMinimumSize(w,0)+setMaximumSize(w,QWIDGETSIZE_MAX)`，**只把 max 设 0 会被
  setGeometry 夹成 0 高度**；②连点重入要用 `_unfold_full/_unfold_bottom` 属性记住目标，否则拿动画中间值当目标。
- **联系人空白真因 = 分页游标 `_contact_loaded` 未复位**（`reloadFriendItems()` 前调 `ResetContactLoadedCount()`）。
- **图标空白真因 = exe 裸奔靠 PATH 加载 Qt**（本机有官方6.5.3/MSYS2 6.11/anaconda 三提供者）——已用 `test/deploy_qt_runtime.sh` 把 Qt 6.5.3 运行时部署到 `Client/bin/` **和 Creator shadow build 目录**（用户实际从 IDE 运行，两个都要！10-26）；**删 build 目录/换机/升级 Qt 后必须重跑该脚本**。改图标格式若涉及"内建→插件"(PNG→SVG)，必查 imageformats 插件。
- 重连：disconnected 里**先 `StartReconnect()` 再 emit**；token 唯一来源=GateServer 登录回包；心跳定时器必须判空 user_info。
- 上传：流式原始字节（非 base64）；三层上限 1GB/1MB/300MB；**上限挡不住慢速** → 加最低速率+总时长；
  拒绝上传先排空 body（drain）再响应；Beast `body_limit` 必须在 `async_read` 前设。
- **连接池 autocommit 泄漏**（群聊 9/9↔4/9 漂移真因）：修在 `MysqlDao.h::returnConnection` 还池前 `setAutoCommit(true)`。
- 群聊：1033/1034 建群、1035/1036 加人；建群 UI 已补（右键会话列表→发起群聊，**群里不显示此项**）；
  **群成员已能看（10-21）**：不加新协议，靠 1018 回包的 `members` 字段（`MysqlDao::GetUserThreads`
  批量 `IN(...)` 查 group_chat_member），客户端 `ui/groupmemberdlg.{h,cpp}` 弹窗展示。
  **加成员 UI(1035)也已做（10-21）**：右键群会话 → 查看群成员 → 邀请好友入群
  （`ui/selectfrienddlg.{h,cpp}` 多选、过滤群内成员；1036 回包不回"加了谁"，
  客户端用 `_pending_added` 记发送的 uid 直接并列表，**不重发 1018**——老服务端没 members 会冲掉列表）。
  群头像用**字母头像**（`common/avatarutil.{h,cpp}`：首字+8 色板+seed 选色，零资源文件）。
  ⚠️ 1018 的 members 字段仍需 VS 重编 ChatServer 后才生效（不加成员的路径已可用）。

## 协作约定
用户 C++ 后端方向、中文交流；要**直接结论 + 证据**、结构化表格；反感反复编译失败循环。

## 云部署已跑通（10-26）
- 阿里云 47.96.16.175(Ubuntu24.04/2C2G) 跑全套后端：5 个 systemd unit `hchat-{gate,chat1,chat2,status,varify}` + MySQL(3308)/Redis(6380，密码 `Hcat@2026Secure`，仅 127.0.0.1)。公网仅开 8080/8990/8991。
- 产物：WSL `linux/build_wsl.sh` 出静态二进制（protobuf/grpc 静态；动态仅 libmysqlcppconn.so.9 + Boost1.83/zstd/ssl）。`test/deploy_to_cloud.sh` rsync→`/opt/hchat`，`test/server_systemd_setup.sh`、`test/server_varify_setup.sh` 一键装。
- **StatusServer 的 `[chatserver1/2] Host` 必须=公网IP（下发给客户端），`[Mysql]/[Redis]` 必须留 127.0.0.1**（曾用全局 sed 误改，改为按 section awk 精确替换）。VarifyServer `config.json` 的 `redis.passwd` 要对齐本机密码。
- 客户端联调：改**运行目录** `config.ini` 的 `[GateServer] host` 为公网 IP（运行时读取，**无需重编**），测完改回 localhost。

## ★ WSL 调用四坑（10-26 实战）
1. **MSYS 路径转换**：Git Bash 把裸参数 `/mnt/d/...` 改写成 `PortableGit/mnt/d/...` → 加 `MSYS_NO_PATHCONV=1`，或把路径塞进 `bash -c '...'` 的引号里。
2. **`$` 提前展开**：`wsl -- bash -c '...$VAR...'` 里 `$VAR` 被外层 Git Bash 展开成空 → 用 `\$` 转义，或把脚本写成文件再 `bash file`（最稳）。
3. **root 遗留物**：之前 sudo 构建在 `~/HCat_src`、`~/build_wsl.log` 留 root 文件，xxxl `rm -rf` 报 Permission denied → 先 `wsl -u root` 清理/chown 再跑。
4. **脚本自替换竞态**：`build_wsl.sh` 第1步 `git reset --hard` 会替换脚本自身，运行中的 bash 读旧 inode → 第44行可能跑旧版（把子目录当 root 传给 apply_port.py，等于没打补丁，编译报 `sw/redis++/redis++.h` 找不到）。**先把 ~/HCat 预同步到 HEAD，让 reset 成空操作**即可避免。
5. **客户端 config.ini 在 exe 同目录**：客户端读 `applicationDirPath()/config.ini`（**不是源码目录**）。改 `Client/config.ini` **不会**自动进 `build/.../bin/config.ini`——`.pro` 的 QMAKE_POST_LINK `copy` 只在**重新链接 exe** 时触发，只改配置不 relink 就不拷。所以"改了 host 还连 localhost"= exe 旁那份旧的。**手动 copy 到 exe 目录即免重编生效**（改完重启客户端，启动仅读一次）。登录实测：`curl -X POST http://IP:8080/user_login -d '{"email":"...","passwd":"..."}'`，种子号 pwd 明文 `745230`。
6. **换号/新号联系人残留**：`tcpmgr.cpp` CHAT_LOGIN_RSP 的 `UserMgr::Clear()` 原只在 `_b_reconnecting`（重连）调，首次登录不清；`AppendFriendList` 是**追加**语义 → 新号登录会把上一个账号的好友带出来（现象：刚注册的新号联系人里显示老账号好友）。已改为**每次登录无条件 Clear**（先 `token=GetToken()` 再 `Clear()` 再 `SetToken(token)`）。UI 侧靠 `ChatDialog::applyLoginSession→reloadFriendItems`（removeFriendItems+重读）清干净。数据模型：`user` 表 id 是主键、uid 是业务号，`friend` 表用 uid。
7. **会话列表"Contact Name"假条目**：`ChatUserWid::SetChatData` 查不到对方(`GetFriendById` null，非好友/孤儿线程)时直接 return，控件保留 `.ui` 模板文字("Contact Name/Recent Message/12:00")。已在 `chatdialog.cpp::slot_load_chat_thread` 加 `GetFriendById(other_uid)==nullptr → continue` 过滤孤儿私聊线程（群聊靠群名兜底不受影响）。数据：`chat_thread` 只有(thread_id,type,created_at)，user1_id/user2_id 在 `private_chat`。
8. **弹窗阴影平台限制**：Win/MinGW 下 `Qt::Popup` + `WA_TranslucentBackground` + `QGraphicsDropShadowEffect` 会触发 `UpdateLayeredWindowIndirect failed`（面板残影/刷屏），`emojipanel.cpp:19` 已改"不透明窗口+圆角遮罩"规避。故**后端弹窗/表情面板/Popup 不要用图形阴影**；主窗口 QMainWindow 的阴影走另一路径、OK。P2 §4 "给弹窗加阴影"因此主动跳过。
