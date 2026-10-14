# Hcat 项目长期备忘（原名 HChat，2026-10-12 改名）

## 架构
- `Client/`：Qt 客户端（HttpMgr 发 HTTP 到 GateServer）
- `GateServer/`：C++ Boost.Beast HTTP 服务（端口 8080），依赖 vcpkg（grpc/protobuf/redis++/hiredis）+ MySQL Connector/C++（JDBC）
- `VarifyServer/`：Node.js gRPC 验证码服务（端口 50051），验证码写 Redis（6379→本项目用 6380）
- MySQL 3308 / Redis 6380 / GateServer 8080 / VarifyServer 50051（见 `GateServer/config.ini`）
- 运行 GateServer 的工作目录必须是 `GateServer/`（ConfigMgr 从 cwd 读 config.ini）

## 数据库
- 库 `hcat`（2026-10-13 实测纠正：旧记录"xxxl"已过时，config.ini 写 `Schema = Hcat`，MySQL 3308 上实际库名小写 `hcat`；另有 `llfc` 是教程遗留空库，勿混用）。表 `user(id,uid,name,email,pwd,nick,icon)`、`user_id(id PRIMARY KEY)`（手工维护自增，注册时 +1）
- 注册走 `MysqlDao::RegUserTransaction(name,email,pwd,icon)`（事务版），`MysqlDao::RegUser`（存储过程 `reg_user`，只有 3 个入参）保留但未使用

## 源码编码（改代码前务必注意）
- `GateServer/` 源码是 **GBK**，`Client/` 源码是 **UTF-8**。
  用 Edit 工具往 GBK 文件写中文会变乱码 → 改 GBK 文件时用 Python 脚本
  `decode('gbk')` 改完 `encode('gbk')` 写回，注释一律用 ASCII；注意 CRLF。

## 路由注册的重要坑
- `LogicSystem::RegPost/RegGet` 用的是 `_post_handlers.insert()`，**同一 URL 注册两次时
  后注册的静默失效**（std::map::insert 不覆盖已有键）。改 handler 前先 grep 确认没有重复。

## Client（Qt）约定
### 编译环境（★ 两个必须记住的事实）
- **Qt 的 MinGW（`D:\QT6\Tools\mingw1120_64`）不带 `sh.exe`**。`mingw32-make` 先找 PATH 里的 `sh`，
  找不到才用 cmd。所以同一份 Makefile：**Qt Creator 里构建用 cmd**（`copy` 内建命令可用），
  **Git Bash 里跑 make 用 sh**（`copy` 找不到 → Error 127；`xcopy /Y` 的 `/Y` 被 MSYS 当成
  POSIX 路径转换 → 「参数无效」Error 4）。要在 bash 里复现 Qt Creator 的行为，
  先清掉 PATH 里的 sh（用 PowerShell 设 `$env:PATH` 后调 `mingw32-make.exe`）。
- 命令行构建 Client **必须用 Qt 的 MinGW**，不要用 MSYS2 的 gcc：
  `export PATH=/d/QT6/Tools/mingw1120_64/bin:$PATH`。
  两套 MinGW 混编会出 `__emutls_v._ZSt11__once_call`（链接期）或
  「无法找到入口点 `__emutls_v._ZSt11__once_call`」（运行期，双击 exe 时）。

### 构建 / 运行约定
- 构建目录 `Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug`，跑 `mingw32-make -j4`。
- **`Hchat.pro` 的 post-link 是 cmd 语法**（`copy /Y` + `xcopy /Y /E /I`），且必须是
  **一条** `QMAKE_POST_LINK +=`、命令之间用 `&&` 连接 —— 分多次追加会被 qmake 用空格
  拼成一行，cmd 把后续命令全当成 copy 的参数 → `Error 1`（链接其实已成功，只是后置拷贝失败）。
- qmake 要用 bash 直接调用；`cmd //c qmake` 会写出损坏的 Makefile（uic 规则丢换行，
  报 `multiple target patterns`），**不要用**。
- **★ 重生成 Makefile 务必带 `-config debug`**：在本构建目录裸跑 `qmake Hcat.pro`（不带 `-config debug`）
  会把顶层 `Makefile` 的 `first` 默认目标从 `debug` 翻成 `release`，之后 `mingw32-make -j4` 改去编
  release（且 release 的 post-link `copy` 同样报 Error 127），**不再更新你跑的那个 debug exe**。
  正确姿势：`qmake -config debug -o Makefile Hcat.pro`。改 `.pro` 加新文件（如 animutil.cpp）后必须重生成。
- **★ 不要手删 `.qmake.stash` / `Makefile*` 来"重新生成"**：qmake 原生 exe 用 `system()` 探测
  编译器 include 路径，在 Git Bash 沙箱里"全新检测"会挂
  （`QMAKE_CXX.COMPILER_MACROS is not defined / failed to parse default include paths`）。
  只在"有缓存"时（构建目录原有 Qt Creator 生成的 Makefile/.qmake.stash）能跳过检测、正常生成。
  一旦手删缓存再 qmake 就挂。命令行想重建就直接在 **Qt Creator 里 Build**（它用 cmd+正确 PATH，
  qmake 探测正常），别在 bash 里硬刚 qmake。
- Qt 的 `D:\QT6\6.5.3\mingw_64\bin` **只有 qmake/Qt DLL，没有 g++.exe**；真正的编译器在
  `D:\QT6\Tools\mingw1120_64\bin`（11.2.0）。qmake 探测时 PATH 里若没有后者会回退 msys64 的
  g++ 16.1.0 而解析失败（同属"工具链探测"类问题）。
- `rc.qrc` 若引用了不存在的资源文件（如孤儿 `res/ice.png`），qmake 重生成 Makefile 会把它列为
  依赖，`mingw32-make` 报 `No rule to make target '../../res/xxx.png'` 整构建挂掉——这种孤儿引用
  直接删掉 `<file>` 行即可（不影响功能）。
- **删构建产物时保留 `bin` 目录**，否则链接报 `cannot open output file bin/Hchat.exe`。
- exe 同级必须有：MinGW 运行时 3 个 DLL、Qt6 运行时 4 个 DLL、`platforms/`（缺了报
  "no Qt platform plugin could be initialized"）、`styles/`、`imageformats/`、`config.ini`。
  这些由 post-link 自动拷贝，**不要手动删**。
- `mingw32-make clean` 删不干净 `.o`；**从教程复制文件会保留原始时间戳（09-02）**，
  比构建产物还旧 → make 判定"不用重编"，moc 不重跑 → 大量 undefined reference。
  解决：`touch *.cpp *.h *.ui *.qrc` 后全量重编；touch 了 `.pro` 还要手工先跑一次 qmake。
- 源码编码混用：`applyfriend.h` / `chatdialog.h` / `userdata.h` / `usermgr.h` 是 **GBK**，其余 UTF-8。
  批量脚本要按文件探测编码再写回。
- 分层：`main.cpp`（读 config.ini 拼 `gate_url_prefix`）→ `MainWindow`（setCentralWidget 切页）→
  Login/Register/Reset 三个 Dialog → `HttpMgr` 单例 → GateServer。
- 回包两级分发：`sig_http_finish` → `slot_http_finish` → 按 `Modules` 分流 `sig_reg/reset/login_mod_finish`
  → 各 Dialog 再用 `QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers` 查表回调。
  **Modules 决定发给谁，ReqId 决定用哪个处理函数。**
- **`_handlers[id](...)` 前必须先 `contains(id)`**：`QMap::operator[]` 会为不存在的 key 插入空
  `std::function`，调用即抛 `std::bad_function_call` 崩溃（已修）。
- 服务端 `/reset_pwd` 只返回 `error/email/user/varifycode`，**没有 uuid/uid**；`CheckEmail` 只返回 bool，
  `UpdatePwd` 按 name 更新，服务端拿不到 uid。
- 构建：Qt 6.5.3 MinGW 在 `D:\QT6\6.5.3\mingw_64`；命令行编译
  `export PATH=/c/msys64/mingw64/bin:$PATH` 后到 `Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug` 跑
  `mingw32-make -j4`。从 bash 跑时 `.pro` 里的 `copy /Y` 会报 Error 127（cmd 内建命令），可忽略。
- **改 `style/stylesheet.qss` 等资源文件后，make 不会自动重编 `debug/Hcat_resource_res.o`**。若发现 qss 改动未生效（如按钮颜色还是旧的），手动 `rm -f debug/Hcat_resource_res.o` 后再 `mingw32-make -j4`，触发 `windres` 重新打包 `.qrc`。

## 重要构建/运行约定（GateServer）
- **改 GateServer 源码后必须重新生成 Debug|x64**：`GateServer.vcxproj` 的 `Debug|x64` 已被改造为
  "Release 运行时 + 无优化"（UseDebugLibraries=false、/MD+NDEBUG+_ITERATOR_DEBUG_LEVEL=0、链 vcpkg
  release 库、hiredis.lib），可正常连 MySQL 并断点调试。
- **★ start.bat 启动的是 `GateServer\x64\Debug\GateServer.exe`**（看其 GATE_EXE 路径）。
  所以重新生成必须用 **Debug|x64** —— 若按 "Release|x64" 生成，产物落到 `x64\Release\`，start.bat
  永远加载不到，等于白改。这是最容易踩的坑：改了代码、重新生成、重启，跑的却还是旧 exe。
  （早期记忆写的"GateServer 只能用 Release 配置运行"已过时：那是改造前默认 Debug(/MDd) 会崩；
  现在 Debug|x64 已改造且 start.bat 用的就是它；Release|x64 也能跑但 start.bat 不用。）
- exe 同级必须有 `plugin\` 目录（拷自 `<连接器>\lib64\plugin`），否则报
  "Authentication plugin 'mysql_native_password' cannot be loaded"（root 用户用的是该插件）。
- Debug 输出目录的 DLL 必须与 exe 的 CRT 一致：vcpkg 的 abseil_dll/cares/libssl-3-x64/re2/jsoncpp
  的 Debug 与 Release **文件名相同**，切换运行时后必须用 Release 版覆盖，否则要么加载失败（依赖
  MSVCP140D.dll），要么 ABI 错乱。已把残留 Debug 版 DLL 移到 `x64/Debug/stale_debug_dlls/`。

## 配置核查更正（2026-10-11 复查，纠正误判）
- 两台 ChatServer 的 `config.ini` `Name` **没反、自洽**：`ChatServer/`=chatserver1(8990/50055)，
  `ChatServer2/`=chatserver2(8991/50056)，各自 peer 段指向对方 RPC 端口。不要对调。
- `VarifyServer` **不连 MySQL**（只用 Redis 写验证码 + email 发信），其 `config.json` 的 mysql 块
  是教程残留，无需补 port。
- 教训：排障断言前必须读实际配置 + 追踪代码引用，别凭单条日志下结论。

- 用户是 C++ 后端方向、中文交流、喜欢直接给结论和证据。

## UI（.ui）文件编辑硬规则
- **顶层 `</widget>` 必须在 `<customwidgets>` 之前**：手写/重写 .ui 极易把对话框的 `</widget>` 漏写到
  `<customwidgets>` 之后，导致 customwidgets 被嵌套进 widget → uic 报 "Unexpected element customwidgets"
  （且不会静默丢控件，而是直接校验失败）。正确收尾顺序：
  `</layout>` → `</widget>`(关对话框) → `<customwidgets>` → `<resources/>` → `<connections/>` → `</ui>`。
- 改完 .ui 必跑 `uic xxx.ui -o /tmp/t.h` 校验，再编译对应 .o（make 会自动重生成 ui_xxx.h）。
- 删 .ui 里的 QLabel 等控件前，先 grep 全工程 `ui->控件名`，确认 .cpp/.h 无引用再删；
  若被引用（如 head_label、pass_visible、各 *_edit/*_btn），只能隐藏或保留，不能删 widget 名。
- 输入框想"文字显示在框内"：删左侧标签列，给 QLineEdit 加 `placeholderText` 属性即可（Qt6 支持）。

## Client 主窗口尺寸必须与登录/注册/重置页同步
- 登录/注册/重置是 `MainWindow` 里 `QStackedWidget` 的页面，不是独立顶层窗口。`MainWindow` 的尺寸必须和当前页面对话框的 `minimumSize/maximumSize` 完全一致，否则对话框会在右侧被裁，表现为"左边空隙大、右边空隙小"。
- 改 `Client/mainwindow.cpp` 时，所有切到登录/注册/重置的入口（构造、`slot_switch_login`、`slot_switch_register`、`slot_switch_reset`、`offlineLogin`）都要按目标对话框尺寸重设 `MainWindow` 的 min/max/resize，推荐直接读对话框自身的 `minimumSize()/maximumSize()`，避免硬编码数字在改 .ui 后再次过期。
- 进入聊天页 `slot_switch_chatdlg` 前必须 `setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)`，清除登录页残留的窄上限，否则退出登录再登录后聊天窗口会被锁在 360 宽。

## Client 无边框 MainWindow 实现要点
- 登录/注册/重置/聊天都是 `QStackedWidget` 页面，无边框应做在 `MainWindow` 本身，而不是对话框（子页设 `FramelessWindowHint` 无效）。
- 实现结构：`MainWindow`（Frameless + WA_TranslucentBackground）→ 中央容器（四周留白）→ `main_widget`（圆角+白底+阴影+QGraphicsDropShadowEffect）→ [标题栏][_stacked_widget]。
- 圆角必须配合 `QPainterPath::addRoundedRect` + `setMask` 做圆角遮罩，否则透明背景会露出方形锯齿。
- 最大化/还原时要去掉圆角、阴影和四周留白，让窗口铺满屏幕。
- 拖动做在 `MainWindow` 的 `mousePressEvent`/`mouseMoveEvent` 中；必须跳过按钮、输入框、下拉框等交互控件，否则点击控件会变成拖动。
- 边缘缩放：Qt 6.5.3 MinGW 构建里没有 `startSystemResize`，需自写在 `mouseMoveEvent` 中按 `Qt::Edges` 调 `setGeometry`。
- 尺寸切换要给内容尺寸加上 `2*kShadowMargin`；若当前页显示标题栏，还要再加 `kTitleBarHeight`，否则内容会被标题栏压扁。
- 小窗（登录/注册/重置）不要顶部标题栏条，改在页面右上角浮“最小化 + 关闭”两颗按钮（像 QQ 登录页），避免占用 34px 内容高度；聊天大窗再显示完整标题栏+最小/最大/关闭三键。
- 小窗浮动按钮不要在 `updateWindowControls()` 里直接用 `_main_widget->width()` 定位（切页时可能还没 layout），应通过 `QTimer::singleShot(0, ...)` 延迟到首帧完成，并在 `resizeEvent()` 中持续跟随。
- 切页 / 退出登录时必须先更新 `_ui_status` 再调用 `updateWindowControls()`，否则 `updateWindowControls()` 按旧状态判断（如从聊天页退出登录时仍认为是大窗），导致小窗错误显示标题栏、聊天页漏标题栏。
- 窗口按钮统一 `setFixedSize(30,30)` 正方形、`tbl->setSpacing(0)` 紧排（参考 QQ），字符用 `−`(最小化)/`□`(最大化)/`×`(关闭)；qss 统一 `font-size:13px;padding:0;text-align:center;`，hover 最小/最大化灰底、关闭变红。

## Client UI 按钮体系（Design Token）
- 全局按钮统一用 `role` 动态属性 + qss 属性选择器实现，qss 单一来源、零内联样式。
- `style/stylesheet.qss` §9 定义调色板（主色 `#ff7a45`、hover `#ff9a6b`、press `#e96a30`、disabled `#ffd2bf`、文字 `#1f1f1f`、描边 `#e3e3e3`）与三类按钮：
  - `QPushButton[role="primary"]` 实心主色（登录/注册确认/重置确认/提交/上传主操作）；
  - `QPushButton[role="ghost"]` 描边浅底（取消/上传头像）；
  - `QPushButton[role="link"]` 文字橙（去注册/返回/退出/获取验证码）。
- 用法惯例（与项目既有 `setProperty("state",...)+reload_qss()` 同构）：在对话框构造函数 `setupUi` 后
  `ui->xxx->setProperty("role","primary|ghost|link"); reload_qss(ui->xxx);`。`reload_qss` 在 `global.h` 声明、`global.cpp` 定义为 `unpolish+polish`。
- 聊天栏 `send_btn`/`receive_btn` 是 `ClickedBtn`，已有基于 `[state=...]` 的自绘圆角胶囊样式（qss 312-318），**不纳入 role 体系**，避免破坏聊天 UI。
- 改按钮样式前先 grep `setProperty("role"` 与 qss §9，确认归属哪一类；新增主操作按钮直接设 `role="primary"` 即可，不要再写内联 styleSheet。
- **★ `reg_btn`（登录页"注册账号"）必须是 QQ 式嵌入背景文字**：绝不能出现在 §8 实心橙色主按钮组（`#login_btn,#submit_btn,#sure_btn,#up_btn`，含 hover/pressed/disabled 四态）里，否则会被实心色覆盖。它只由 §9 专属 `QPushButton#reg_btn`（四态全 `background-color:transparent` + 橙字 `#ff7a45`/hover `#ff9a6b`/pressed `#e96a30`/disabled `#bbbbbb`）管控。`role="link"` 选择器在该 MinGW 构建下对 `reg_btn` 不命中，故必须保留 `#reg_btn` objectName 兜底选择器。
