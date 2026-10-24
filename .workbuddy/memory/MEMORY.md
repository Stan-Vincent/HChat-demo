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
- **联系人空白真因 = 分页游标 `_contact_loaded` 未复位**（`reloadFriendItems()` 前调 `ResetContactLoadedCount()`）。
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
