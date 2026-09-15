// RemoteDeck_Touch · Web UI (v2.7 리뉴얼 — PC 스타일 5탭)
// S1 module-shell: 상태 / 제어 / 로그 + 설정·관리 골격. (설정폼 S2 / 관리 S3·S4 / 이미지 S5)

const $ = id => document.getElementById(id);
const fmtBytes = n => n < 1024 ? n + 'B' : n < 1048576 ? (n/1024).toFixed(1)+'KB' : (n/1048576).toFixed(2)+'MB';
const fmtMs = ms => { const s = Math.floor(ms/1000); return new Date(s*1000).toTimeString().slice(0,8); };
const fmtUptime = sec => {
  const d = Math.floor(sec/86400), h = Math.floor(sec%86400/3600), m = Math.floor(sec%3600/60);
  return (d ? d+'d ' : '') + String(h).padStart(2,'0') + ':' + String(m).padStart(2,'0');
};

// ---------- Tabs ----------
function activateTab(name) {
  document.querySelectorAll('.tab').forEach(b => b.classList.toggle('active', b.dataset.tab === name));
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.toggle('active', p.id === 'tab-' + name));
  if (name === 'status') renderStatusTab();
  if (name === 'logs') refreshLog();
}

function activateSubTab(name) {
  document.querySelectorAll('.sub-tab').forEach(b => b.classList.toggle('active', b.dataset.sub === name));
  document.querySelectorAll('.sub-panel').forEach(p => p.classList.toggle('active', p.id === 'sub-' + name));
  // S2/S5에서 서브탭별 로더 연결 예정
}

// ---------- Status (header + 상태 탭) ----------
let lastStatus = null;

async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const s = await r.json();
    lastStatus = s;
    $('statusBar').textContent =
      `${s.network.iface} ${s.network.ip} · heap ${fmtBytes(s.heap_free)} · v${s.fw_version}`;
    if ($('tab-status').classList.contains('active')) renderStatusTab();
  } catch (e) {
    $('statusBar').textContent = 'status error: ' + e.message;
  }
}

function infoRow(k, v, cls) {
  return `<div class="info-row"><span class="k">${k}</span><span class="v${cls ? ' ' + cls : ''}">${v}</span></div>`;
}

function renderStatusTab() {
  const s = lastStatus;
  if (!s) { $('sysInfo').innerHTML = '<div class="hint">loading...</div>'; return; }
  const mqtt = !!s.mqtt_connected;
  const net = s.network || {};
  $('sysInfo').innerHTML = [
    infoRow('장치 ID', s.device_id || '-'),
    infoRow('IP 주소', `${net.ip || '-'} (${net.iface || '-'})`),
    infoRow('가동 시간', fmtUptime(s.uptime_sec || 0)),
    infoRow('MQTT', mqtt ? '연결됨' : '연결 안됨', mqtt ? 'on' : 'off'),
    infoRow('시각', s.time || '-'),
    infoRow('펌웨어', `v${s.fw_version} (${s.fw_date || '-'})`),
  ].join('');
  $('resInfo').innerHTML = [
    infoRow('Heap free', fmtBytes(s.heap_free)),
    infoRow('Heap min', fmtBytes(s.heap_min)),
    infoRow('SPIFFS', `${fmtBytes(s.spiffs_used)} / ${fmtBytes(s.spiffs_total)}`),
  ].join('');
}

// ---------- Control Tab (Long polling + ETag) ----------
let ctrlEtag = 0, ctrlPolling = false, ctrlAbort = null;

function renderCtrl(d) {
  const circle = $('ctrlCircle'), label = $('ctrlLabel');
  circle.className = 'state-circle' + (d.in ? ' in' : d.out ? ' out' : '');
  label.className = 'state-label' + (d.in ? ' in' : d.out ? ' out' : '');
  label.textContent = d.in ? '재실 (IN)' : d.out ? '부재 (OUT)' : '대기';
  $('btnIn').classList.toggle('active', d.in);
  $('btnOut').classList.toggle('active', d.out);
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
    }
    await new Promise(r => setTimeout(r, 3000));  // 3s throttle (sockets=4 한계)
  }
  $('ctrlPolling').textContent = 'idle';
}

async function toggleControl(state) {
  const body = state === 'in' ? { in: true } : { out: true };
  try {
    const r = await fetch('/api/control', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
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

// ---------- Admin: reboot ----------
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
    logAutoInterval = setInterval(refreshLog, 10000);
  } else if (logAutoInterval) {
    clearInterval(logAutoInterval); logAutoInterval = null;
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

// ---------- Init ----------
async function init() {
  document.querySelectorAll('.tab').forEach(b =>
    b.addEventListener('click', () => activateTab(b.dataset.tab)));
  document.querySelectorAll('.sub-tab').forEach(b =>
    b.addEventListener('click', () => activateSubTab(b.dataset.sub)));

  $('btnIn').addEventListener('click', () => toggleControl('in'));
  $('btnOut').addEventListener('click', () => toggleControl('out'));
  $('btnReboot').addEventListener('click', rebootDevice);
  $('logRefresh').addEventListener('click', refreshLog);
  $('logAuto').addEventListener('change', toggleLogAuto);

  await fetchStatus();
  renderStatusTab();
  setTimeout(pollControl, 2000);
  setInterval(fetchStatus, 10000);
}

init();
