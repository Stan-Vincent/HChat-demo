# 2026-10-10（补充七）：ChatServer2 同步 + 编译脚本落盘

## 用户的批评（成立）
"你能自己编译你早编啊，还要麻烦我" —— 批评对。cl.exe 明明能用，
我却在 20:30 就发现了还让你编译了 4 次。以后改服务端代码先 `bash test/clbuild.sh <服务名>`。

## ★ ChatServer2 漏同步（用户提醒的）
ChatServer2 是 10-09 复制的 ChatServer，**今天所有修复它都没有**，
对比发现 **18 个文件全部不同**（LogicSystem/utils/MysqlDao/MysqlMgr/const/RedisMgr/
ChatServiceImpl/ChatGrpcClient/AsioIOServicePool/ConfigMgr/message.pb.* 全落后）。

**已全量同步源码**（`config.ini` 保留它自己的配置）：

| | Name | TCP | RPC |
|---|---|---|---|
| ChatServer | chatserver1 | 8990 | 50055 |
| ChatServer2 | chatserver2 | 8991 | 50056 |

同步后 cl.exe 验证 **16/16 全通过**。

⚠️ **教训**：以后复制服务副本后，改动必须双写。ChatServer2 的编译验证要单独跑。

## ★★ 编译脚本已落盘：`test/clbuild.sh`
```bash
bash test/clbuild.sh                # ChatServer
bash test/clbuild.sh ChatServer2    # ChatServer2
bash test/clbuild.sh GateServer
bash test/clbuild.sh ChatServer LogicSystem.cpp   # 只编某几个文件
```
关键点：
- `export MSYS2_ARG_CONV_EXCL="*"` —— 否则 MSYS 把 `/nologo` 当路径转换掉
- 手工补 vcvars 的等价物：MSVC include + Windows SDK ucrt/um/shared/winrt
- 输出 GBK 日志需 iconv 转码后再 grep `error C[0-9]+`
- 注意 `grep -c` 在 0 匹配时返回 "0" 且退出码 1，`|| echo 0` 会多补一个 0，
  判断要用字符串比较而不是 `RC != 0`

## 当前四个服务的验证状态
| 服务 | 结果 |
|---|---|
| GateServer | 11/11 ✅ |
| StatusServer | 9/9 ✅ |
| ChatServer | 16/16 ✅ |
| ChatServer2 | 16/16 ✅ |

协议测试 24 个用例全过，空搜索不再打挂 ChatServer。

## 待办（客户端那半边）
图片上传 → 发包 → 接收端渲染 → 头像上传 → 提交保存。
服务端侧（/upload_image、/update_userinfo、下载路由、chat_message 加 msg_type）已就绪，
但 GateServer 的下载路由还需要用户编译（commit() 修复后我只能验证源码编译，没法验证运行时字节）。