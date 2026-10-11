$ErrorActionPreference = "Continue"
$cl = "D:\VS\vs1\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
$msvc = "D:\VS\vs1\VC\Tools\MSVC\14.44.35207"
$sdkRoot = "D:\Windows Kits\10"
$ver = (Get-ChildItem "$sdkRoot\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$vcpkg = "D:\software\vcpkg\vcpkg\installed\x64-windows\include"
$mysql = "D:\Program Files\mysql-connector-c++-26.7.0-winx64\include"
$base = @("/nologo","/c","/std:c++17","/EHsc","/MD","/DNDEBUG","/DSPDLOG_HEADER_ONLY","/D_ITERATOR_DEBUG_LEVEL=0","/D_WIN32_WINNT=0x0A00","/DWINVER=0x0A00","/I","D:\QTcode\HChat\common","/I","D:\QTcode\HChat\common\third_party\spdlog\include","/I",$vcpkg,"/I","$vcpkg\jsoncpp","/I","$vcpkg\hiredis","/I",$mysql,"/I","$msvc\include","/I","$sdkRoot\Include\$ver\ucrt","/I","$sdkRoot\Include\$ver\shared","/I","$sdkRoot\Include\$ver\um","/I","D:\software\boost_1_92_0")
$fail = 0; $results = @()
foreach ($svc in "ChatServer","ChatServer2","StatusServer","GateServer") {
  $files = Get-ChildItem "D:\QTcode\HChat\$svc" -Filter *.cpp
  foreach ($f in $files) {
    $l = "$env:TEMP\one_all.log"
    & $cl @base @("/I", "D:\QTcode\HChat\$svc", $f.FullName, "/Fo:$env:TEMP\") > $l 2>&1
    $errs = @(Get-Content $l -ErrorAction SilentlyContinue | Where-Object { $_ -match "error C\d+|fatal error" })
    if ($errs.Count -gt 0) {
      $fail++
      $results += "FAIL $svc\$($f.Name)"
      $results += ($errs | Select-Object -First 6)
    }
  }
}
if ($fail -eq 0) { $results += "ALL OK" }
$results + "failed=$fail" | Out-File "$env:TEMP\all_result.txt" -Encoding UTF8
