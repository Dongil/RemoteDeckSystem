// RemoteDeck_Touch · Web UI (module-webui: Images + Config + Logs)
// Design Ref: §5.1, §5.4

const $ = id => document.getElementById(id);
const ROLES = ['title', 'photo', 'name'];
const fmtBytes = n => n < 1024 ? n + 'B' : n < 1048576 ? (n/1024).toFixed(1)+'KB' : (n/1048576).toFixed(2)+'MB';
const fmtMs = ms => { const s = Math.floor(ms/1000); return new Date(s*1000).toTimeString().slice(0,8); };

// ---------- Tabs ----------
function activateTab(name) {
  document.querySelectorAll('.tab').forEach(b => b.classList.toggle('active', b.dataset.tab === name));
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.toggle('active', p.id === 'tab-' + name));
  if (name === 'config' && !configLoaded) loadConfig();
  if (name === 'logs') refreshLog();
}

// ---------- Control Tab (Long polling 10s + ETag) ----------
let ctrlEtag = 0;
let ctrlPolling = false;
let ctrlAbort = null;

function renderCtrl(d) {
  const circle = $('ctrlCircle'), label = $('ctrlLabel');
  const btnIn = $('btnIn'), btnOut = $('btnOut');
  circle.className = 'state-circle' + (d.in ? ' in' : d.out ? ' out' : '');
  label.className = 'state-label' + (d.in ? ' in' : d.out ? ' out' : '');
  label.textContent = d.in ? '재실 (IN)' : d.out ? '부재 (OUT)' : '대기';
  btnIn.classList.toggle('active', d.in);
  btnOut.classList.toggle('active', d.out);
  $('ctrlEtag').textContent = d.etag;
}

async function pollControl() {
  if (ctrlPolling) return;
  ctrlPolling = true;
  $('ctrlPolling').textContent = 'polling…';
  while (ctrlPolling) {
    try {
      ctrlAbort = new AbortController();
      const r = await fetch(`/api/control?since=${ctrlEtag}`, { signal: ctrlAbort.signal });
      if (!r.ok) throw new Error('HTTP ' + r.status);
      const d = await r.json();
      ctrlEtag = d.etag;
      renderCtrl(d);
    } catch (e) {
      if (e.name === 'AbortError') break;
      $('ctrlPolling').textContent = 'error: ' + e.message;
      await new Promise(r => setTimeout(r, 2000));  // back-off
    }
  }
  $('ctrlPolling').textContent = 'idle';
}

function stopPolling() {
  ctrlPolling = false;
  if (ctrlAbort) ctrlAbort.abort();
}

async function toggleControl(state) {
  // state: 'in' or 'out'
  const body = state === 'in' ? { in: true } : { out: true };
  try {
    const r = await fetch('/api/control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    ctrlEtag = d.etag;
    renderCtrl(d);
    toast(`${state.toUpperCase()} 전송됨 (etag=${d.etag})`);
  } catch (e) {
    toast('전송 실패: ' + e.message, true);
  }
}

// ---------- Status ----------
async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const s = await r.json();
    $('statusBar').textContent =
      `heap=${fmtBytes(s.heap_free)} (min ${fmtBytes(s.heap_min)}) · ` +
      `spiffs=${fmtBytes(s.spiffs_used)}/${fmtBytes(s.spiffs_total)} · ` +
      `${s.network.iface} ${s.network.ip} · v${s.fw_version}`;
  } catch (e) {
    $('statusBar').textContent = 'status error: ' + e.message;
  }
}

// ---------- Toast ----------
function toast(msg, fail = false, ms = 3000) {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast' + (fail ? ' fail' : '');
  t.hidden = false;
  clearTimeout(toast._t);
  toast._t = setTimeout(() => { t.hidden = true; }, ms);
}

// ---------- Images Tab ----------
async function fetchImagesList() {
  try {
    const r = await fetch('/api/images/list');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return await r.json();
  } catch (e) {
    toast('이미지 목록 로드 실패: ' + e.message, true);
    return { images: [] };
  }
}

function matchImageForRole(images, role) {
  return images.find(i => i.name.toLowerCase() === role + '.png')
      || images.find(i => i.name.toLowerCase() === role + '.bmp');
}

async function renderImageCards() {
  const data = await fetchImagesList();
  const cards = ROLES.map(role => {
    const img = matchImageForRole(data.images, role);
    const cacheBust = '?t=' + Date.now();
    const thumb = img
      ? `<img src="/api/images/${img.name}${cacheBust}" alt="${role}">`
      : `<div class="placeholder">no image</div>`;
    const meta = img ? `${img.name} · ${fmtBytes(img.size)}` : '— 기본 이미지 사용 —';
    const del = img ? `<button class="danger" data-del="${img.name}">삭제</button>` : '';
    return `
      <div class="img-card">
        <div class="role">${role}</div>
        <div class="thumb">${thumb}</div>
        <div class="meta">${meta}</div>
        <button data-role="${role}">교체</button>
        ${del}
      </div>`;
  }).join('');
  $('imageCards').innerHTML = cards;
  $('imageCards').querySelectorAll('button[data-role]').forEach(b =>
    b.onclick = () => triggerFileSelect(b.dataset.role));
  $('imageCards').querySelectorAll('button[data-del]').forEach(b =>
    b.onclick = () => deleteImage(b.dataset.del));
}

function triggerFileSelect(role) {
  const inp = $('fileInput');
  inp.dataset.targetRole = role;
  inp.click();
}

async function uploadFile(file) {
  if (!file) return;
  const name = file.name.toLowerCase();
  if (!name.endsWith('.png') && !name.endsWith('.bmp')) {
    toast('PNG 또는 BMP 파일만 업로드 가능', true);
    return;
  }
  if (file.size > 200 * 1024) {
    toast(`파일이 너무 큼: ${fmtBytes(file.size)} (최대 200KB)`, true);
    return;
  }
  const role = $('fileInput').dataset.targetRole;
  const ext = name.endsWith('.png') ? '.png' : '.bmp';
  const finalName = role ? (role + ext) : file.name;
  const renamed = new File([file], finalName, { type: file.type });
  const fd = new FormData();
  fd.append('file', renamed);

  const bar = $('progressBar'), fill = $('progressFill');
  bar.hidden = false; fill.style.width = '0%'; fill.textContent = '0%';

  try {
    await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/images/upload');
      xhr.upload.onprogress = e => {
        if (e.lengthComputable) {
          const p = Math.round(e.loaded * 100 / e.total);
          fill.style.width = p + '%'; fill.textContent = p + '%';
        }
      };
      xhr.onload = () => xhr.status >= 200 && xhr.status < 300 ? resolve() : reject(new Error('HTTP ' + xhr.status));
      xhr.onerror = () => reject(new Error('network'));
      xhr.send(fd);
    });
    toast(`${finalName} 업로드 완료 — 단말 갱신 중`);
    setTimeout(renderImageCards, 1500);
  } catch (e) {
    toast(`업로드 실패: ${e.message}`, true);
  } finally {
    setTimeout(() => { bar.hidden = true; }, 1500);
    $('fileInput').dataset.targetRole = '';
    $('fileInput').value = '';
  }
}

async function deleteImage(name) {
  if (!confirm(`${name} 을(를) 삭제하시겠습니까?`)) return;
  try {
    const r = await fetch('/api/images/' + name, { method: 'DELETE' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    toast(`${name} 삭제됨`);
    setTimeout(renderImageCards, 800);
  } catch (e) {
    toast(`삭제 실패: ${e.message}`, true);
  }
}

function bindDragDrop() {
  const z = $('dropZone');
  ['dragenter', 'dragover'].forEach(ev => z.addEventListener(ev, e => { e.preventDefault(); z.classList.add('dragover'); }));
  ['dragleave', 'drop'].forEach(ev => z.addEventListener(ev, e => { e.preventDefault(); z.classList.remove('dragover'); }));
  z.addEventListener('drop', e => { const f = e.dataTransfer.files[0]; if (f) uploadFile(f); });
  $('fileInput').addEventListener('change', e => { if (e.target.files[0]) uploadFile(e.target.files[0]); });
}

// ---------- Config Tab ----------
let configLoaded = false;

async function loadConfig() {
  try {
    const r = await fetch('/api/config');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const txt = await r.text();
    let pretty = txt;
    try { pretty = JSON.stringify(JSON.parse(txt), null, 2); } catch (_) { /* keep raw */ }
    $('configEditor').value = pretty;
    configLoaded = true;
  } catch (e) {
    $('configEditor').value = '// load failed: ' + e.message;
  }
  // imagesconfig.json (read-only)
  try {
    const r = await fetch('/api/imagesconfig');
    const txt = await r.text();
    let pretty = txt; try { pretty = JSON.stringify(JSON.parse(txt), null, 2); } catch (_) {}
    $('imagesConfigView').textContent = pretty;
  } catch (e) {
    $('imagesConfigView').textContent = 'load failed';
  }
}

async function saveConfig() {
  const body = $('configEditor').value;
  // 클라이언트 사이드 JSON 검증
  try { JSON.parse(body); } catch (e) {
    toast('JSON 형식 오류: ' + e.message, true); return;
  }
  try {
    const r = await fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body
    });
    if (!r.ok) {
      const e = await r.text();
      throw new Error(e || 'HTTP ' + r.status);
    }
    toast('설정 저장됨 (재부팅 후 반영)');
  } catch (e) {
    toast('저장 실패: ' + e.message, true);
  }
}

async function uploadOta() {
  const f = $('otaFile').files[0];
  if (!f) { toast('OTA bin 파일을 선택하세요', true); return; }
  if (!f.name.toLowerCase().endsWith('.bin')) {
    toast('.bin 파일만 업로드 가능', true); return;
  }
  if (f.size > 1900 * 1024) {
    toast(`파일 크기 초과: ${(f.size/1024).toFixed(0)}KB (최대 ~1.85MB)`, true); return;
  }
  if (!confirm(`${f.name} (${(f.size/1024).toFixed(0)}KB) 업로드 후 자동 재부팅됩니다. 진행하시겠습니까?`)) return;

  const fd = new FormData();
  fd.append('file', f);
  const bar = $('otaProgress'), fill = $('otaFill'), msg = $('otaMsg');
  bar.hidden = false; fill.style.width = '0%'; fill.textContent = '0%';
  msg.textContent = '업로드 중...';

  try {
    await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/ota');
      xhr.timeout = 120000;  // OTA 는 시간 걸림
      xhr.upload.onprogress = e => {
        if (e.lengthComputable) {
          const p = Math.round(e.loaded * 100 / e.total);
          fill.style.width = p + '%'; fill.textContent = p + '%';
        }
      };
      xhr.onload = () => xhr.status >= 200 && xhr.status < 300 ? resolve() : reject(new Error('HTTP ' + xhr.status));
      xhr.onerror = () => reject(new Error('network'));
      xhr.ontimeout = () => reject(new Error('timeout'));
      xhr.send(fd);
    });
    msg.textContent = '✅ 업로드 완료 — 단말 재부팅 중 (~20초 대기)';
    toast('OTA 성공 — 재부팅 중');
  } catch (e) {
    // 단말이 OTA 성공 후 응답 send 도중 reboot → connection drop = network/timeout.
    // 단순 fail 보다는 reboot 가능성 안내 (사용자가 새로고침으로 확인).
    msg.textContent = '⚠ 응답 끊김 (' + e.message + ') — 단말 reboot 중일 수 있음. 30초 대기 후 새로고침';
    toast('연결 끊김 — 단말 reboot 가능성, 30초 대기 후 확인', false, 5000);
    setTimeout(() => { window.location.reload(); }, 30000);
  }
}

async function rebootDevice() {
  if (!confirm('지금 재부팅하시겠습니까? 약 15-30초 후 다시 접속 가능합니다.')) return;
  try {
    const r = await fetch('/api/reboot', { method: 'POST' });
    if (r.ok) toast('재부팅 중...');
  } catch (e) {
    toast('재부팅 요청 실패: ' + e.message, true);
  }
}

// ---------- Logs Tab ----------
let logAutoInterval = null;

async function refreshLog() {
  try {
    const r = await fetch('/api/log');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    const html = (d.entries || []).map(e => `
      <div class="log-entry ok">
        <span class="time">${fmtMs(e.ts)}</span>
        <span class="event">${e.event}</span>
        ${e.detail}
      </div>`).join('') || '<div class="hint">no entries</div>';
    $('serverLog').innerHTML = html;
  } catch (e) {
    $('serverLog').textContent = 'log fetch failed: ' + e.message;
  }
}

function toggleLogAuto() {
  if ($('logAuto').checked) {
    refreshLog();
    logAutoInterval = setInterval(refreshLog, 10000);  // 10s
  } else {
    if (logAutoInterval) { clearInterval(logAutoInterval); logAutoInterval = null; }
  }
}

// ---------- Init ----------
async function init() {
  // tabs
  document.querySelectorAll('.tab').forEach(b =>
    b.addEventListener('click', () => activateTab(b.dataset.tab)));
  // images
  bindDragDrop();
  await renderImageCards();
  // config buttons
  $('cfgReload').addEventListener('click', loadConfig);
  $('cfgSave').addEventListener('click', saveConfig);
  $('cfgReboot').addEventListener('click', rebootDevice);
  $('otaUpload').addEventListener('click', uploadOta);
  // log buttons
  $('logRefresh').addEventListener('click', refreshLog);
  $('logAuto').addEventListener('change', toggleLogAuto);
  // control buttons + long polling 시작
  $('btnIn').addEventListener('click', () => toggleControl('in'));
  $('btnOut').addEventListener('click', () => toggleControl('out'));
  pollControl();  // 페이지 로딩 시 즉시 시작 (Control 탭 기본 활성)
  // status — 5초 폴링 (이미지 탭 외에도 항상)
  await fetchStatus();
  setInterval(fetchStatus, 5000);
}

init();
