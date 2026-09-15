#!/usr/bin/env python3
# /api/control Long polling + POST wake 검증
import urllib.request, base64, time, json, threading, sys

BASE = 'http://192.168.10.122/api/control'
AUTH = 'Basic ' + base64.b64encode(b'admin:12345').decode()

def get(since=0, timeout=15):
    req = urllib.request.Request(f'{BASE}?since={since}')
    req.add_header('Authorization', AUTH)
    t0 = time.time()
    body = urllib.request.urlopen(req, timeout=timeout).read().decode()
    return time.time() - t0, json.loads(body)

def post(payload, timeout=5):
    req = urllib.request.Request(BASE, method='POST',
        data=payload.encode(),
        headers={'Content-Type': 'application/json', 'Authorization': AUTH})
    body = urllib.request.urlopen(req, timeout=timeout).read().decode()
    return json.loads(body)

# A) Long polling timeout (no change → ~10s)
print('=== A. Long polling timeout (no change) ===')
_, d0 = get(0)
print(f'  initial: etag={d0["etag"]} in={d0["in"]} out={d0["out"]}')
elapsed, dA = get(d0['etag'])
print(f'  resp: etag={dA["etag"]} in={dA["in"]} out={dA["out"]}  elapsed={elapsed:.2f}s (expect ~10s)')

# B) Long polling + POST wake (~2s)
print()
print('=== B. POST wake (expect ~2s) ===')
_, dB0 = get(0)
print(f'  initial: etag={dB0["etag"]} in={dB0["in"]} out={dB0["out"]}')
opposite = '{"in":true}' if dB0['out'] else '{"out":true}'

def trigger():
    time.sleep(2)
    r = post(opposite)
    print(f'  [t+2s] POST {opposite} → etag={r["etag"]}')

threading.Thread(target=trigger, daemon=True).start()
elapsed, dB = get(dB0['etag'])
print(f'  poll wake: etag={dB["etag"]} in={dB["in"]} out={dB["out"]}  elapsed={elapsed:.2f}s')

# C) Toggle sequence
print()
print('=== C. Toggle sequence ===')
for body in ['{"in":true}', '{"out":true}', '{"in":true}', '{"out":true}']:
    r = post(body)
    print(f'  POST {body:24}  → etag={r["etag"]} in={r["in"]} out={r["out"]}')
