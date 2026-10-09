@echo off
chcp 936 >nul
title HChat 一键停止
setlocal

rem ============================================================
rem  关掉本项目启动的所有服务
rem
rem  做法：按监听端口反查 PID 再结束进程。
rem  端口表来自各服务的 config.ini：
rem    50051 VarifyServer      8080 GateServer       50052 StatusServer
rem    8990  50055 ChatServer 1                        8991  50056 ChatServer 2
rem  客户端不监听端口，按进程名结束。
rem
rem  这里用 PowerShell 而不是 cmd 内层的 for + netstat 管道，
rem  因为嵌套 for /f 里的管道在部分 Windows 上会静默失灵。
rem ============================================================

echo.
echo  正在关闭 HChat 所有服务 ...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ports = @(50051,8080,50052,8990,50055,8991,50056);" ^
  "$conns = Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object { $ports -contains $_.LocalPort };" ^
  "if ($conns) { $conns | Select-Object -Unique OwningProcess | ForEach-Object { $p = $ports -join ','; Write-Host ('    结束 PID ' + $_.OwningProcess); Stop-Process -Id $_.OwningProcess -Force -ErrorAction SilentlyContinue } } else { Write-Host '    没有找到正在监听的服务进程' };" ^
  "$cli = Get-Process Hchat -ErrorAction SilentlyContinue;" ^
  "if ($cli) { Write-Host '    结束客户端 Hchat.exe'; $cli | Stop-Process -Force -ErrorAction SilentlyContinue }"

echo.
echo  全部关闭完毕。
echo.
pause
