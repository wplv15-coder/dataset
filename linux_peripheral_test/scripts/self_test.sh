#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PORT="${PORT:-18888}"
SERVER="$ROOT/build/server"
CLIENT="$ROOT/build/client"
LOG="$(mktemp)"

if [[ ! -x "$SERVER" || ! -x "$CLIENT" ]]; then
    echo "请先执行 make" >&2
    exit 1
fi

"$SERVER" "$PORT" >"$LOG" 2>&1 &
SPID=$!
cleanup() {
    kill "$SPID" 2>/dev/null || true
    wait "$SPID" 2>/dev/null || true
    rm -f "$LOG"
}
trap cleanup EXIT

for _ in $(seq 1 20); do
    if grep -q "已启动" "$LOG" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

"$CLIENT" 127.0.0.1 "$PORT" --all --auto 2

echo "----- Server 日志 -----"
cat "$LOG"
echo "-----------------------"

grep -q "分配槽位 0" "$LOG"
grep -q "分配槽位 1" "$LOG"
grep -q "分配槽位 2" "$LOG"
grep -q "注册成功：Client=GPIO" "$LOG"
grep -q "注册成功：Client=I2C" "$LOG"
grep -q "注册成功：Client=SPI" "$LOG"
grep -q "\[线程 0\]" "$LOG"
grep -q "\[线程 1\]" "$LOG"
grep -q "\[线程 2\]" "$LOG"

echo "self_test OK: 3 个 Client 已分别由 3 个线程处理"
