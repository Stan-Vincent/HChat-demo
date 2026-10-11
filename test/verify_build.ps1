$cl = "D:\VS\vs1\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
$msvc = "D:\VS\vs1\VC\Tools\MSVC\14.44.35207"
$sdkRoot = "D:\Windows Kits\10"
$ver = (Get-ChildItem "$sdkRoot\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$vcpkg = "D:\software\vcpkg\vcpkg\installed\x64-windows\include"
$mysql = "D:\Program Files\mysql-connector-c++-26.7.0-winx64\include"
$out = "$env:TEMP\hchat_verify"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$log = "$env:TEMP\verify_result.txt"
Remove-Item $log -ErrorAction SilentlyContinue

$base = @("/nologo","/c","/std:c++17","/EHsc","/MD","/D_CONSOLE","/DNDEBUG",
          "/DSPDLOG_HEADER_ONLY","/D_WIN32_WINNT=0x0A00","/DWINVER=0x0A00",
          "/D_ITERATOR_DEBUG_LEVEL=0",
          "/I","D:\QTcode\HChat\common",
          "/I","D:\QTcode\HChat\common\third_party\spdlog\include",
          "/I",$vcpkg, "/I","$vcpkg\jsoncpp", "/I","$vcpkg\hiredis", "/I",$mysql,
          "/I","$msvc\include",
          "/I","$sdkRoot\Include\$ver\ucrt",
          "/I","$sdkRoot\Include\$ver\shared",
          "/I","$sdkRoot\Include\$ver\um",
          "/I","D:\software\boost_1_92_0")

$files = @(
  "D:\QTcode\HChat\GateServer\GateServer.cpp",
  "D:\QTcode\HChat\GateServer\RedisMgr.cpp",
  "D:\QTcode\HChat\GateServer\LogicSystem.cpp",
  "D:\QTcode\HChat\GateServer\HttpConnection.cpp",
  "D:\QTcode\HChat\GateServer\MysqlDao.cpp",
  "D:\QTcode\HChat\ChatServer\ChatServer.cpp",
  "D:\QTcode\HChat\ChatServer\CSession.cpp",
  "D:\QTcode\HChat\ChatServer\LogicSystem.cpp",
  "D:\QTcode\HChat\ChatServer2\LogicSystem.cpp",
  "D:\QTcode\HChat\StatusServer\StatusServer.cpp"
)

$lines = @()
$fail = 0
foreach ($f in $files) {
  if (-not (Test-Path $f)) { $lines += "SKIP  $f"; continue }
  $svcDir = Split-Path $f -Parent
  $l = "$env:TEMP\one.log"
  & $cl @base @("/I",$svcDir,$f,"/Fo:$out\") *> $l
  $errs = @(Get-Content $l -ErrorAction SilentlyContinue |
            Where-Object { $_ -match "error C\d+|error LNK\d+|fatal error" })
  if ($errs.Count -gt 0) {
    $fail++
    $lines += "FAIL  $(Split-Path $f -Leaf)  [$svcDir]"
    foreach ($e in ($errs | Select-Object -First 5)) { $lines += "      " + $e.Trim() }
  } else {
    $lines += "OK    $(Split-Path $f -Leaf)  [$svcDir]"
  }
}
$lines += ""
$lines += "failed=$fail / $($files.Count)"
$lines | Out-File $log -Encoding UTF8
Write-Host "done, failed=$fail"