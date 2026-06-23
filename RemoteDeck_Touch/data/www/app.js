// RemoteDeck_Touch v2.2 - Web UI
const $ = id => document.getElementById(id);
const ROLES = ['title', 'photo', 'name'];
const fmtBytes = n => n < 1024 ? n + 'B' : n < 1048576 ? (n/1024).toFixed(1)+'KB' : (n/1048576).toFixed(2)+'MB';

let autoRefreshTimer = null;

// ── Toast ──────────────────────────────────────
function toast(msg, fail = false, ms = 3000) {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast' + (fail ? ' fail' : '');
  t.hidden = false;
  clearTimeout(toast._t);
  toast._t = setTimeout(() => { t.hidden = true; }, ms);
}

// ── Tab nav ────────────────────────────────────
document.querySelectorAll('nav .tab').forEach(b => {
  b.addEventListener('click', () => {
    document.querySelectorAll('nav .tab').forEach(x => x.classList.remove('active'));
    document.querySelectorAll('.page').forEach(x => x.classList.remove('active'));
    b.classList.add('active');
    $('page-' + b.dataset.tab).classList.add('active');
    if (b.dataset.tab === 'logs') refreshLogs();
    if (b.dataset.tab === 'control') refreshControl();
  });
});

// ── CONTROL TAB (LCD mirror) ───────────────────
let _imagesConfig = null;
let _ctrlTimer = null;

function lcdImgUrl(role, status) {
  // status('IN'/'OUT') 면 photo 자리에 in.bmp/out.bmp 우선, 없으면 photo
  const t = Date.now();
  if (role === 'photo') {
    if (status === 'IN')  return `/api/images/in.bmp?t=${t}`;
    if (status === 'OUT') return `/api/images/out.bmp?t=${t}`;
  }
  return `/api/images/${role}.bmp?t=${t}`;
}

async function loadLcdImages(status) {
  // imagesconfig 의 url 사용도 가능하나 단순화: role 기반 fallback
  const setSrc = (id, role) => {
    const url = lcdImgUrl(role, status);
    const img = $(id);
    img.onerror = () => {
      // fallback: png 시도
      const alt = url.replace('.bmp', '.png');
      if (img.src !== alt) img.src = alt;
    };
    img.src = url;
  };
  setSrc('lcdTitle', 'title');
  setSrc('lcdName', 'name');
  setSrc('lcdPhoto', 'photo');  // status 가 IN/OUT 이면 in.bmp/out.bmp 가 우선
}

async function refreshControl() {
  try {
    const r = await fetch('/api/state');
    if (!r.ok) throw new Error('state ' + r.status);
    const s = await r.json();
    const badge = $('stateBadge');
    if (s.status === 'IN') {
      badge.textContent = '재실 (IN)';
      badge.className = 'state-badge in';
    } else if (s.status === 'OUT') {
      badge.textContent = '부재 (OUT)';
      badge.className = 'state-badge out';
    } else {
      badge.textContent = s.status || '—';
      badge.className = 'state-badge unknown';
    }
    $('lcdState').textContent = s.status || '';
    $('ctrlDeviceId').textContent = s.device_id ? '(' + s.device_id + ')' : '';
    await loadLcdImages(s.status);
  } catch (e) {
    $('stateBadge').textContent = 'error';
    $('stateBadge').className = 'state-badge unknown';
  }
}

async function postState(payload, label) {
  try {
    const r = await fetch('/api/state', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    toast(`${label} 전송 (${j.sent}) — 현재: ${j.current}`);
    setTimeout(refreshControl, 800);
  } catch (e) {
    toast(`${label} 실패: ${e.message}`, true);
  }
}

$('lcdPhotoArea').addEventListener('click', () => postState({ action: 'toggle' }, 'TOGGLE'));
$('btnSetIn').onclick    = () => postState({ status: 'IN' },  'IN');
$('btnSetOut').onclick   = () => postState({ status: 'OUT' }, 'OUT');
$('btnToggle').onclick   = () => postState({ action: 'toggle' }, 'TOGGLE');

// ── Status bar (top) ───────────────────────────
async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    if (!r.ok) throw new Error('status ' + r.status);
    const s = await r.json();
    $('statusBar').textContent =
      `heap=${fmtBytes(s.heap_free)} (min ${fmtBytes(s.heap_min)}) · ` +
      `${s.network.iface} ${s.network.ip} · ${s.fw}`;
  } catch (e) {
    $('statusBar').textContent = 'status error';
  }
}

// ── IMAGES TAB ─────────────────────────────────
async function renderImageCards() {
  let images = [];
  try {
    const r = await fetch('/api/images/list');
    const data = await r.json();
    images = data.images || [];
  } catch (e) {
    toast('이미지 목록 로드 실패: ' + e.message, true);
  }
  const t = Date.now();
  const cards = ROLES.map(role => {
    const img = images.find(i => i.name.toLowerCase() === role + '.png')
             || images.find(i => i.name.toLowerCase() === role + '.bmp');
    const thumb = img
      ? `<img src="/api/images/${img.name}?t=${t}" alt="${role}">`
      : `<div class="placeholder">no image</div>`;
    const meta = img ? `${img.name} · ${fmtBytes(img.size)}` : '— 기본 이미지 사용 —';
    const del = img ? `<button class="danger" data-del="${img.name}">삭제</button>` : '';
    return `<div class="img-card">
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
    toast('PNG/BMP만 업로드 가능', true); return;
  }
  if (file.size > 200 * 1024) {
    toast(`파일이 너무 큼: ${fmtBytes(file.size)} (max 200KB)`, true); return;
  }
  const role = $('fileInput').dataset.targetRole;
  const ext = name.endsWith('.png') ? '.png' : '.bmp';
  const finalName = role ? (role + ext) : file.name;
  const renamed = new File([file], finalName, { type: file.type });
  const fd = new FormData();
  fd.append('file', renamed);

  const bar = $('progressBar');
  const fill = $('progressFill');
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
    toast(`${finalName} 업로드 OK — LCD 갱신 중`);
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
  if (!confirm(`${name} 삭제?`)) return;
  try {
    const r = await fetch('/api/images/' + name, { method: 'DELETE' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    toast(`${name} 삭제됨`);
    setTimeout(renderImageCards, 600);
  } catch (e) {
    toast(`삭제 실패: ${e.message}`, true);
  }
}

function bindDragDrop() {
  const z = $('dropZone');
  ['dragenter', 'dragover'].forEach(ev => z.addEventListener(ev, e => {
    e.preventDefault(); z.classList.add('dragover');
  }));
  ['dragleave', 'drop'].forEach(ev => z.addEventListener(ev, e => {
    e.preventDefault(); z.classList.remove('dragover');
  }));
  z.addEventListener('drop', e => {
    if (e.dataTransfer.files[0]) uploadFile(e.dataTransfer.files[0]);
  });
  $('fileInput').addEventListener('change', e => {
    if (e.target.files[0]) uploadFile(e.target.files[0]);
  });
}

// ── CONFIG TAB ─────────────────────────────────
async function loadConfig(path, elemId) {
  try {
    const r = await fetch(path);
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const text = await r.text();
    // Pretty-print
    try {
      const obj = JSON.parse(text);
      $(elemId).value = JSON.stringify(obj, null, 2);
    } catch {
      $(elemId).value = text;
    }
  } catch (e) {
    toast(path + ' 로드 실패: ' + e.message, true);
  }
}

async function saveConfig(path, elemId, label) {
  const body = $(elemId).value;
  try {
    JSON.parse(body); // 검증
  } catch (e) {
    toast('JSON 파싱 실패: ' + e.message, true); return;
  }
  try {
    const r = await fetch(path, { method: 'POST', headers: {'Content-Type': 'application/json'}, body });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    if (j.reboot_required) toast(`${label} 저장됨 — 재부팅 필요`);
    else toast(`${label} 저장됨`);
  } catch (e) {
    toast(label + ' 저장 실패: ' + e.message, true);
  }
}

$('btnLoadDevice').onclick = () => loadConfig('/api/config', 'deviceConfigText');
$('btnSaveDevice').onclick = () => saveConfig('/api/config', 'deviceConfigText', 'Device config');
$('btnLoadServer').onclick = () => loadConfig('/api/serverconfig', 'serverConfigText');
$('btnSaveServer').onclick = () => saveConfig('/api/serverconfig', 'serverConfigText', 'Server config');
$('btnReboot').onclick = async () => {
  if (!confirm('단말을 재부팅하시겠습니까?')) return;
  try {
    await fetch('/api/reboot', { method: 'POST' });
    toast('재부팅 요청 전송됨');
  } catch (e) {
    toast('재부팅 실패: ' + e.message, true);
  }
};

// ── LOGS TAB ───────────────────────────────────
async function refreshLogs() {
  try {
    const r = await fetch('/api/log');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const data = await r.json();
    const html = (data.logs || []).map(e => {
      const ts = new Date(e.ts).toISOString().substring(11, 19);  // uptime hh:mm:ss (approx)
      return `<div class="log-entry"><span class="ts">${ts}</span><span class="event">${e.event}</span>${e.detail}</div>`;
    }).join('') || '<div class="log-entry">로그 없음</div>';
    $('logContainer').innerHTML = html;
  } catch (e) {
    $('logContainer').innerHTML = '<div class="log-entry">로그 로드 실패: ' + e.message + '</div>';
  }
}
$('btnRefreshLogs').onclick = refreshLogs;
$('autoRefresh').addEventListener('change', e => {
  if (autoRefreshTimer) clearInterval(autoRefreshTimer);
  if (e.target.checked) autoRefreshTimer = setInterval(refreshLogs, 5000);
});

// ── Init ───────────────────────────────────────
async function init() {
  bindDragDrop();
  await fetchStatus();
  await refreshControl();   // 첫 탭 (Control) — LCD 상태 즉시 표시
  await renderImageCards();
  await loadConfig('/api/config', 'deviceConfigText');
  await loadConfig('/api/serverconfig', 'serverConfigText');
  setInterval(fetchStatus, 5000);
  // Control 탭은 보일 때만 2초마다 polling (MQTT 응답 반영용)
  setInterval(() => {
    if ($('page-control').classList.contains('active')) refreshControl();
  }, 2000);
  // logs 탭은 보이는 시점에만 polling
  autoRefreshTimer = setInterval(() => {
    if ($('page-logs').classList.contains('active')) refreshLogs();
  }, 5000);
}
init();
