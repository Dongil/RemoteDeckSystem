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
  if (name === 'admin' && !_schedLoaded) loadSchedule();
  if (name === 'settings') {   // 설정 진입 시 현재 활성 sub-tab 로드 (첫 진입 시 device 채움)
    const act = document.querySelector('.sub-tab.active');
    activateSubTab(act ? act.dataset.sub : 'device');
  }
}

function activateSubTab(name) {
  document.querySelectorAll('.sub-tab').forEach(b => b.classList.toggle('active', b.dataset.sub === name));
  document.querySelectorAll('.sub-panel').forEach(p => p.classList.toggle('active', p.id === 'sub-' + name));
  if (name === 'device' && !_devLoaded) loadDeviceConfig();
  if (name === 'server' && !_srvLoaded) loadServerConfig();
  // 이미지 관리(sub-image)는 S5에서 연결
}

// ---------- 설정 (Device / Server Config) ----------
// 전체 config 객체를 로드해 두고, 저장 시 렌더된 필드만 병합 → 통짜 POST.
// (미렌더 필드 version_info/web_config_mode 등 보존 → 하위호환)
let _devCfg = null, _devLoaded = false;
let _srvCfg = null, _srvLoaded = false;
const val = id => $(id).value;
const setVal = (id, v) => { const e = $(id); if (e) e.value = (v == null ? '' : v); };
const hm = (h, m) => String(h == null ? 0 : h).padStart(2, '0') + ':' + String(m == null ? 0 : m).padStart(2, '0');
const parseHm = s => { const p = (s || '0:0').split(':'); return [parseInt(p[0]) || 0, parseInt(p[1]) || 0]; };

async function postConfig(url, obj, label) {
  try {
    const r = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(obj) });
    if (!r.ok) { const t = await r.text(); throw new Error(t || ('HTTP ' + r.status)); }
    toast(label + ' 설정 저장됨 (재부팅 후 반영)');
  } catch (e) { toast(label + ' 저장 실패: ' + e.message, true); }
}

async function loadDeviceConfig() {
  try {
    const r = await fetch('/api/config');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    _devCfg = await r.json();
    const c = _devCfg, n = c.network_config || {};
    setVal('dcDeviceId', c.device_id); setVal('dcServerUrl', c.server_url);
    $('dcNetEth').checked = !!n.using_ethernet;
    $('dcNetWifi').checked = !n.using_ethernet;
    $('dcDhcp').checked = !n.using_static;
    setVal('dcIp', n.static_ip); setVal('dcGw', n.static_gateway); setVal('dcSubnet', n.static_subnet);
    setVal('dcDns1', n.static_primaryDNS); setVal('dcDns2', n.static_secondaryDNS); setVal('dcMac', n.static_mac);
    setVal('dcWifiSsid', n.wifi_ssid); setVal('dcWifiPw', n.wifi_passwd);
    setVal('dcSleep', String(c.sleep_time == null ? 0 : c.sleep_time));
    const no = c.night_off || {};
    $('dcNightEn').checked = !!no.enabled;
    setVal('dcNightStart', hm(no.start_hour == null ? 22 : no.start_hour, no.start_minute));
    setVal('dcNightEnd', hm(no.end_hour == null ? 6 : no.end_hour, no.end_minute));
    _devLoaded = true;
  } catch (e) { toast('Device config 로드 실패: ' + e.message, true); }
}

async function saveDeviceConfig() {
  const c = _devCfg || {};
  c.device_id = val('dcDeviceId');
  c.server_url = val('dcServerUrl');
  const n = c.network_config = c.network_config || {};
  n.using_ethernet = $('dcNetEth').checked;
  n.using_static = !$('dcDhcp').checked;
  n.static_ip = val('dcIp'); n.static_gateway = val('dcGw'); n.static_subnet = val('dcSubnet');
  n.static_primaryDNS = val('dcDns1'); n.static_secondaryDNS = val('dcDns2'); n.static_mac = val('dcMac');
  n.wifi_ssid = val('dcWifiSsid'); n.wifi_passwd = val('dcWifiPw');
  c.sleep_time = parseInt(val('dcSleep')) || 0;
  const no = c.night_off = c.night_off || {};
  const s = parseHm(val('dcNightStart')), e = parseHm(val('dcNightEnd'));
  no.enabled = $('dcNightEn').checked;
  no.start_hour = s[0]; no.start_minute = s[1];
  no.end_hour = e[0]; no.end_minute = e[1];
  await postConfig('/api/config', c, 'Device');
}

async function loadServerConfig() {
  try {
    const r = await fetch('/api/serverconfig');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    _srvCfg = await r.json();
    const c = _srvCfg;
    setVal('scMqttUrl', c.mqtt_url); setVal('scMqttUser', c.mqtt_user); setVal('scMqttPw', c.mqtt_passwd);
    setVal('scMqttPort', c.mqtt_port); setVal('scMqttKeep', c.mqtt_keepalive);
    setVal('scMqttPub', c.mqtt_pub); setVal('scMqttSub', c.mqtt_sub); setVal('scMqttPing', c.mqtt_ping);
    setVal('scConfigUrl', c.config_url); setVal('scImageUrl', c.image_url); setVal('scStatusUrl', c.status_url);
    $('scHttpReq').checked = !!c.using_httprequest;
    _srvLoaded = true;
  } catch (e) { toast('Server config 로드 실패: ' + e.message, true); }
}

async function saveServerConfig() {
  const c = _srvCfg || {};
  c.mqtt_url = val('scMqttUrl'); c.mqtt_user = val('scMqttUser'); c.mqtt_passwd = val('scMqttPw');
  c.mqtt_port = parseInt(val('scMqttPort')) || 0; c.mqtt_keepalive = parseInt(val('scMqttKeep')) || 0;
  c.mqtt_pub = val('scMqttPub'); c.mqtt_sub = val('scMqttSub'); c.mqtt_ping = val('scMqttPing');
  c.config_url = val('scConfigUrl'); c.image_url = val('scImageUrl'); c.status_url = val('scStatusUrl');
  c.using_httprequest = $('scHttpReq').checked;
  await postConfig('/api/serverconfig', c, 'Server');
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
    const ov = $('otaCurVer'); if (ov) ov.textContent = `v${s.fw_version} (${s.fw_date || '-'})`;
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

// v2.7 S3: 펌웨어 OTA (기존 /api/ota 재활용)
async function uploadOta() {
  const f = $('otaFile').files[0];
  if (!f) { toast('OTA bin 파일을 선택하세요', true); return; }
  if (!f.name.toLowerCase().endsWith('.bin')) { toast('.bin 파일만 업로드 가능', true); return; }
  if (f.size > 1900 * 1024) { toast(`파일 크기 초과: ${(f.size/1024).toFixed(0)}KB (최대 ~1.85MB)`, true); return; }
  if (!confirm(`${f.name} (${(f.size/1024).toFixed(0)}KB) 업로드 후 자동 재부팅됩니다. 진행하시겠습니까?`)) return;
  const fd = new FormData(); fd.append('file', f);
  const bar = $('otaProgress'), fill = $('otaFill'), msg = $('otaMsg');
  bar.hidden = false; fill.style.width = '0%'; fill.textContent = '0%';
  msg.textContent = '업로드 중...';
  const done = () => { fill.style.width = '100%'; fill.textContent = '100%'; };
  try {
    const status = await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/ota');
      xhr.timeout = 120000;
      xhr.upload.onprogress = e => {
        if (e.lengthComputable) {
          // 업로드 완료(전송 100%) 시점에 100% 채움 — 이후 단말이 flash 쓰고 reboot.
          const p = Math.round(e.loaded * 100 / e.total);
          fill.style.width = p + '%'; fill.textContent = p + '%';
        }
      };
      xhr.onload = () => resolve(xhr.status);
      xhr.onerror = () => reject(new Error('drop'));       // 단말 reboot 로 연결 끊김 = 성공 신호
      xhr.ontimeout = () => reject(new Error('timeout'));
      xhr.send(fd);
    });
    if (status >= 200 && status < 300) {
      done();
      msg.textContent = '✅ OTA 완료 — 단말 재부팅(→ LCD 모드). 웹 재사용 시 다시 웹 설정 모드로 진입하세요.';
      toast('OTA 완료 — 재부팅 중');
    } else {
      fill.textContent = '실패';
      msg.textContent = 'OTA 실패 (HTTP ' + status + ') — 단말 로그를 확인하세요.';
      toast('OTA 실패 (HTTP ' + status + ')', true);
    }
  } catch (e) {
    // 업로드 직후 단말 reboot 로 연결이 끊기는 것은 정상(성공 신호) → 100% 표시.
    done();
    msg.textContent = '✅ 업로드 완료 — 연결 끊김은 재부팅 신호입니다. 단말이 LCD 모드로 전환됩니다.';
    toast('OTA 완료 — 재부팅 중', false, 5000);
  }
}

// v2.7 S4a: 재부팅 스케줄 (/api/schedule = deviceconfig.reboot_schedule)
let _schedLoaded = false;
async function loadSchedule() {
  try {
    const r = await fetch('/api/schedule');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const s = await r.json();
    $('schEn').checked = !!s.enabled;
    const days = s.days || [];
    document.querySelectorAll('.schDay').forEach(c => c.checked = days.includes(parseInt(c.value)));
    setVal('schTime', hm(s.hour == null ? 4 : s.hour, s.minute));
    _schedLoaded = true;
  } catch (e) { toast('스케줄 로드 실패: ' + e.message, true); }
}
async function saveSchedule() {
  const days = [];
  document.querySelectorAll('.schDay').forEach(c => { if (c.checked) days.push(parseInt(c.value)); });
  const t = parseHm(val('schTime'));
  await postConfig('/api/schedule', { enabled: $('schEn').checked, days, hour: t[0], minute: t[1] }, '재부팅 스케줄');
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
  $('otaUpload').addEventListener('click', uploadOta);
  $('schSave').addEventListener('click', saveSchedule);
  $('dcSave').addEventListener('click', saveDeviceConfig);
  $('scSave').addEventListener('click', saveServerConfig);
  $('logRefresh').addEventListener('click', refreshLog);
  $('logAuto').addEventListener('change', toggleLogAuto);

  await fetchStatus();
  renderStatusTab();
  setTimeout(pollControl, 2000);
  setInterval(fetchStatus, 10000);
}

init();
