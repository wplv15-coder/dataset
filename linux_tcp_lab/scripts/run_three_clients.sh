#!/usr/bin/env bash
# 本机验证步骤 1、2：一个 Server 线程 accept，三个 Client 各占一条工作线程。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVER="$ROOT/bin/server"
CLIENT="$ROOT/bin/client"
PORT="${PORT:-18888}"
LOG="${TMPDIR:-/tmp}/linux_tcp_lab_server.log"

if [[ ! -x "$SERVER" || ! -x "$CLIENT" ]]; then
  echo "build first: make -C $ROOT"
  exit 1
fi

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

: > "$LOG"
"$SERVER" "$PORT" >"$LOG" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 50); do
  if grep -q "listening on" "$LOG" 2>/dev/null; then
    break
  fi
  sleep 0.05
done

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "server failed to start"
  cat "$LOG"
  exit 1
fi

echo "=== client GPIO ==="
"$CLIENT" 127.0.0.1 "$PORT" GPIO --demo
echo "=== client I2C ==="
"$CLIENT" 127.0.0.1 "$PORT" I2C --demo
echo "=== client SPI ==="
"$CLIENT" 127.0.0.1 "$PORT" SPI --demo

# 上面是串行三个 Client。下面再并发连一次，确认 Server 同时建 3 条线程。
echo "=== concurrent 3 clients ==="
"$CLIENT" 127.0.0.1 "$PORT" GPIO --demo > /tmp/linux_tcp_lab_gpio.log 2>&1 &
P1=$!
"$CLIENT" 127.0.0.1 "$PORT" I2C --demo > /tmp/linux_tcp_lab_i2c.log 2>&1 &
P2=$!
"$CLIENT" 127.0.0.1 "$PORT" SPI --demo > /tmp/linux_tcp_lab_spi.log 2>&1 &
P3=$!
wait "$P1" "$P2" "$P3"

echo
echo "=== server log ==="
cat "$LOG"

thread_count="$(grep -c 'registered as' "$LOG" || true)"
if [[ "$thread_count" -lt 6 ]]; then
  echo "expected at least 6 HELLO registrations (3 serial + 3 concurrent), got $thread_count"
  exit 1
fi

echo
echo "PASS: server accepted multiple clients and created one thread per connection"
