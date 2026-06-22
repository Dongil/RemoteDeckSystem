// RemoteDeck_Touch · Image Manager
// Design Ref: §5.1 Screen Layout, §5.4 Page UI Checklist

const $ = id => document.getElementById(id);
const ROLES = ['title', 'photo', 'name'];
const fmtBytes = n => n < 1024 ? n + 'B' : n < 1048576 ? (n/1024).toFixed(1)+'KB' : (n/1048576).toFixed(2)+'MB';
const now = () => new Date().toTimeString().slice(0, 8);

let activityLog = [];

function pushLog(event, detail, ok = true) {
  activityLog.unshift({ time: now(), event, detail, ok });
  if (activityLog.length > 20) activityLog.pop();
  renderLog();
}

function renderLog() {
  const c = $('activityLog');
  c.innerHTML = activityLog.map(e => `
    <div class="log-entry ${e.ok ? 'ok' : 'fail'}">
      <span class="time">${e.time}</span>
      <span class="event">${e.event}</span>
      ${e.detail}
    </div>`).join('');
}

function toast(msg, fail = false, ms = 3000) {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast' + (fail ? ' fail' : '');
  t.hidden = false;
  clearTimeout(toast._t);
  toast._t = setTimeout(() => { t.hidden = true; }, ms);
}

async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    if (!r.ok) throw new Error('status fetch ' + r.status);
    const s = await r.json();
    $('statusBar').textContent =
      `heap=${fmtBytes(s.heap_free)} (min ${fmtBytes(s.heap_min)}) · ` +
      `spiffs=${fmtBytes(s.spiffs_used)}/${fmtBytes(s.spiffs_total)} · ` +
      `${s.network.iface} ${s.network.ip} · v${s.fw_version}`;
  } catch (e) {
    $('statusBar').textContent = 'status error: ' + e.message;
  }
}

async function fetchImagesList() {
  try {
    const r = await fetch('/api/images/list');
    if (!r.ok) throw new Error('list fetch ' + r.status);
    return await r.json();
  } catch (e) {
    toast('이미지 목록 로드 실패: ' + e.message, true);
    return { images: [] };
  }
}

function matchImageForRole(images, role) {
  // 우선 role.png, 없으면 role.bmp
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

  // 카드 버튼 이벤트 바인딩
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
  // role 이 지정되어 있으면 파일명 강제 (예: role.png)
  const ext = name.endsWith('.png') ? '.png' : '.bmp';
  const finalName = role ? (role + ext) : file.name;

  const renamed = new File([file], finalName, { type: file.type });
  const fd = new FormData();
  fd.append('file', renamed);

  const bar = $('progressBar');
  const fill = $('progressFill');
  bar.hidden = false;
  fill.style.width = '0%';
  fill.textContent = '0%';

  try {
    await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/images/upload');
      xhr.upload.onprogress = e => {
        if (e.lengthComputable) {
          const p = Math.round(e.loaded * 100 / e.total);
          fill.style.width = p + '%';
          fill.textContent = p + '%';
        }
      };
      xhr.onload = () => {
        if (xhr.status >= 200 && xhr.status < 300) resolve(xhr.responseText);
        else reject(new Error('HTTP ' + xhr.status));
      };
      xhr.onerror = () => reject(new Error('network'));
      xhr.send(fd);
    });
    toast(`${finalName} 업로드 완료 — 단말 갱신 중`);
    pushLog('UPLOAD', `${finalName} (${fmtBytes(file.size)}) ok`, true);
    setTimeout(renderImageCards, 1500);  // 단말 핫리로드 대기 후 썸네일 갱신
  } catch (e) {
    toast(`업로드 실패: ${e.message}`, true);
    pushLog('UPLOAD', `${finalName} ${e.message}`, false);
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
    pushLog('DELETE', name, true);
    setTimeout(renderImageCards, 800);
  } catch (e) {
    toast(`삭제 실패: ${e.message}`, true);
    pushLog('DELETE', `${name} ${e.message}`, false);
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
    const f = e.dataTransfer.files[0];
    if (f) uploadFile(f);
  });
  $('fileInput').addEventListener('change', e => {
    if (e.target.files[0]) uploadFile(e.target.files[0]);
  });
}

async function init() {
  bindDragDrop();
  await fetchStatus();
  await renderImageCards();
  setInterval(fetchStatus, 5000);  // 5초마다 상태 갱신
}

init();
