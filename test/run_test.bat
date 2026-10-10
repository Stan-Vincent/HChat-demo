@echo off
chcp 936 >nul
title HChat 自动化测试
setlocal enabledelayedexpansion

rem ============================================================
rem  编译 ChatServer 后跑这个脚本，自动验证服务端能不能扛住
rem  各种异常 / 边界输入。
rem
rem  前提：MySQL / Redis / VarifyServer / GateServer / StatusServer
rem        已经在跑（可以直接跑 Start\start.bat backend）
rem
rem  退出码：0=全过  1=有用例异常  2=环境没就绪  3=服务端被打挂
rem
rem  ★ 三个踩过的坑，改这个脚本时务必保持：
rem    1) if 块内的 echo 里不能出现未转义的右括号 —— cmd 会把它当成块的结束符，
rem       导致块内后续命令跑到块外，整个脚本语法错乱直接闪退。
rem       要写括号必须用 ^( ^) 转义，或者干脆写成子程序。
rem    2) for 循环里的 set 要用 !变量! 才是即时展开，用 %变量% 拿到的是
rem       循环开始前的旧值（cmd 的延迟展开）。
rem    3) 等待服务起来要用 ping -n N 127.0.0.1 >nul，不要用 timeout ——
rem       timeout 在输入被重定向时会直接返回不等待，导致误判成启动失败。
rem ============================================================

set "ROOT=%~dp0.."
cd /d "%ROOT%"

echo.
echo ============================================================
echo   HChat 自动化测试
echo ============================================================
echo.

rem ---------- 1. 依赖自检 ----------
rem  用子程序而不是 for 循环，绕开延迟展开问题
echo [1/3] 检查依赖...

set "MISSING="
call :checkPort 3308 "MySQL"
call :checkPort 6380 "Redis"
call :checkPort 50051 "VarifyServer"
call :checkPort 8080 "GateServer"
call :checkPort 50052 "StatusServer"

if not "!MISSING!"=="" (
    echo.
    echo   [错误] 下面这些没启动: !MISSING!
    echo   先运行:  Start\start.bat backend
    echo.
    pause
    exit /b 2
)
echo     依赖全部 OK
echo.

rem ---------- 2. ChatServer 没跑就拉起来 ----------
call :checkPort 8990 "ChatServer"
if "!MISSING!"=="" goto chatserver_alive

echo     正在自动启动 ChatServer ...
cd /d "%ROOT%\ChatServer"
start "ChatServer" x64\Debug\ChatServer.exe
cd /d "%ROOT%"

rem 轮询等待，最多约 15 秒。ChatServer 要初始化 5 个 Redis 连接池 + MySQL 懒加载，
rem 实测要 4~5 秒才监听 8990，固定 sleep 3 秒会误判成启动失败。
set /a __wait=0
:wait_chatserver
call :checkPort 8990 "ChatServer"
if "!MISSING!"=="" goto chatserver_ready
if !__wait! GEQ 30 goto chatserver_failed
ping -n 2 127.0.0.1 >nul
set /a __wait+=1
goto wait_chatserver

:chatserver_ready
set "MISSING="
echo     ChatServer 已启动
goto chatserver_done

:chatserver_failed
echo.
echo     [错误] ChatServer 启动失败，请手动运行看报错
echo.
pause
exit /b 2

:chatserver_alive
echo     ChatServer 已在运行

:chatserver_done
echo.

rem ---------- 3. 跑协议测试 ----------
echo [2/3] 跑协议层测试 ...
echo.
python test\protocol_test.py
set "TEST_RC=!errorlevel!"

rem ---------- 4. 判定 ----------
echo.
echo [3/3] 判定 ...
set "MISSING="
call :checkPort 8990 "ChatServer"
if not "!MISSING!"=="" (
    echo.
    echo   #######  ChatServer 已被测试打挂  #######
    echo   说明还有未修复的崩溃点，去看 ChatServer 控制台最后几行日志。
    echo.
    pause
    exit /b 3
)
echo     ChatServer 存活

if "!TEST_RC!"=="0" (
    echo.
    echo   全部用例通过，服务端扛住了所有异常输入。
    echo.
    pause
    exit /b 0
)

echo.
echo   有用例执行异常，看上面标 x 的行，但服务端没崩。
echo.
pause
exit /b 1

rem ---------- 端口检查子程序 ----------
:checkPort
netstat -ano | findstr "LISTENING" | findstr ":%~1 " >nul
if errorlevel 1 (
    if not "!MISSING!"=="" (
        set "MISSING=!MISSING!, %~2:%~1"
    ) else (
        set "MISSING=%~2:%~1"
    )
)
exit /b 0