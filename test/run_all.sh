#!/usr/bin/env bash
# ============================================================
#  Hcat 一键回归：把三套回归脚本串起来跑
#
#    [1] ChatServer 协议测试   test/protocol_test.py    28 用例
#    [2] GateServer HTTP e2e   test/gate_e2e_test.py    20 用例
#    [3] 大文件上传（流式）    test/upload_big_test.py   4 用例
#    [4] 群聊端到端            test/group_chat_test.py   9 用例
#
#  ★ [4] 依赖服务端【新编译】的群聊代码（按 thread type 分支投递 + 建群/加人接口）。
#     服务端还是旧版时它会整片失败，用 SKIP_GROUP=1 跳过。
#
#  用法：
#    bash test/run_all.sh                 # 只跑已就绪的；缺服务会列出来，退出码 2
#    bash test/run_all.sh --start         # 缺什么起什么，跑完自动关掉自己起的
#    bash test/run_all.sh --start --keep  # 起了就不关
#    SKIP_PROTOCOL=1 bash test/run_all.sh # 跳过某一套
#    GATE_URL=http://127.0.0.1:18080 bash test/run_all.sh   # [3] 打 mock_gate
#    RUN_MOCK=1      bash test/run_all.sh                   # 加跑 beast_proto 套件
#
#  退出码：0=全绿  1=有用例失败  2=服务没起齐
#
#  ★ 为什么不用 run_test.bat：它结尾有 pause，非交互环境下会挂住；
#    而且它只跑 [1]。这里直接调 python，自己管预检和汇总。
# ============================================================
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 2
LOGDIR="$ROOT/test/_logs"
mkdir -p "$LOGDIR"

AUTO_START=0
KEEP=0
for a in "$@"; do
    case "$a" in
        --start) AUTO_START=1 ;;
        --keep)  KEEP=1 ;;
        -h|--help) sed -n '2,25p' "${BASH_SOURCE[0]}"; exit 0 ;;
    esac
done

PY="${PYTHON:-python}"

# ★ 必须用 $'...'（ANSI-C quoting），否则变量里存的是字面量 \033，
#   printf %s 不会解释它，终端上就会真的打出 "\033[31m" 这种东西。
GREEN=$'\033[32m'; RED=$'\033[31m'; YELLOW=$'\033[33m'; RESET=$'\033[0m'

# ---------- 端口探测 ----------
# netstat 在 Windows 上最稳；/dev/tcp 在某些 MSYS 构建里不可用。
port_up() {
    netstat -ano 2>/dev/null | grep -q "LISTENING" && \
    netstat -ano 2>/dev/null | grep "LISTENING" | grep -q ":$1 "
}

svc_name() {
    case "$1" in
        3308) echo "MySQL" ;;
        6380) echo "Redis" ;;
        8080) echo "GateServer" ;;
        8990) echo "ChatServer" ;;
        8991) echo "ChatServer2" ;;
        50051) echo "VarifyServer" ;;
        50052) echo "StatusServer" ;;
        *) echo "port-$1" ;;
    esac
}

# ---------- 起服务 ----------
# ★ exec 让 subshell 被目标进程替换，这样 $! 拿到的就是 exe 自己的 PID，
#   后面 taskkill 才杀得准；不 exec 的话 $! 是 subshell 的 PID。
launch() {
    local name="$1" dir="$2"; shift 2
    ( cd "$dir" 2>/dev/null || exit 1; exec "$@" ) >>"$LOGDIR/$name.log" 2>&1 &
    echo $! > "$LOGDIR/$name.pid"
    echo "    已拉起 $(svc_name_for_log "$name") (pid $(cat "$LOGDIR/$name.pid"))"
}

svc_name_for_log() {
    case "$1" in
        gate) echo "GateServer :8080" ;;
        chat) echo "ChatServer :8990" ;;
        chat2) echo "ChatServer2 :8991" ;;
        status) echo "StatusServer :50052" ;;
        varify) echo "VarifyServer :50051" ;;
        *) echo "$1" ;;
    esac
}

stop_started() {
    for f in "$LOGDIR"/*.pid; do
        [ -f "$f" ] || continue
        pid="$(cat "$f" 2>/dev/null)"
        [ -n "$pid" ] || continue
        taskkill //F //PID "$pid" >/dev/null 2>&1
        rm -f "$f"
    done
}
if [ "$AUTO_START" = 1 ] && [ "$KEEP" = 0 ]; then
    trap stop_started EXIT
fi

wait_port() {
    local p="$1" i=0
    while [ "$i" -lt 40 ]; do
        port_up "$p" && return 0
        sleep 1
        i=$((i + 1))
    done
    return 1
}

# ---------- 预检 ----------
# [1] 要 ChatServer；[2][3] 要 GateServer（或 GATE_URL 指的目标）。
GATE_TARGET="${GATE_URL:-http://127.0.0.1:8080}"
GATE_PORT="$(echo "$GATE_TARGET" | sed -n 's|.*:\([0-9]\{2,5\}\).*|\1|p')"
[ -n "$GATE_PORT" ] || GATE_PORT=8080

NEED=""
# ★ protocol_test 不只是打 ChatServer：它先用 GateServer 的 /user_login 换 token，
#   而 /user_login 内部还要 gRPC 调 StatusServer 拿聊天服地址。
#   缺 StatusServer 时不会报"连不上"，而是登录直接失败（1002/1009），
#   28 个用例一行都跑不到 —— 看着像 ChatServer 挂了，其实是依赖没起全。
[ "${SKIP_PROTOCOL:-0}" = 1 ] || NEED="$NEED 8990 8080 50052"
[ "${SKIP_GATE:-0}"     = 1 ] || NEED="$NEED $GATE_PORT"
[ "${SKIP_UPLOAD:-0}"   = 1 ] || NEED="$NEED $GATE_PORT"
# [4] 群聊：不同的号可能被 StatusServer 分到不同聊天服，
#     所以两台 ChatServer 都要在，顺带把跨服 gRPC 扇出也覆盖了。
[ "${SKIP_GROUP:-0}"    = 1 ] || NEED="$NEED 8990 8991 8080 50052"
NEED="$(echo "$NEED" | tr ' ' '\n' | grep -v '^$' | sort -u | tr '\n' ' ')"

echo
echo "============================================================"
echo "  Hcat 一键回归   根目录: $ROOT"
echo "============================================================"

MISSING=""
for p in $NEED; do
    if port_up "$p"; then
        echo "  [OK]   $(svc_name "$p") :$p"
    else
        echo "  [缺失] $(svc_name "$p") :$p"
        MISSING="$MISSING $p"
    fi
done
# MySQL / Redis 只是提示，gate_e2e 查不到库会退回 uid=2，不阻断
for p in 3308 6380; do
    port_up "$p" || echo "  [提示] $(svc_name "$p") :$p 没起，gate_e2e 会退回 uid=2"
done

if [ -n "$MISSING" ]; then
    if [ "$AUTO_START" != 1 ]; then
        echo
        echo "  ${YELLOW}缺服务，先起后端再跑：Start\\start.bat backend${RESET}"
        echo "  或者：bash test/run_all.sh --start"
        exit 2
    fi
    echo
    echo "  自动拉起缺失的服务 ..."
    for p in $MISSING; do
        case "$p" in
            8080)  launch gate   "$ROOT/GateServer"   "$ROOT/GateServer/x64/Debug/GateServer.exe" ;;
            8990)  launch chat   "$ROOT/ChatServer"   "$ROOT/ChatServer/x64/Debug/ChatServer.exe" ;;
            # ★ ChatServer2 的工程文件叫 ChatServer.vcxproj，产出的 exe 也叫 ChatServer.exe
            8991)  launch chat2  "$ROOT/ChatServer2"  "$ROOT/ChatServer2/x64/Debug/ChatServer.exe" ;;
            50052) launch status "$ROOT/StatusServer" "$ROOT/StatusServer/x64/Debug/StatusServer.exe" ;;
            50051) launch varify "$ROOT/VarifyServer" node server.js ;;
            *) echo "    :$p 我不会自己起，请手动拉" ;;
        esac
    done
    for p in $MISSING; do
        if wait_port "$p"; then
            echo "    :$p 就绪"
        else
            echo "    ${RED}:$p 等待超时，日志在 $LOGDIR/${RESET}"
        fi
    done
fi

# ---------- 跑套件 ----------
# 端口 listen 了 ≠ 就绪：GateServer 的 MySQL 连接池还在建，
# 刚起来就打登录容易拿到假的失败。给 2 秒缓冲。
sleep 2

SUITES=""
run_suite() {
    local idx="$1" tag="$2" title="$3"; shift 3
    local log="$LOGDIR/$tag.log"
    echo
    echo "------------------------------------------------------------"
    echo "  [$idx] $title"
    echo "------------------------------------------------------------"
    "$@" 2>&1 | tee "$log"
    local rc="${PIPESTATUS[0]}"
    local pass fail
    pass="$(grep -c '\[PASS\]' "$log" 2>/dev/null)"
    fail="$(grep -c '\[FAIL\]' "$log" 2>/dev/null)"
    [ -n "$pass" ] || pass=0
    [ -n "$fail" ] || fail=0
    SUITES="$SUITES$idx|$tag|$title|$pass|$fail|$rc
"
    return 0
}

IDX=0

if [ "${SKIP_PROTOCOL:-0}" != 1 ]; then
    IDX=$((IDX + 1))
    run_suite "$IDX" protocol "ChatServer 协议测试 (protocol_test.py)" \
        "$PY" "$ROOT/test/protocol_test.py"
fi

if [ "${SKIP_GATE:-0}" != 1 ]; then
    IDX=$((IDX + 1))
    run_suite "$IDX" gate_e2e "GateServer HTTP e2e (gate_e2e_test.py)" \
        "$PY" "$ROOT/test/gate_e2e_test.py"
fi

if [ "${SKIP_UPLOAD:-0}" != 1 ]; then
    IDX=$((IDX + 1))
    export GATE_URL="$GATE_TARGET"
    run_suite "$IDX" upload_big "大文件上传流式协议 (upload_big_test.py) -> $GATE_TARGET" \
        "$PY" "$ROOT/test/upload_big_test.py"
fi

if [ "${SKIP_GROUP:-0}" != 1 ]; then
    IDX=$((IDX + 1))
    run_suite "$IDX" group_chat "群聊端到端 (group_chat_test.py)" \
        "$PY" "$ROOT/test/group_chat_test.py"
fi

# 配置一致性（不需要起服务）：上传上限两边同值、错误码数值不冲突、端口不撞车
IDX=$((IDX + 1))
run_suite "$IDX" config "配置一致性 (config_consistency_check.py)" \
    "$PY" "$ROOT/test/config_consistency_check.py"

# 可选：mock 套件（不需要真实 GateServer，验证的是同一份服务端逻辑的移植版）
if [ "${RUN_MOCK:-0}" = 1 ] && [ -x "$ROOT/test/beast_proto/mock_gate.exe" ]; then
    IDX=$((IDX + 1))
    run_suite "$IDX" mock_smoke "mock_gate 服务端用例 (mock_smoke.py)" \
        "$PY" "$ROOT/test/beast_proto/mock_smoke.py"
    IDX=$((IDX + 1))
    run_suite "$IDX" slow_guard "防慢速闸门 (slow_guard_test.py)" \
        "$PY" "$ROOT/test/beast_proto/slow_guard_test.py"
fi

# ---------- 汇总 ----------
echo
echo "============================================================"
echo "  汇总"
echo "============================================================"
# ★ 把中文标题放最后一列：printf 的宽度按字节算，中文一行里有宽有窄，
#   一旦把中文夹在数字列中间，后面的数字就会参差不齐。
printf "  %-4s %-11s %6s %6s %7s   %s\n" "#" "套件" "通过" "失败" "rc" "说明"
echo "  ----------------------------------------------------------"
TOTAL_P=0; TOTAL_F=0; BAD=0
while IFS='|' read -r idx tag title pass fail rc; do
    [ -n "$idx" ] || continue
    TOTAL_P=$((TOTAL_P + pass))
    TOTAL_F=$((TOTAL_F + fail))
    [ "$rc" = 0 ] || BAD=$((BAD + 1))
    printf "  %-4s %-11s %6s %6s %7s   %s\n" \
        "[$idx]" "$tag" "$pass" "$fail" "rc=$rc" "$title"
done <<< "$SUITES"
echo "  ----------------------------------------------------------"
printf "  %-4s %-11s %6s %6s\n" "" "合计" "$TOTAL_P" "$TOTAL_F"

if [ "$BAD" -gt 0 ]; then
    echo
    echo "  ${RED}有 $BAD 个套件没通过，详见 $LOGDIR/*.log${RESET}"
    exit 1
fi
echo
echo "  ${GREEN}全部通过${RESET}  日志: $LOGDIR/"
exit 0
