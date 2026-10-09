@echo off
chcp 936 >nul
title HChat 一键启动
setlocal

rem ============================================================
rem  HChat 一键启动脚本
rem
rem  用法：
rem    start.bat              启动全部（后端 4 个 + 客户端）
rem    start.bat backend      只启动后端，不启动客户端
rem    start.bat chat2        启动全部，但把客户端换成 ChatServer2
rem    start.bat gate         只启动 GateServer
rem    start.bat status       只启动 StatusServer
rem    start.bat chat         只启动 ChatServer 1
rem    start.bat varify       只启动 VarifyServer
rem    start.bat client       只启动 Qt 客户端
rem
rem  配合 stop.bat 可一键关闭全部
rem ============================================================

rem ---------- 定位工程根目录（pushd 能把 .. 规范化成干净的绝对路径）----------
pushd "%~dp0.." || (echo 找不到工程根目录 & pause & exit /b 1)
set "ROOT=%CD%"
popd

rem ---------- 各服务的 exe 绝对路径 ----------
rem 注意：start 命令会在切换工作目录【之前】解析程序路径，
rem      所以只给 /D 是不够的，程序路径必须写成绝对路径。
set "VARIFY_DIR=%ROOT%\VarifyServer"
set "GATE_EXE=%ROOT%\GateServer\x64\Debug\GateServer.exe"
set "STATUS_EXE=%ROOT%\StatusServer\x64\Debug\StatusServer.exe"
set "CHAT_EXE=%ROOT%\ChatServer\x64\Debug\ChatServer.exe"
set "CHAT2_EXE=%ROOT%\ChatServer2\x64\Debug\ChatServer.exe"
set "CLIENT_DIR=%ROOT%\Client\build\Desktop_Qt_6_5_3_MinGW_64_bit-Debug\bin"
set "CLIENT_EXE=%CLIENT_DIR%\Hchat.exe"

rem ping 代替 timeout：timeout 在输入被重定向时会报错，ping 不会
goto :main

:s_varify
echo  [单独] VarifyServer  gRPC :50051 ...
start "VarifyServer   :50051" /D "%VARIFY_DIR%" cmd /k "node server.js"
goto :done

:s_gate
echo  [单独] GateServer    HTTP :8080 ...
start "GateServer     :8080" /D "%ROOT%\GateServer" "%GATE_EXE%"
goto :done

:s_status
echo  [单独] StatusServer  gRPC :50052 ...
start "StatusServer   :50052" /D "%ROOT%\StatusServer" "%STATUS_EXE%"
goto :done

:s_chat
echo  [单独] ChatServer 1  TCP :8990 ...
start "ChatServer 1   :8990" /D "%ROOT%\ChatServer" "%CHAT_EXE%"
goto :done

:s_chat2
echo  [单独] ChatServer 2  TCP :8991  RPC :50056 ...
start "ChatServer 2   :8991" /D "%ROOT%\ChatServer2" "%CHAT2_EXE%"
goto :done

:s_client
echo  [单独] Qt 客户端 ...
start "HChat Client" /D "%CLIENT_DIR%" "%CLIENT_EXE%"
goto :done

:main
echo.
echo  ============================================================
echo    HChat 一键启动
echo    工程根目录: %ROOT%
echo  ============================================================
echo.

rem ---------- 0. 依赖检查 ----------
echo  [检查] MySQL 3308 / Redis 6380 ...

netstat -ano | findstr "LISTENING" | findstr ":3308 " >nul
if errorlevel 1 (echo     [警告] MySQL 没监听 - 注册和登录会失败) else (echo     MySQL 3308  OK)

netstat -ano | findstr "LISTENING" | findstr ":6380 " >nul
if errorlevel 1 (echo     [警告] Redis 没监听 - 验证码功能会失败) else (echo     Redis 6380  OK)
echo.

if /i "%~1"=="varify"  goto :s_varify
if /i "%~1"=="gate"    goto :s_gate
if /i "%~1"=="status"  goto :s_status
if /i "%~1"=="chat"    goto :s_chat
if /i "%~1"=="client"  goto :s_client

rem ---------- 1. VarifyServer :50051 ----------
echo  [1/5] VarifyServer   gRPC :50051 ...
start "VarifyServer   :50051" /D "%VARIFY_DIR%" cmd /k "node server.js"
ping -n 3 127.0.0.1 >nul

rem ---------- 2. GateServer :8080 ----------
echo  [2/5] GateServer     HTTP :8080 ...
start "GateServer     :8080" /D "%ROOT%\GateServer" "%GATE_EXE%"
ping -n 2 127.0.0.1 >nul

rem ---------- 3. StatusServer :50052 ----------
echo  [3/5] StatusServer   gRPC :50052 ...
start "StatusServer   :50052" /D "%ROOT%\StatusServer" "%STATUS_EXE%"
ping -n 2 127.0.0.1 >nul

rem ---------- 4. ChatServer :8990 ----------
echo  [4/5] ChatServer 1   TCP :8990  RPC :50055 ...
start "ChatServer 1   :8990" /D "%ROOT%\ChatServer" "%CHAT_EXE%"
ping -n 2 127.0.0.1 >nul

if /i "%~1"=="chat2"   goto :s_chat2
if /i "%~1"=="backend" goto :done

rem ---------- 5. Client ----------
echo  [5/5] Qt 客户端 ...
start "HChat Client" /D "%CLIENT_DIR%" "%CLIENT_EXE%"
goto :done

:done
echo.
echo  ============================================================
echo    启动完毕。每个服务在独立窗口里，关掉窗口即停止该服务。
echo    想一次全关:  运行  Start\stop.bat
echo  ============================================================
echo.
pause
