import sys
import subprocess
import os

print("Installing dependencies...")
subprocess.check_call([sys.executable, "-m", "pip", "install", "playwright", "markdown"])
subprocess.check_call([sys.executable, "-m", "playwright", "install", "chromium"])

import markdown
from playwright.sync_api import sync_playwright

CWD = os.path.abspath(os.getcwd())

def get_base64_image(filepath):
    import base64
    with open(filepath, "rb") as img_file:
        encoded = base64.b64encode(img_file.read()).decode('utf-8')
    ext = os.path.splitext(filepath)[1][1:].lower()
    if ext == 'jpg':
        ext = 'jpeg'
    return f"data:image/{ext};base64,{encoded}"

with open("docs/RemoteDeck_PC_Manual.md", "r", encoding="utf-8") as f:
    manual = f.read()

with open("docs/wiring-guide.md", "r", encoding="utf-8") as f:
    wiring = f.read()

# Replace local paths with absolute paths for the browser
manual = manual.replace("docs/wiring-guide.md", "")
manual = manual.replace("docs/RemoteDeck_PC_Manual.md", "")

# NanoBanana Images
img1_path = r"C:\Users\Administrator\.gemini\antigravity\brain\b9d89b63-834c-45a4-acb0-e7b77bcffb71\nanobanana_network_1776129875897.png"
img2_path = r"C:\Users\Administrator\.gemini\antigravity\brain\b9d89b63-834c-45a4-acb0-e7b77bcffb71\nanobanana_hardware_1776129888562.png"
asset_relay_path = os.path.join(CWD, "asset", "relay.png")
asset_fpanel_path = os.path.join(CWD, "asset", "F_Panel.png")
asset_circuit_path = os.path.join(CWD, "asset", "회로도.png")

img1_b64 = get_base64_image(img1_path)
img2_b64 = get_base64_image(img2_path)
relay_b64 = get_base64_image(asset_relay_path)
fpanel_b64 = get_base64_image(asset_fpanel_path)
circuit_b64 = get_base64_image(asset_circuit_path)

combined_md = f"""
# RemoteDeck PC 사용자 매뉴얼 통합본 (v2.2.0)

<div style="text-align:center;">
<img src="{img1_b64}" class="nanobanana" alt="NanoBanana Network">
<br><em>나노바나나 AI 시스템 모니터링</em>
</div>

이 문서는 RemoteDeck System의 종합 사용자 매뉴얼 및 하드웨어 배선 가이드를 제공합니다. 삽화는 전문적인 로봇 어시스턴트 "나노바나나"와 함께 안내됩니다.

---

{manual}

---

# 🚀 부록: 외부 기기 및 하드웨어 상세 배선 가이드

<div style="text-align:center;">
<img src="{img2_b64}" class="nanobanana" alt="NanoBanana Hardware">
<br><em>나노바나나 엔지니어의 배선 검수</em>
</div>

하드웨어 추가 확장 및 배선에 대한 심층 문서입니다. 아래 기존 설계도를 바탕으로 작업해주세요.

### 참고 자료: Relay 및 F_Panel 위치

![Relay Image]({relay_b64})

![F_Panel Image]({fpanel_b64})

### 전체 회로도

![회로도]({circuit_b64})

---

{wiring}
"""

html_body = markdown.markdown(combined_md, extensions=['tables', 'fenced_code'])

html = f"""
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  body {{ 
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
    line-height: 1.6; 
    color: #333;
    padding: 2em; 
  }}
  h1 {{ color: #2c3e50; border-bottom: 2px solid #eaecef; padding-bottom: 0.3em; margin-top: 1.5em; }}
  h2 {{ color: #e74c3c; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; margin-top: 1.5em; }}
  h3 {{ color: #2980b9; margin-top: 1.5em; }}
  table {{ border-collapse: collapse; width: 100%; margin: 1em 0; }}
  th, td {{ border: 1px solid #dfe2e5; padding: 6px 13px; text-align: left; }}
  th {{ background-color: #f6f8fa; font-weight: bold; }}
  tr:nth-child(even) {{ background-color: #f8f9fa; }}
  pre {{ background: #f6f8fa; padding: 16px; border-radius: 6px; overflow-x: auto; }}
  code {{ background: #f6f8fa; padding: 0.2em 0.4em; border-radius: 3px; font-family: monospace; font-size: 85%; }}
  img {{ max-width: 100%; display: block; margin: 1rem auto; box-shadow: 0 4px 10px rgba(0,0,0,0.1); border-radius: 4px; }}
  .nanobanana {{ max-width: 50%; border-radius: 12px; box-shadow: 0 6px 15px rgba(0,0,0,0.15); }}
  blockquote {{ border-left: 4px solid #dfe2e5; color: #6a737d; margin: 0; padding-left: 1em; background: #fafbfc; padding: 1em; }}
</style>
</head>
<body>
{html_body}
</body>
</html>
"""

print("Rendering PDF...")
with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page()
    page.set_content(html)
    
    # Wait for images to load
    page.evaluate("Promise.all(Array.from(document.images).map(img => { if (img.complete) return Promise.resolve(img.naturalHeight !== 0); return new Promise(resolve => { img.addEventListener('load', () => resolve(true)); img.addEventListener('error', () => resolve(false)); }); }))")

    output_path = os.path.join(CWD, "docs", "사용자메뉴얼_수정본.pdf")
    page.pdf(path=output_path, format="A4", print_background=True, margin={"top": "10mm", "bottom": "10mm", "left": "10mm", "right": "10mm"})
    browser.close()

print(f"PDF successfully generated at: {output_path}")
