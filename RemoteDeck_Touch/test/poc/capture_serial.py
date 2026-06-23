#!/usr/bin/env python3
# COM3 serial capture — pio device monitor 가 redirect 시 비어 있는 문제 회피
# 사용: python capture_serial.py <duration_sec> <out_file>
import serial, sys, time

dur = int(sys.argv[1]) if len(sys.argv) > 1 else 30
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/serial.log"

try:
    ser = serial.Serial("COM3", 115200, timeout=0.1)
except Exception as e:
    print(f"OPEN FAIL: {e}", file=sys.stderr)
    sys.exit(1)

end = time.time() + dur
with open(out, "wb") as f:
    while time.time() < end:
        data = ser.read(4096)
        if data:
            f.write(data)
            f.flush()
ser.close()
print(f"captured {dur}s to {out}")
