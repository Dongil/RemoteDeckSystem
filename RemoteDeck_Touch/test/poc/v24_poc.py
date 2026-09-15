#!/usr/bin/env python3
# v2.4 module-spi-poc Gate 검증 — brower 6 동시 + 22KB inline + MQTT 동시
# Design Ref: §8.3
# Plan SC: FR-12 (PoC 엄격)
#
# 사용법:
#   python v24_poc.py [device_ip]   default 192.168.10.122
# Pass 조건:
#   P1: brower 6 동시 GET / (22KB inline HTML) → 6/6 200, body 사이즈 정확
#   P2: 6 동시 × 5 burst = 30 req → fail 0
#   P3: 30초 sustained 혼합 → fail 0
#   P4: MQTT 외부 의존이라 본 스크립트는 web 부하만 (사용자 별도 mqtt_pub.py 실행)
#   P5: 수동 (사용자가 부하 중 LCD touch 후 < 200ms resume 확인)
#
# 1개라도 fail → v2.5 분기 결정.

import sys, time, base64, gzip
import urllib.request
import concurrent.futures as cf

BASE = f'http://{sys.argv[1] if len(sys.argv) >= 2 else "192.168.10.122"}'
AUTH = 'Basic ' + base64.b64encode(b'admin:12345').decode()

def get(path, timeout=15):
    req = urllib.request.Request(BASE + path)
    req.add_header('Authorization', AUTH)
    req.add_header('Accept-Encoding', 'gzip')
    t0 = time.time()
    with urllib.request.urlopen(req, timeout=timeout) as r:
        raw = r.read()
        encoding = r.headers.get('Content-Encoding', '')
        elapsed = time.time() - t0
        return r.status, raw, encoding, elapsed

def check_html(raw, encoding):
    """gzip 자동 decompress + sanity"""
    body = gzip.decompress(raw) if encoding == 'gzip' else raw
    return body

pass_count = 0
fail_count = 0

def ok(msg): global pass_count; pass_count += 1; print(f'  [OK] {msg}')
def ng(msg): global fail_count; fail_count += 1; print(f'  [FAIL] {msg}')

# --- P1: 6 동시 GET / ---
print('\n=== P1: brower 6 동시 GET / (22KB inline HTML) ===')
def fetch_root(i):
    try:
        status, raw, enc, t = get('/')
        body = check_html(raw, enc)
        return (i, status, len(body), t, None)
    except Exception as e:
        return (i, 0, 0, 0, str(e))

with cf.ThreadPoolExecutor(max_workers=6) as ex:
    results = list(ex.map(fetch_root, range(1, 7)))
for i, status, sz, t, err in results:
    if err:
        ng(f'req{i}: {err}')
    elif status == 200 and sz > 10000:
        ok(f'req{i}: 200 size={sz} rt={t:.2f}s')
    else:
        ng(f'req{i}: status={status} size={sz}')

# --- P2: 6 동시 × 5 burst ---
print('\n=== P2: 6 동시 × 5 burst (30 req) ===')
total = 0
burst_fail = 0
for b in range(5):
    with cf.ThreadPoolExecutor(max_workers=6) as ex:
        results = list(ex.map(fetch_root, range(1, 7)))
    for i, status, sz, t, err in results:
        total += 1
        if err or status != 200:
            burst_fail += 1
if burst_fail == 0:
    ok(f'30 req: all 200')
else:
    ng(f'30 req: fail={burst_fail}')

# --- P3: 30초 sustained ---
print('\n=== P3: 30초 sustained (status + control polling) ===')
def fetch(path):
    try:
        s, _, _, t = get(path, timeout=5)
        return (s == 200, t)
    except:
        return (False, 0)

t_end = time.time() + 30
ok_cnt = 0; fail_cnt = 0
while time.time() < t_end:
    with cf.ThreadPoolExecutor(max_workers=3) as ex:
        rs = list(ex.map(fetch, ['/api/status', '/api/control?since=0', '/api/log']))
    for s, t in rs:
        if s: ok_cnt += 1
        else: fail_cnt += 1
    time.sleep(0.5)
print(f'  30s: ok={ok_cnt} fail={fail_cnt}')
if fail_cnt == 0:
    ok('sustained 0 fail')
else:
    ng(f'sustained fail={fail_cnt}')

# --- P4: heap stability ---
print('\n=== P4: heap 안정성 ===')
try:
    import json
    _, raw, _, _ = get('/api/status')
    d = json.loads(raw.decode())
    print(f'  uptime={d["uptime_sec"]}s heap={d["heap_free"]:,} heap_min={d["heap_min"]:,}')
    if d['heap_free'] >= 30000:
        ok(f'heap baseline ≥ 30KB')
    else:
        ng(f'heap baseline < 30KB ({d["heap_free"]})')
except Exception as e:
    ng(f'status fetch: {e}')

# --- Gate ---
print()
print('=' * 50)
print(f'PoC v2.4 Gate: pass={pass_count} fail={fail_count}')
print('=' * 50)
if fail_count > 0:
    print('❌ PoC FAIL — v2.5 분기 결정 필요')
    sys.exit(1)
else:
    print('✅ PoC PASS — module-png-restore 진행 가능')
    print('   P5 (LCD touch tap-to-acquire) 는 사용자 수동 검증')
    sys.exit(0)
