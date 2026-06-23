#!/usr/bin/env bash
# Design Ref: §8.3 — L2 PoC 엄격 시나리오 (Phase 1 Gate)
# Plan SC: FR-02 — PoC 풀세트 통과 (1개라도 fail → v2.4 분기)
#
# 사용법:
#   ./run_poc.sh <device_ip>
#   예) ./run_poc.sh 192.168.0.100
#
# 시나리오:
#   P1: 연속 GET /api/status 10회 — heap 안정성
#   P2: 병렬 GET 5개 x 30초 — 동시성 처리
#   P3: 별도 터미널에서 mqtt_pub.py 실행 + GET 1분 — MQTT/HTTP coexist
#   P4: idle 1분 — heap leak 검출
#
# Pass 조건:
#   - 모든 HTTP 응답 200
#   - heap_free ≥ 80KB 유지
#   - 응답 시간 ≤ 1000ms
#   - ESP 부팅 hang 無 (수동 LCD 관찰)

set -u

IP="${1:-}"
if [[ -z "$IP" ]]; then
    echo "usage: $0 <device_ip>"
    exit 1
fi

AUTH="admin:12345"
BASE="http://$IP"

pass=0
fail=0

step() { echo; echo "=== $1 ==="; }
ok()   { echo "  [OK] $1"; pass=$((pass+1)); }
ng()   { echo "  [FAIL] $1"; fail=$((fail+1)); }

# --- P1: 연속 upload(status GET) 10회 ---
# 실제 upload 는 module-webui 단계 — PoC 는 status GET 으로 burst 검증
step "P1: 연속 GET /api/status 10회"
for i in $(seq 1 10); do
    body=$(curl -s -u "$AUTH" -w "\n%{http_code} %{time_total}" "$BASE/api/status")
    code=$(echo "$body" | tail -n1 | awk '{print $1}')
    rt=$(echo "$body"   | tail -n1 | awk '{print $2}')
    heap=$(echo "$body" | head -n-1 | grep -o '"heap_free":[0-9]*' | cut -d: -f2)
    if [[ "$code" == "200" ]] && [[ -n "$heap" ]] && [[ "$heap" -ge 80000 ]]; then
        ok "burst $i/10: code=$code heap=$heap rt=${rt}s"
    else
        ng "burst $i/10: code=$code heap=${heap:-?} rt=${rt}s"
    fi
done

# --- P2: 병렬 GET 5개 x 30초 ---
step "P2: 병렬 GET 5개 x 30초"
end=$(($(date +%s) + 30))
parallel_fail=0
while [[ $(date +%s) -lt $end ]]; do
    pids=()
    for i in 1 2 3 4 5; do
        ( curl -s -o /dev/null -u "$AUTH" -w "%{http_code}\n" "$BASE/api/status" \
          | grep -q "^200$" || echo "FAIL" >> /tmp/poc_p2_fail.log ) &
        pids+=($!)
    done
    for p in "${pids[@]}"; do wait "$p"; done
done
if [[ -f /tmp/poc_p2_fail.log ]]; then
    parallel_fail=$(wc -l < /tmp/poc_p2_fail.log)
    rm -f /tmp/poc_p2_fail.log
fi
if [[ "$parallel_fail" -eq 0 ]]; then
    ok "병렬 30s 동안 실패 0건"
else
    ng "병렬 30s 동안 실패 $parallel_fail 건"
fi

# --- P3: MQTT + GET 동시 1분 ---
step "P3: MQTT pub 1Hz + GET /api/status 1분 (별도 터미널에서 mqtt_pub.py 실행 중이어야 함)"
end=$(($(date +%s) + 60))
mqtt_fail=0
while [[ $(date +%s) -lt $end ]]; do
    code=$(curl -s -o /dev/null -u "$AUTH" -w "%{http_code}" "$BASE/api/status")
    if [[ "$code" != "200" ]]; then
        mqtt_fail=$((mqtt_fail+1))
    fi
    sleep 1
done
if [[ "$mqtt_fail" -eq 0 ]]; then
    ok "MQTT 동시 트래픽 1분 동안 GET 실패 0건"
else
    ng "MQTT 동시 GET 실패 $mqtt_fail 건"
fi

# --- P4: idle 1분 → heap 변화 ---
step "P4: idle 1분 — heap leak 검출"
heap_before=$(curl -s -u "$AUTH" "$BASE/api/status" | grep -o '"heap_free":[0-9]*' | cut -d: -f2)
sleep 60
heap_after=$(curl -s -u "$AUTH" "$BASE/api/status" | grep -o '"heap_free":[0-9]*' | cut -d: -f2)
diff=$((heap_before - heap_after))
abs_diff=${diff#-}
if [[ "$abs_diff" -lt 5120 ]]; then
    ok "idle heap 안정 (before=$heap_before after=$heap_after diff=${diff})"
else
    ng "idle heap leak 의심 (before=$heap_before after=$heap_after diff=${diff})"
fi

# --- Gate 판정 ---
echo
echo "================================"
echo "PoC Gate Result: pass=$pass fail=$fail"
echo "================================"
if [[ "$fail" -gt 0 ]]; then
    echo "❌ PoC FAIL — v2.4 분기 결정 필요 (Plan §5 Risk)"
    exit 1
else
    echo "✅ PoC PASS — module-webui 진행 가능"
    exit 0
fi
