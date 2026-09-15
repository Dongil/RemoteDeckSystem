#!/usr/bin/env python3
# Design Ref: §8.3 P3 — MQTT pub 1Hz (run_poc.sh 와 동시 실행)
# Plan SC: FR-02 — MQTT/HTTP coexist 검증
#
# 사용법:
#   pip install paho-mqtt
#   python3 mqtt_pub.py <broker_ip> [topic]
#
# run_poc.sh P3 단계 시작 1초 전에 실행하고, P3 종료 후 Ctrl+C.

import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("pip install paho-mqtt 필요")
    sys.exit(1)

broker = sys.argv[1] if len(sys.argv) >= 2 else "127.0.0.1"
topic  = sys.argv[2] if len(sys.argv) >= 3 else "remotedeck/poc/test"

client = mqtt.Client()
client.connect(broker, 1883, 60)
client.loop_start()

i = 0
print(f"publishing 1Hz to {broker} / {topic} (Ctrl+C to stop)")
try:
    while True:
        payload = '{"status":"IN","data":"poc","tick":%d}' % int(time.time())
        result = client.publish(topic, payload)
        i += 1
        if i % 10 == 0:
            print(f"  sent {i} messages")
        time.sleep(1)
except KeyboardInterrupt:
    print(f"\nstopped after {i} messages")
finally:
    client.loop_stop()
    client.disconnect()
