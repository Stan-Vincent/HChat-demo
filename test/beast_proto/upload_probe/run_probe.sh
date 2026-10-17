#!/usr/bin/env bash
# 起 mock_gate + 跑 upload_probe（两轮：正常 / 拒收），全程一条命令内完成，
# 避免后台进程被沙箱清理。
#
#   ./run_probe.sh
set -u

HERE=/d/QTcode/HChat/test/beast_proto
PROBE=$HERE/upload_probe
OUT=$HERE/mock_upload

export PATH=/d/QT6/Tools/mingw1120_64/bin:/d/QT6/6.5.3/mingw_64/bin:$PATH

cd "$HERE"
rm -rf "$OUT" && mkdir -p "$OUT"

run_round() {
    local cap="$1" mode="$2" port="$3" label="$4"
    echo "==================================================="
    echo "[round] $label   cap=$cap  port=$port"
    echo "==================================================="
    "$HERE/mock_gate.exe" "$port" "$OUT" "$cap" > "$HERE/mock_$mode.log" 2>&1 &
    local srv=$!
    for i in $(seq 1 60); do
        if grep -q "listening" "$HERE/mock_$mode.log" 2>/dev/null; then break; fi
        sleep 0.2
    done
    "$PROBE/upload_probe.exe" "$port" "$OUT" "$mode"
    local rc=$?
    kill $srv 2>/dev/null
    wait $srv 2>/dev/null
    return $rc
}

rc1=0; rc2=0
run_round 314572800 main   18082 "正常上传（上限 300MB）" || rc1=$?
run_round 1048576   reject 18083 "服务端拒收（上限 1MB，客户端发 2MB）" || rc2=$?

echo
echo "==================================================="
echo "round-main   rc=$rc1"
echo "round-reject rc=$rc2"
echo "==================================================="
echo "--- 落盘文件 ---"
ls -la "$OUT" | head -20
exit $(( rc1 | rc2 ))
