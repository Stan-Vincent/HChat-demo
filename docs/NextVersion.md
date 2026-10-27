# Hcat NextVersion — 待完善项清单

> 基于对当前代码库（后端服务 + Qt 客户端 + 用户体验）的实际核查整理。
> 每项都标注了**代码证据**，方便定位。
>
> 状态图例：`[ ]` 待办 · `[~]` 进行中 · `[x]` 已完成
> 贡献约定：**欢迎直接补充你的体验感受和改进方案**，在下方对应小节追加即可。

---

## 📌 优先级总览

| 优先级 | 模块 | 事项 | 状态 |
|---|---|---|---|
| P0 | 客户端 | 聊天链路错误无用户提示 | `[x]` 已完成 |
| P0 | 客户端 | Enter 发送快捷键 | `[x]` 已完成 |
| P0 | 客户端 | 槽函数无 try/catch 兜底 | `[x]` 已完成 |
| P1 | 后端 | 结构化日志 | `[x]` 已完成 |
| P1 | 后端 | Redis 重连 + 健康检查 | `[~]` 代码已改，待重编验证 |
| P1 | 客户端 | 断线自动重连 | `[x]` 已完成 |
| P2 | 后端 | token 生命周期 | `[ ]` |
| P2 | 后端 | HTTP 连接复用（keep-alive） | `[ ]` |
| P2 | 客户端 | 图片上传前校验 | `[ ]` |
| P3 | 后端 | 优雅停机 | `[ ]` |
| P3 | 后端 | 多副本 / 去单点 | `[ ]` |

---

## 🖥 后端服务

### 1. 结构化日志（P1）~~✅ 已完成~~

**现状**：全靠 `std::cout` / `qDebug`，无级别、无时间戳、无请求 ID。线上出问题只能翻控制台。

- [x] 引入 spdlog（header-only + bundled fmt，零额外依赖），统一门面 `common/logger.h`
- [x] 后端 289 处 `std::cout` 甄别转换 → `LOG_INFO/WARN/ERROR`
- [x] 客户端 216 处 `qDebug/qWarning` → `qCDebug(lcNetwork/lcChat/lcUi/lcUser/lcAvatar)`
- [x] **安全修复**：3 处明文密码打印 + 1 处 token 明文打印 → 全部脱敏
- [x] 删除 3 处 `receive body is ...` 全量报文打印（噪音 + 性能 + 泄露风险）
- [x] 审计脚本 `test/audit_log_calls.py`（防占位符/参数不匹配导致**运行时**抛异常）
- [ ] ~~分级可配置~~ 已实现：环境变量 `HCHAT_LOG_LEVEL` / `QT_LOGGING_RULES`
- [ ] 请求 ID 串联（便于追踪单次请求全链路）

**代码位置**：`common/logger.h`、`Client/logcategory.h`、`test/audit_log_calls.py`

**使用方式**：
```cpp
// 后端
hcat::log::init("gate");            // main 里调一次
LOG_INFO("user {} login ok, uid={}", name, uid);

// 客户端
qCDebug(lcNetwork) << "recv msg id =" << id;
$env:QT_LOGGING_RULES="hcat.network.debug=true"   # 打开调试
$env:HCHAT_LOG_LEVEL="debug"                        # 后端打开调试
```
日志落盘：`logs/<module>_<日期>.log`，按天轮转保留 7 天。

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 1b. 安全：密码/token 明文打印 ~~✅ 已修复~~

原代码在 4 处把**数据库明文密码**和**登录 token** 直接打进日志，属于严重安全隐患，已全部脱敏：
- `GateServer/MysqlDao.cpp`、`ChatServer/MysqlDao.cpp`、`StatusServer/MysqlDao.cpp`（密码比对处）
- `ChatServer/LogicSystem.cpp`（chat 登录 token）

**遗留**：数据库本身仍存明文密码（未做加盐哈希），属独立议题。

---

### 2. Redis 重连 + 健康检查（P1）~~🔧 代码已完成，待重编验证~~

**原现状**：GateServer 的 `RedisMgr` 只有 Get/Set/HSet 等封装，**完全没有重连逻辑**；
ChatServer 版有 PING 巡检但无退避、间隔 60 秒，且 `reconnect()` 里 `reply->type` 未判 NULL（会崩）。

**已做（2026-10-21）**：
- [x] 连接池抽成共享一份 `common/RedisConPool.h`（原来是 ChatServer/GateServer 两份不同实现）
- [x] 巡检间隔 60s → **可配置，默认 5s**；命令超时 2s（避免 PING 永久阻塞巡检线程）
- [x] 补齐连接改**指数退避** 1s→2s→4s…上限 30s（Redis 重启时别高频重连把它再打爆）
- [x] 修 `reconnect()` 空指针（`reply` 先判空再判类型）、`host_/pwd_` 传 `std::string` 给 hiredis 的问题
- [x] GateServer 新增 `GET /health`：探 Redis + MySQL，任一不可用返回 **503**（fail-fast）
- [x] `RedisMgr::Ping()` / `MysqlDao::Ping()` / `MysqlMgr::Ping()` 探活原语
- [x] 测试 `test/redis_reconnect_test.py`：用 `CLIENT PAUSE` 模拟抖动，验证 degraded→自愈

- [ ] gRPC 下游（StatusServer/VarifyServer）探活未纳入 /health
- [ ] **待用户 VS 重编 GateServer + ChatServer 后跑测试验证**

**代码位置**：`ChatServer/RedisMgr.cpp`（全文件只有业务封装，无 Reconnect）；`GateServer/RedisMgr.cpp` 同理

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 3. token 生命周期（P2）

**现状**：登录生成 token 存 Redis，聊天连接时取出比对（`LogicSystem.cpp:140-149`）。但**无过期时间、无刷新、不主动失效**。

- [ ] token 设置 TTL
- [ ] 支持续期
- [ ] 改密码 / 登出时主动删除 Redis 中的 token
- [ ] 考虑签名 token（HMAC）替代纯 Redis 存储

**代码位置**：`ChatServer/LogicSystem.cpp:127-149`（`USERTOKENPREFIX + uid_str` 比对）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 4. HTTP 连接复用（keep-alive）（P2）

**现状**：每个响应都 `keep_alive(false)`，写完即关连接。客户端需反复建连，效率损失。

- [ ] 移除 `_response.keep_alive(false)`
- [ ] 补齐连接关闭、超时、Keep-Alive 头处理

**代码位置**：`GateServer/HttpConnection.cpp:33`、`GateServer/HttpConnection.cpp:175`

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 5. 优雅停机（P3）

**现状**：收到 SIGINT/SIGTERM 会 `io_context.stop()`，但**不等待在途请求处理完**。

- [ ] 停止 accept 新连接
- [ ] 等待存量 session 收尾
- [ ] 关闭 DB / Redis 连接

**代码位置**：`ChatServer/ChatServer.cpp:68-70`（`signal_set` + `io_context.stop()`）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 6. 多副本 / 去单点（P3）

**现状**：GateServer / StatusServer 均为单实例。

- [ ] StatusServer 考虑多副本 + Redis 选主 / 一致性
- [ ] GateServer 无状态化后可水平扩容（当前基本具备）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

## 🖼 Qt 客户端

### 7. 聊天链路错误无用户提示（P0）~~✅ 已完成~~

**现状（已完成时）**：头像加载失败、图片上传失败、消息发送失败等全部只进控制台，用户界面毫无感知。

- [x] 复用右上角轻量 Toast（`Client/widgets/toast.h`，Info/Success/Warning/Error 四态、自动消失、不阻塞）
- [x] 图片上传失败（`chatpage.cpp` 发送 / 重发两条路径）→ Toast 报错 + 气泡保持失败态
- [x] 文件上传失败 → Toast 报错 + 气泡标失败态
- [x] ~~消息发送失败 → 补一个点击重发~~ **早已实现**（`ChatPage::slot_resend`：点失败图标重发，
  文本直接发 / 图片重新上传；`armResendTimer` 8 秒无回包自动标失败；`_resend_ctx` 切会话清理）
- [ ] 聊天记录加载失败 → 提示 + 重试入口
- [ ] 头像/图片**下载**失败不弹 Toast（避免噪音，符合“只提示用户主动操作的结果”原则）

**代码位置**：`chatpage.cpp`（`qDebug() << "load bubble image failed"`、`"upload image failed"`）、`imagemanager.cpp`（`qDebug() << "download image failed"`）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 8. Enter 发送快捷键（P0）

**现状**：发送必须点按钮，Enter 只能换行。与主流 IM 习惯不符。

- [ ] Enter 发送 / Shift+Enter 换行（或 Ctrl+Enter 发送）
- [ ] 同步作用于文本与图片混排

**代码位置**：`Client/MessageTextEdit.cpp`（需重写 `keyPressEvent`）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 9. 槽函数 try/catch 兜底（P0）~~✅ 已完成~~

**现状（已完成时）**：全项目 0 个 try/catch。Qt 槽里抛异常会 `std::terminate` 直接崩。此前 `_handlers[id]` 的 `std::bad_function_call` 就是典型案例（已修根因，但无兜底）。

- [x] **在 `TcpMgr::handleMsg` 统一兜底**：包住所有网络回包 handler 的调用，
  异常时打 `qCWarning` 并吞掉，不再穿透 Qt 事件循环导致 terminate。
  选这一处是因为它是**全部回包的必经之路**，一处覆盖所有 handler，不必逐个槽撒网。
- [ ] 其余 UI 槽（按钮点击等）暂未包——它们不处理外部输入，抛异常概率低，
  且真抛了也有 handleMsg 这层保险兜底大部分场景。

**代码位置**：`chatdialog.cpp` 各 `slot_*`、`tcpmgr.cpp` 的 `_handlers`、`httpmgr.cpp` 回包

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 10. 断线自动重连（P1）

**现状**：心跳超时直接弹"该终端下线"后结束，用户需手动重登。短暂网络抖动也会掉线。

- [ ] 指数退避自动重连 TCP
- [ ] 重连成功后自动恢复会话列表与当前聊天
- [ ] 区分「主动退出」与「异常断开」

**代码位置**：`mainwindow.cpp:131`（下线提示）；`tcpmgr.cpp:108`（`disconnected` 处理）、`ChatDialog` 的 `_timer`（10s 心跳）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

### 11. 图片上传前校验（P2）

**现状**：文本有 1024 字限制，但**图片上传前不校验大小/类型**，超大图会先读进内存再被后端拒。

- [ ] 上传前校验大小与格式
- [ ] 超限时即时提示，别等上传失败

**代码位置**：`chatpage.cpp`（`on_send_btn_clicked` 图片分支）；`MessageTextEdit.cpp:151`（仅文件有 100M 校验，图片无）

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

## 🎨 用户体验（其他）

> 这部分留给你补充。登录 / 注册 / 重置链路的体验已经打磨得不错
> （表单校验 + `showTip` 错误提示 + 登录态失效提示三件套），
> 但以下维度我还没系统梳理：

- [ ] 会话列表排序 / 未读数展示
- [ ] 消息列表滚动性能（大量历史消息）
- [x] 表情面板（Unicode emoji，复用 TEXT 消息链路，零后端改动；自发贴纸/语音/截图仍待做）
- [ ] 截图、语音等富功能
- [ ] 深色模式 / 主题切换
- [ ] 快捷键体系（Ctrl+F 搜索、Ctrl+, 设置等）
- [ ] 空状态设计（无好友、无会话时的引导）
- [ ] 国际化 / 多语言
- [ ] 移动端或响应式

**我的体验感受 / 改进方案**：
<!-- 在此填写 -->

---

## 📝 其他想法

<!-- 自由追加，不需要归类 -->

| 日期 | 来源 | 内容 |
|---|---|---|
| | | |