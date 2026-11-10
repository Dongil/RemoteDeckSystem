// Tab navigation
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
  });
});

// Sub-tab navigation (settings)
document.querySelectorAll('.sub-tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.sub-tab').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.sub-page').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.stab).classList.add('active');
  });
});

// WebSocket
let ws;
function connectWS() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onmessage = (e) => {
    try {
      const msg = JSON.parse(e.data);
      if (msg.type === 'status') {
        const d = typeof msg.data === 'string' ? JSON.parse(msg.data) : msg.data;
        updateDashboard(d);
      } else if (msg.type === 'ota') {
        const pct = msg.data.progress;
        document.getElementById('ota-progress').style.width = pct + '%';
        document.getElementById('ota-pct').textContent = pct + '%';
      } else if (msg.type === 'log') {
        appendLogEntry(msg.data);
      }
    } catch(err) { console.error('WS parse error:', err); }
  };
  ws.onclose = () => setTimeout(connectWS, 3000);
}
connectWS();

// Dashboard update
function updateDashboard(d) {
  // v2.6.2: PC LED — 큰 폰트 + on/off dot 색상 (initial 이미지 스타일)
  const dot = document.getElementById('pc-dot');
  const txt = document.getElementById('pc-text');
  dot.className = 'led-dot ' + (d.pc_on ? 'on' : 'off');
  txt.textContent = d.pc_on ? 'ON' : 'OFF';

  document.getElementById('r1').textContent = d.relay1 ? 'ON' : 'OFF';
  document.getElementById('r2').textContent = d.relay2 ? 'ON' : 'OFF';
  if (d.gpio) document.getElementById('gpio-val').textContent = d.gpio.join(', ');
  document.getElementById('ip-addr').textContent = d.ip || '--';
  if (d.uptime !== undefined) {
    const h = Math.floor(d.uptime / 3600);
    const m = Math.floor((d.uptime % 3600) / 60);
    document.getElementById('uptime').textContent = h + '시간 ' + m + '분';
    window.__uptimeSec = d.uptime;
  }
  // v2.6.2 fix-5: MQTT 상태 색상 통일 (연결=초록, 연결안됨=빨강)
  const mqttEl = document.getElementById('mqtt-st');
  mqttEl.textContent = d.mqtt_connected ? '연결됨' : '연결 안됨';
  mqttEl.className = d.mqtt_connected ? 'st-ok' : 'st-fail';
  document.getElementById('ntp-time').textContent = d.time || '--';
  document.getElementById('fw-ver').textContent = 'v' + (d.fw_ver || '--');
  document.getElementById('ota-ver').textContent = d.fw_ver || '--';
  if (d.device_name) {
    document.getElementById('dev-name').textContent = d.device_name;
    document.getElementById('device-name-header').textContent = d.device_name;
  }
  if (d.ip) document.getElementById('dev-id').textContent = d.ip;

  // v2.6.2: attendance 카드 조건부 표시 및 상태 갱신
  const card = document.getElementById('attendance-card');
  if (card) {
    if (d.attendance && d.attendance.enabled) {
      card.style.display = '';
      const src = d.attendance.source === 'gpio2' ? 'SwitchMonitor (GPIO2)' : 'PC LED (PIR)';
      document.getElementById('att-source-badge').textContent = src;
      const present = d.attendance.current === 'present';
      document.getElementById('att-dot').className = present ? 'dot present' : 'dot absent';
      document.getElementById('att-text').textContent = present ? '재실' : '부재';
    } else {
      card.style.display = 'none';
    }
  }

  // rcm-hub-v3.0 §6.4: RCM 허브 카드 (hub_enabled 시 표시)
  updateHubCard(d);
}

// rcm-hub-v3.0 §6.4: 정지 사유 한글 매핑 (stop_reason = "<code> <reason>")
const HUB_STOP_TEXT = {
  '4001': '장비 키 또는 제품번호 오류',
  '4009': '같은 제품번호 장비가 이미 접속 중',
  '4010': '버전 불일치'
};
function updateHubCard(d) {
  const card = document.getElementById('hub-card');
  if (!card || d.hub_enabled === undefined) return;
  if (!d.hub_enabled) { card.style.display = 'none'; return; }
  const wasHidden = card.style.display === 'none';
  card.style.display = '';
  if (wasHidden) setTimeout(loadHubDetail, 0);
  const badge = document.getElementById('hub-badge');
  const reasonEl = document.getElementById('hub-reason');
  const reason = d.hub_stop_reason || '';
  let reasonText = '', state = 'wait', label = '연결 중';
  if (d.hub_connected) {
    state = 'ok'; label = '연결됨';
  } else if (d.hub_stopped) {
    state = 'fail'; label = '정지';
    const code = reason.split(' ')[0];
    reasonText = (HUB_STOP_TEXT[code] ? HUB_STOP_TEXT[code] + ' (' + code + ')' : reason) + ' — 설정 확인 후 재부팅하세요';
  } else if (reason === 'config incomplete') {
    state = 'fail'; label = '설정 불완전';
    reasonText = '허브 주소·제품번호·장비 키를 입력한 뒤 재부팅하세요 (설정 > 기타)';
  }
  badge.className = 'hub-badge ' + state;
  document.getElementById('hub-badge-text').textContent = label;
  card.className = 'card hub-card ' + state;
  reasonEl.textContent = reasonText;
  reasonEl.style.display = reasonText ? '' : 'none';
}

// rcm-hub-v3.0: 허브 카드 상세 (GET /api/hub, 3초 폴링 — 홈 탭에서 카드가 보일 때만)
const HUB_OP_TEXT = {
  'pc.powerOn': 'PC 켜기', 'pc.forceOff': 'PC 강제 끄기',
  'deck.relay.set': '릴레이 설정', 'deck.relay.pulse': '릴레이 펄스', 'deck.reboot': '재부팅',
  'state.get': '상태 조회', 'deck.state': '상태 보고', 'hub.closing': '허브 종료 알림'
};
const HUB_TYPE_TEXT = { challenge: '인증 요청', hello: '인증 응답', welcome: '접속 승인', socket: '연결' };
const HUB_CMD_OPS = ['pc.powerOn', 'pc.forceOff', 'deck.relay.set', 'deck.relay.pulse', 'deck.reboot'];
let hubDetail = null, hubShowAll = false;

function fmtDur(s) {
  if (s == null || s < 0) return '--';
  if (s < 60) return s + '초';
  if (s < 3600) return Math.floor(s / 60) + '분 ' + (s % 60) + '초';
  if (s < 86400) return Math.floor(s / 3600) + '시간 ' + Math.floor(s % 3600 / 60) + '분';
  return Math.floor(s / 86400) + '일 ' + Math.floor(s % 86400 / 3600) + '시간';
}
function fmtAgo(s) { return s == null || s < 0 ? '--' : (s < 2 ? '방금' : fmtDur(s) + ' 전'); }
function escHtml(t) { return String(t).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c])); }
function hubTime(e) {
  if (e.t) return new Date(e.t * 1000).toTimeString().slice(0, 8);
  return '-' + fmtDur(e.ago);
}

function loadHubDetail() {
  const card = document.getElementById('hub-card');
  if (!card || card.style.display === 'none') return;
  if (!document.getElementById('home').classList.contains('active')) return;
  fetch('/api/hub').then(r => r.json()).then(h => {
    hubDetail = h;
    const set = (id, v) => { const el = document.getElementById(id); if (el) el.innerHTML = v; };
    set('hub-addr', escHtml(h.url || '--'));
    set('hub-pid', escHtml(h.product_id || '--') + (h.room && h.room !== h.product_id ? ' / ' + escHtml(h.room) : ''));
    set('hub-uptime', h.connected ? fmtDur(h.connected_for_s) : '<span class="hub-dim">연결 안 됨</span>');
    set('hub-session', h.session ? escHtml(h.session) : '--');
    set('hub-hb', h.heartbeat_ms ? (h.heartbeat_ms / 1000) + '초' : '--');
    set('hub-conn', h.welcomes + '회 / ' + h.drops + '회');
    set('hub-rxtx', h.rx + ' / ' + h.tx + (h.res_fail ? ' <span class="hub-ng">(실패 응답 ' + h.res_fail + ')</span>' : ''));
    set('hub-lastrx', fmtAgo(h.last_rx_s));
    if (h.last_cmd) {
      const ok = h.last_cmd_ok === 1;
      set('hub-lastcmd', '<span class="' + (ok ? 'hub-ok' : 'hub-ng') + '">' + (ok ? '✓ ' : '✗ ') + '</span>' +
        escHtml(HUB_OP_TEXT[h.last_cmd] || h.last_cmd) + ' <span class="hub-dim">' + fmtAgo(h.last_cmd_s) + '</span>');
    } else set('hub-lastcmd', '<span class="hub-dim">없음</span>');
    renderHubLog();
  }).catch(() => {});
}

function renderHubLog() {
  const box = document.getElementById('hub-log');
  if (!box || !hubDetail) return;
  const onlyCmd = document.getElementById('hub-only-cmd').checked;
  let rows = hubDetail.log || [];
  if (onlyCmd) rows = rows.filter(e => (e.type === 'req' || e.type === 'res') && e.op !== 'state.get');
  const total = rows.length;
  if (!hubShowAll) rows = rows.slice(0, 12);
  const more = document.getElementById('hub-more');
  more.style.display = total > 12 ? '' : 'none';
  more.textContent = hubShowAll ? '접기' : '더 보기 (' + total + ')';
  if (!rows.length) {
    box.innerHTML = '<p class="hub-empty">' + (onlyCmd ? '받은 명령이 없습니다' : '아직 기록이 없습니다') + '</p>';
    return;
  }
  const DIR = {
    i: ['i', '↓', '수신 (허브 → 장비)'], o: ['o', '↑', '송신 (장비 → 허브)'],
    c: ['c', '⚡', '소켓 연결'], d: ['x', '✕', '소켓 끊김']
  };
  box.innerHTML = rows.map(e => {
    const [dirCls, dirTxt, dirTitle] = DIR[e.dir] || DIR.c;
    const name = e.op ? (HUB_OP_TEXT[e.op] || e.op) : (HUB_TYPE_TEXT[e.type] || e.type);
    const sub = e.type === 'req' ? '명령' : e.type === 'res' ? '응답' : '';
    const isCmd = e.type === 'req' && HUB_CMD_OPS.includes(e.op);
    const res = e.ok === true ? '<span class="hub-ok">✓</span>' : e.ok === false ? '<span class="hub-ng">✗</span>' : '';
    return '<div class="hub-row' + (isCmd ? ' cmd' : '') + (e.ok === false || e.dir === 'd' ? ' err' : '') + '">' +
      '<span class="t">' + hubTime(e) + '</span>' +
      '<span class="d ' + dirCls + '" title="' + dirTitle + '">' + dirTxt + '</span>' +
      '<span class="op" title="' + escHtml(e.op || e.type) + (e.id ? ' #' + escHtml(e.id) : '') + '">' +
        escHtml(name) + (sub ? '<small>' + sub + '</small>' : '') + '</span>' +
      '<span class="det" title="' + escHtml(e.detail || '') + '">' + escHtml(e.detail || '') + '</span>' +
      '<span class="r">' + res + '</span></div>';
  }).join('');
}
function toggleHubMore() { hubShowAll = !hubShowAll; renderHubLog(); }
setInterval(loadHubDetail, 3000);

// v2.6.2: attendance 전송 이력 (5s 폴링)
// fix-2/4: O/X 아이콘, 시분초 시각 (device NTP time 사용)
function loadAttendanceHistory() {
  const card = document.getElementById('attendance-card');
  if (!card || card.style.display === 'none') return;
  fetch('/api/attendance/history').then(r => r.json()).then(d => {
    const box = document.getElementById('att-history');
    if (!box) return;
    if (!d.history || d.history.length === 0) {
      box.innerHTML = '<p class="att-empty">아직 기록이 없습니다</p>';
      return;
    }
    // v2.6.2 fix-7: 최근 5건만 표시 (링버퍼는 서버에서 8건 유지, 외부 API 완전성)
    box.innerHTML = d.history.slice(0, 5).map(e => {
      // v2.6.2 fix-4: device NTP 시각 표시 (time 필드). 없으면 상대 uptime fallback.
      let timeLabel = e.time && e.time.length > 0 ? e.time : '';
      if (!timeLabel) {
        const upSec = (window.__uptimeSec || 0);
        const ago = Math.max(0, upSec - Math.floor(e.ts / 1000));
        timeLabel = ago + '초 전';
      }
      const stCls = e.active ? 'present' : 'absent';
      const stTxt = e.active ? '재실' : '부재';
      // v2.6.2 fix-2/3: O/X 아이콘 + 실제 HTTP 결과 반영
      let tx;
      if (e.http === 200) {
        tx = '<span class="tx ok"><b>O</b> 전송성공</span>';
      } else if (e.http === -1) {
        tx = '<span class="tx pending">⏳ 전송중</span>';
      } else {
        // -2 begin fail / -11 timeout / 4xx/5xx 등 모두 실패
        tx = '<span class="tx fail"><b>X</b> 전송실패 (' + e.http + ')</span>';
      }
      return '<div class="att-row"><span class="time">' + timeLabel + '</span>'
           + '<span class="state ' + stCls + '">' + stTxt + '</span>'
           + tx + '</div>';
    }).join('');
  }).catch(() => {});
}
setInterval(loadAttendanceHistory, 5000);
document.querySelector('[data-tab="home"]').addEventListener('click', loadAttendanceHistory);
document.querySelector('[data-tab="home"]').addEventListener('click', () => setTimeout(loadHubDetail, 0));

// Initial load
fetch('/api/status').then(r => r.json()).then(d => {
  updateDashboard(d);
  document.getElementById('dev-id').textContent = d.ip || '--';
}).catch(() => {});

// Relay control
function relay(num, action) {
  fetch('/api/relay', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({relay: num, action: action, duration: 500})
  }).then(r => {
    // rcm-hub-v3.0 G-01: 허브 사용 중 릴레이1 켜기(latch)는 409로 거부됨
    if (r.status === 409) alert('RCM 허브 연동 중에는 릴레이1(PC 전원)을 켜 둘 수 없습니다. 펄스를 사용하거나 허브에서 전원을 제어하세요.');
    else if (!r.ok) alert('릴레이 제어 실패 (HTTP ' + r.status + ')');
  }).catch(() => {});
}

// WOL
function sendWOL() {
  const mac = document.getElementById('wol-mac').value;
  if (!mac) return alert('MAC 주소를 입력하세요');
  fetch('/api/wol', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({mac: mac})
  }).then(() => alert('WOL 패킷을 전송했습니다'));
}

// Schedule
function loadSchedules() {
  fetch('/api/schedule').then(r => r.json()).then(d => {
    const list = document.getElementById('sched-list');
    list.innerHTML = '';
    if (!d.schedules || d.schedules.length === 0) {
      list.innerHTML = '<p style="color:#888">등록된 스케줄이 없습니다</p>';
      return;
    }
    d.schedules.forEach(s => {
      const days = ['일','월','화','수','목','금','토']
        .filter((_, i) => s.days & (1 << i)).join(',');
      const actionMap = {on:'켜기', off:'끄기', toggle:'토글', reboot:'재부팅'};
      const div = document.createElement('div');
      div.className = 'status-line';
      // Design Ref: §5.4 — reboot은 🔁 아이콘 + relay 필드 미표시
      const label = (s.action === 'reboot')
        ? '🔁 ' + (actionMap[s.action] || s.action)
        : '릴레이' + s.relay + ' ' + (actionMap[s.action] || s.action);
      div.innerHTML = '#' + s.id + ' ' +
        String(s.hour).padStart(2,'0') + ':' + String(s.minute).padStart(2,'0') +
        ' [' + days + '] ' + label +
        (s.enabled ? ' <span style="color:#0f0">활성</span>' : ' <span style="color:#f00">비활성</span>') +
        ' <button onclick="delSchedule(' + s.id + ')" style="margin-left:8px;padding:2px 8px">삭제</button>';
      list.appendChild(div);
    });
  });
}
loadSchedules();

function addSchedule() {
  const time = document.getElementById('s-time').value.split(':');
  let days = 0;
  if (document.getElementById('d-sun').checked) days |= 1;
  if (document.getElementById('d-mon').checked) days |= 2;
  if (document.getElementById('d-tue').checked) days |= 4;
  if (document.getElementById('d-wed').checked) days |= 8;
  if (document.getElementById('d-thu').checked) days |= 16;
  if (document.getElementById('d-fri').checked) days |= 32;
  if (document.getElementById('d-sat').checked) days |= 64;

  fetch('/api/schedule', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      id: 0, enabled: true,
      hour: parseInt(time[0]), minute: parseInt(time[1]),
      days: days,
      action: document.getElementById('s-action').value,
      relay: parseInt(document.getElementById('s-relay').value)
    })
  }).then(() => loadSchedules());
}

function delSchedule(id) {
  if (!confirm('이 스케줄을 삭제하시겠습니까?')) return;
  fetch('/api/schedule?id=' + id, {method: 'DELETE'}).then(() => {
    loadSchedules();
    // 관리 탭에서 삭제한 경우 재부팅 스케줄 리스트도 즉시 새로고침
    if (typeof loadRebootSchedules === 'function') loadRebootSchedules();
  });
}

// ═══════════════════════════════════════════
//  Settings - Config Load / Save
// ═══════════════════════════════════════════

let _cfgCache = {};

function loadConfig() {
  fetch('/api/config').then(r => r.json()).then(d => {
    _cfgCache = d;

    // 장치 정보
    document.getElementById('cfg-device-id').value = d.device_id || '';
    document.getElementById('cfg-device-name').value = d.device_name || '';

    // 네트워크 모드
    const mode = d.network?.mode || 'ethernet';
    document.querySelector('input[name="net-mode"][value="' + mode + '"]').checked = true;

    // 이더넷
    document.getElementById('cfg-eth-dhcp').checked = d.network?.ethernet?.dhcp || false;
    document.getElementById('cfg-eth-ip').value = d.network?.ethernet?.ip || '';
    document.getElementById('cfg-eth-gw').value = d.network?.ethernet?.gateway || '';
    document.getElementById('cfg-eth-subnet').value = d.network?.ethernet?.subnet || '';
    document.getElementById('cfg-eth-dns1').value = d.network?.ethernet?.dns1 || '';
    document.getElementById('cfg-eth-mac').value = d.network?.ethernet?.mac || '';

    // WiFi
    document.getElementById('cfg-wifi-ssid').value = d.network?.wifi?.ssid || '';
    document.getElementById('cfg-wifi-pw').value = d.network?.wifi?.password || '';
    document.getElementById('cfg-wifi-dhcp').checked = d.network?.wifi?.dhcp || false;
    document.getElementById('cfg-wifi-ip').value = d.network?.wifi?.ip || '';
    document.getElementById('cfg-wifi-gw').value = d.network?.wifi?.gateway || '';
    document.getElementById('cfg-wifi-subnet').value = d.network?.wifi?.subnet || '';
    document.getElementById('cfg-wifi-dns1').value = d.network?.wifi?.dns1 || '';
    document.getElementById('cfg-wifi-mac').value = d.network?.wifi?.mac || '';

    // MQTT
    const mqttEnabled = d.mqtt?.broker ? true : false;
    document.getElementById('cfg-mqtt-enable').checked = mqttEnabled;
    document.getElementById('cfg-mqtt-broker').value = d.mqtt?.broker || '';
    document.getElementById('cfg-mqtt-port').value = d.mqtt?.port || 1883;
    document.getElementById('cfg-mqtt-user').value = d.mqtt?.user || '';
    document.getElementById('cfg-mqtt-pass').value = '';
    document.getElementById('cfg-mqtt-keepalive').value = d.mqtt?.keepalive || 120;
    document.getElementById('cfg-mqtt-pub').value = d.mqtt?.topic_pub || '';
    document.getElementById('cfg-mqtt-sub').value = d.mqtt?.topic_sub || '';
    document.getElementById('cfg-mqtt-ping').value = d.mqtt?.topic_ping || '';
    toggleMQTTFields();

    // Web Request
    document.getElementById('cfg-wr-enable').checked = d.web_request?.enabled || false;
    document.getElementById('cfg-wr-timeout').value = d.web_request?.timeout_ms || 5000;
    document.getElementById('cfg-wr-relay1-on').value = d.web_request?.relay1_on || '';
    document.getElementById('cfg-wr-relay1-off').value = d.web_request?.relay1_off || '';
    document.getElementById('cfg-wr-relay2-on').value = d.web_request?.relay2_on || '';
    document.getElementById('cfg-wr-relay2-off').value = d.web_request?.relay2_off || '';
    document.getElementById('cfg-wr-pcled-on').value = d.web_request?.pcled_on || '';
    document.getElementById('cfg-wr-pcled-off').value = d.web_request?.pcled_off || '';
    document.getElementById('cfg-wr-gpio1-high').value = d.web_request?.gpio1_high || '';
    document.getElementById('cfg-wr-gpio1-low').value = d.web_request?.gpio1_low || '';
    document.getElementById('cfg-wr-gpio2-high').value = d.web_request?.gpio2_high || '';
    document.getElementById('cfg-wr-gpio2-low').value = d.web_request?.gpio2_low || '';
    document.getElementById('cfg-wr-gpio3-high').value = d.web_request?.gpio3_high || '';
    document.getElementById('cfg-wr-gpio3-low').value = d.web_request?.gpio3_low || '';
    toggleWRFields();

    // 기타
    document.getElementById('cfg-relay-short').value = d.relay?.pulse_short_ms || 500;
    document.getElementById('cfg-relay-long').value = d.relay?.pulse_long_ms || 5000;
    document.getElementById('cfg-mon-poll').value = d.monitor?.pcled_poll_interval_ms || 1000;
    document.getElementById('cfg-mon-notify').checked = d.monitor?.auto_notify !== false;
    document.getElementById('cfg-wol-mac').value = d.wol?.target_mac || '';
    document.getElementById('cfg-ntp-server').value = d.ntp?.server || '';
    document.getElementById('cfg-ntp-tz').value = d.ntp?.timezone || '';

    // v2.6.1 재부재 시스템
    document.getElementById('cfg-att-enabled').checked = d.attendance?.enabled || false;
    document.getElementById('cfg-att-source').value = d.attendance?.source || 'pcled';
    document.getElementById('cfg-att-on').value = d.web_request?.attendance_on || '';
    document.getElementById('cfg-att-off').value = d.web_request?.attendance_off || '';

    // rcm-hub-v3.0 RCM-API 연동 (device_key는 가려서 옴: "****" = 저장됨)
    document.getElementById('cfg-hub-enabled').checked = d.hub?.enabled || false;
    document.getElementById('cfg-hub-host').value = d.hub?.host || '';
    document.getElementById('cfg-hub-port').value = d.hub?.port || 8891;
    document.getElementById('cfg-hub-path').value = d.hub?.path || '/v1/ws';
    document.getElementById('cfg-hub-tls').checked = d.hub?.tls || false;
    document.getElementById('cfg-hub-pid').value = d.hub?.product_id || '';
    const keyEl = document.getElementById('cfg-hub-key');
    keyEl.value = '';
    keyEl.placeholder = d.hub?.device_key ? '저장됨 (변경 시에만 입력)' : '장비 키 입력';
  });
}

// MQTT enable/disable toggle
document.getElementById('cfg-mqtt-enable').addEventListener('change', toggleMQTTFields);
function toggleMQTTFields() {
  const enabled = document.getElementById('cfg-mqtt-enable').checked;
  document.getElementById('mqtt-fields').style.opacity = enabled ? '1' : '0.4';
  document.querySelectorAll('#mqtt-fields input').forEach(el => el.disabled = !enabled);
}

// ─── Save: Network (with reboot) ───
function saveNetwork() {
  if (!confirm('네트워크 설정을 저장하고 장치를 재부팅하시겠습니까?')) return;
  const mode = document.querySelector('input[name="net-mode"]:checked').value;
  const cfg = {
    device_id: document.getElementById('cfg-device-id').value,
    device_name: document.getElementById('cfg-device-name').value,
    network: {
      mode: mode,
      ethernet: {
        dhcp: document.getElementById('cfg-eth-dhcp').checked,
        ip: document.getElementById('cfg-eth-ip').value,
        gateway: document.getElementById('cfg-eth-gw').value,
        subnet: document.getElementById('cfg-eth-subnet').value,
        dns1: document.getElementById('cfg-eth-dns1').value
      },
      wifi: {
        ssid: document.getElementById('cfg-wifi-ssid').value,
        password: document.getElementById('cfg-wifi-pw').value,
        dhcp: document.getElementById('cfg-wifi-dhcp').checked,
        ip: document.getElementById('cfg-wifi-ip').value,
        gateway: document.getElementById('cfg-wifi-gw').value,
        subnet: document.getElementById('cfg-wifi-subnet').value,
        dns1: document.getElementById('cfg-wifi-dns1').value
      }
    }
  };
  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(cfg)
  }).then(r => r.json()).then(d => {
    if (d.ok) {
      fetch('/api/reboot', {method: 'POST'});
      alert('저장 완료! 장치가 재부팅됩니다.');
      setTimeout(() => location.reload(), 5000);
    } else alert('저장 실패');
  });
}

// Resolve hostname to IP for ESP32 Ethernet compatibility (MQTT broker, RCM hub host 공용)
async function resolveHostToIp(host) {
  if (/^\d{1,3}(\.\d{1,3}){3}$/.test(host)) return host;
  try {
    const dnsResp = await fetch('https://dns.google/resolve?name=' + encodeURIComponent(host) + '&type=A');
    const dnsData = await dnsResp.json();
    const aRecord = dnsData.Answer?.find(a => a.type === 1);
    if (aRecord && confirm(host + ' → ' + aRecord.data + '\nIP로 변환하여 저장하시겠습니까?\n(ESP32 Ethernet DNS 제한)')) {
      return aRecord.data;
    }
  } catch(e) { /* DNS failed, save hostname as-is */ }
  return host;
}

// ─── Save: MQTT (no reboot) ───
async function saveMQTT() {
  const enabled = document.getElementById('cfg-mqtt-enable').checked;
  let broker = document.getElementById('cfg-mqtt-broker').value;
  // Resolve hostname to IP for ESP32 Ethernet compatibility
  if (enabled && broker) broker = await resolveHostToIp(broker);
  const cfg = {
    mqtt: {
      broker: enabled ? broker : '',
      port: parseInt(document.getElementById('cfg-mqtt-port').value) || 1883,
      user: document.getElementById('cfg-mqtt-user').value,
      password: document.getElementById('cfg-mqtt-pass').value,
      keepalive: parseInt(document.getElementById('cfg-mqtt-keepalive').value) || 120,
      topic_pub: document.getElementById('cfg-mqtt-pub').value,
      topic_sub: document.getElementById('cfg-mqtt-sub').value,
      topic_ping: document.getElementById('cfg-mqtt-ping').value
    }
  };
  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(cfg)
  }).then(r => r.json()).then(d => {
    if (d.ok) alert('MQTT 설정이 저장되었습니다.\n적용하려면 재부팅이 필요합니다.');
    else alert('저장 실패');
  });
}

// ─── Test: MQTT (async: POST to start, GET to poll) ───
async function testMQTT() {
  const el = document.getElementById('mqtt-test-result');
  let broker = document.getElementById('cfg-mqtt-broker').value;
  if (!broker) { el.textContent = '브로커 주소를 입력하세요'; el.className = 'test-result fail'; return; }
  const isIP = /^\d{1,3}(\.\d{1,3}){3}$/.test(broker);
  if (!isIP) {
    el.textContent = 'DNS 해석 중...';
    el.className = 'test-result';
    // Resolve hostname via DNS lookup API (browser resolves)
    try {
      const dnsResp = await fetch('https://dns.google/resolve?name=' + encodeURIComponent(broker) + '&type=A');
      const dnsData = await dnsResp.json();
      if (dnsData.Answer && dnsData.Answer.length > 0) {
        broker = dnsData.Answer.find(a => a.type === 1)?.data || broker;
        el.textContent = broker + '로 테스트 중...';
      } else {
        el.textContent = 'DNS 해석 실패: ' + broker;
        el.className = 'test-result fail';
        return;
      }
    } catch(e) {
      el.textContent = 'DNS 해석 실패 (IP 주소를 직접 입력하세요)';
      el.className = 'test-result fail';
      return;
    }
  } else {
    el.textContent = '연결 테스트 중...';
    el.className = 'test-result';
  }
  fetch('/api/mqtttest', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      broker: broker,
      port: parseInt(document.getElementById('cfg-mqtt-port').value) || 1883,
      user: document.getElementById('cfg-mqtt-user').value,
      password: document.getElementById('cfg-mqtt-pass').value
    })
  }).then(r => r.json()).then(d => {
    if (!d.ok) {
      el.textContent = '테스트 시작 실패: ' + (d.error || '');
      el.className = 'test-result fail';
      return;
    }
    // Poll for result
    let polls = 0;
    const poll = setInterval(() => {
      fetch('/api/mqtttest').then(r => r.json()).then(r => {
        if (r.status === 'testing') {
          polls++;
          if (polls > 20) { // 10 seconds max
            clearInterval(poll);
            el.textContent = '시간 초과';
            el.className = 'test-result fail';
          }
          return;
        }
        clearInterval(poll);
        if (r.status === 'ok') {
          el.textContent = '연결 성공!';
          el.className = 'test-result ok';
        } else {
          el.textContent = '연결 실패';
          el.className = 'test-result fail';
        }
      });
    }, 500);
  }).catch(() => {
    el.textContent = '테스트 요청 실패';
    el.className = 'test-result fail';
  });
}

// ─── Save: Etc (no reboot) ───
async function saveEtc() {
  // rcm-hub-v3.0 §6.3: 허브 주소 hostname → IP 변환 (ESP32 Ethernet DNS 제한, saveMQTT와 같은 방식)
  const hubEnabled = document.getElementById('cfg-hub-enabled').checked;
  let hubHost = document.getElementById('cfg-hub-host').value.trim();
  if (hubEnabled && hubHost) hubHost = await resolveHostToIp(hubHost);
  const hubKey = document.getElementById('cfg-hub-key').value.trim();
  const cfg = {
    relay: {
      pulse_short_ms: parseInt(document.getElementById('cfg-relay-short').value) || 500,
      pulse_long_ms: parseInt(document.getElementById('cfg-relay-long').value) || 5000
    },
    monitor: {
      pcled_poll_interval_ms: parseInt(document.getElementById('cfg-mon-poll').value) || 1000,
      auto_notify: document.getElementById('cfg-mon-notify').checked
    },
    wol: {
      target_mac: document.getElementById('cfg-wol-mac').value
    },
    ntp: {
      server: document.getElementById('cfg-ntp-server').value,
      timezone: document.getElementById('cfg-ntp-tz').value
    },
    // v2.6.1 재부재 시스템
    attendance: {
      enabled: document.getElementById('cfg-att-enabled').checked,
      source: document.getElementById('cfg-att-source').value
    },
    web_request: {
      attendance_on: document.getElementById('cfg-att-on').value,
      attendance_off: document.getElementById('cfg-att-off').value
    },
    // rcm-hub-v3.0 RCM-API 연동 (키를 비워 두면 "****" = 기존 키 유지)
    hub: {
      enabled: hubEnabled,
      host: hubHost,
      port: parseInt(document.getElementById('cfg-hub-port').value) || 8891,
      path: document.getElementById('cfg-hub-path').value.trim() || '/v1/ws',
      tls: document.getElementById('cfg-hub-tls').checked,
      product_id: document.getElementById('cfg-hub-pid').value.trim(),
      device_key: hubKey || '****'
    }
  };
  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(cfg)
  }).then(r => r.json()).then(d => {
    if (d.ok) alert('설정이 저장되었습니다.');
    else alert('저장 실패');
  });
}

// Web Request enable/disable toggle
document.getElementById('cfg-wr-enable').addEventListener('change', toggleWRFields);
function toggleWRFields() {
  const enabled = document.getElementById('cfg-wr-enable').checked;
  document.getElementById('wr-fields').style.opacity = enabled ? '1' : '0.4';
  document.querySelectorAll('#wr-fields input').forEach(el => el.disabled = !enabled);
}

// ─── Save: Web Request (no reboot) ───
function saveWebRequest() {
  const cfg = {
    web_request: {
      enabled: document.getElementById('cfg-wr-enable').checked,
      timeout_ms: parseInt(document.getElementById('cfg-wr-timeout').value) || 5000,
      relay1_on: document.getElementById('cfg-wr-relay1-on').value,
      relay1_off: document.getElementById('cfg-wr-relay1-off').value,
      relay2_on: document.getElementById('cfg-wr-relay2-on').value,
      relay2_off: document.getElementById('cfg-wr-relay2-off').value,
      pcled_on: document.getElementById('cfg-wr-pcled-on').value,
      pcled_off: document.getElementById('cfg-wr-pcled-off').value,
      gpio1_high: document.getElementById('cfg-wr-gpio1-high').value,
      gpio1_low: document.getElementById('cfg-wr-gpio1-low').value,
      gpio2_high: document.getElementById('cfg-wr-gpio2-high').value,
      gpio2_low: document.getElementById('cfg-wr-gpio2-low').value,
      gpio3_high: document.getElementById('cfg-wr-gpio3-high').value,
      gpio3_low: document.getElementById('cfg-wr-gpio3-low').value
    }
  };
  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(cfg)
  }).then(r => r.json()).then(d => {
    if (d.ok) alert('Web Request 설정이 저장되었습니다.');
    else alert('저장 실패');
  });
}

// Reboot device
function rebootDevice() {
  if (!confirm('장치를 재부팅하시겠습니까?')) return;
  fetch('/api/reboot', {method: 'POST'});
  alert('장치가 재부팅됩니다.');
  setTimeout(() => location.reload(), 5000);
}

// Account Change (ID + PW)
function changeAccount() {
  const curPw = document.getElementById('cur-pw').value;
  const newId = document.getElementById('new-id').value;
  const newPw = document.getElementById('new-pw').value;
  const confirmPw = document.getElementById('confirm-pw').value;
  if (!curPw) return alert('현재 패스워드를 입력하세요');
  if (!newId) return alert('새 ID를 입력하세요');
  if (!newPw) return alert('새 패스워드를 입력하세요');
  if (newPw !== confirmPw) return alert('새 패스워드가 일치하지 않습니다');

  fetch('/api/auth', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({current_pass: curPw, new_user: newId, new_pass: newPw, confirm_pass: confirmPw})
  }).then(r => {
    if (r.status === 401) { alert('인증 실패. 브라우저를 새로고침하세요.'); return; }
    return r.json();
  }).then(d => {
    if (d?.ok) {
      alert('계정이 변경되었습니다. 다시 로그인하세요.');
      location.reload();
    } else {
      alert(d?.error || '변경 실패');
    }
  });
}

// rcm-hub-v3.0 G-03: 서버 응답(200 ok / 401 / 실패)으로 결과 판정.
// v3.0부터 서버는 응답 후 재부팅한다. 응답이 오지 않으면(v2.x 장비) 업로드 완료 20초 뒤 성공으로 보고 재로드.
function sendOTA(formData, barId, pctId, okMsg) {
  const xhr = new XMLHttpRequest();
  let settled = false, fallback = null;
  const done = (msg, reload) => {
    if (settled) return;
    settled = true;
    if (fallback) clearTimeout(fallback);
    alert(msg);
    if (reload) setTimeout(() => location.reload(), 10000);
  };
  xhr.open('POST', '/api/ota');
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded / e.total * 100);
      document.getElementById(barId).style.width = pct + '%';
      document.getElementById(pctId).textContent = pct + '%';
    }
  };
  xhr.upload.onload = () => {
    fallback = setTimeout(() => done(okMsg, true), 20000);
  };
  xhr.onload = () => {
    if (xhr.status === 401) return done('인증 실패: 다시 로그인한 뒤 업로드하세요.', false);
    let ok = false;
    try { ok = xhr.status === 200 && JSON.parse(xhr.responseText).ok === true; } catch (e) {}
    if (ok) done(okMsg, true);
    else done('업로드 실패 (HTTP ' + xhr.status + '): 파일을 확인하세요. 장치는 재부팅하지 않습니다.', false);
  };
  // 응답 전에 연결이 끊기면(구버전 장비의 즉시 재부팅) 업로드가 끝난 경우에만 성공으로 본다
  xhr.onerror = () => { if (fallback) done(okMsg, true); else done('업로드 중 연결이 끊겼습니다.', false); };
  xhr.send(formData);
}

// OTA Upload
function uploadOTA() {
  const file = document.getElementById('ota-file').files[0];
  if (!file) return alert('.bin 파일을 선택하세요');
  if (!confirm('펌웨어를 업로드하고 재부팅하시겠습니까?')) return;

  const formData = new FormData();
  formData.append('firmware', file);
  sendOTA(formData, 'ota-progress', 'ota-pct', '업로드 완료! 장치가 재부팅됩니다...');
}

// RemoteDeck_PC_v2.5 — SPIFFS(웹 UI) 업로드. 확장자에 .spiffs.bin 이나 .fs.bin 이 포함되어야
// OTAHandler 가 U_SPIFFS 파티션으로 라우팅함.
function uploadFS() {
  const file = document.getElementById('fs-file').files[0];
  if (!file) return alert('.bin 파일을 선택하세요');
  const nameLower = file.name.toLowerCase();
  // v2.6.2 fix-6: 서버 OTAHandler(indexOf substring)와 동일한 substring 검사.
  // "spiffs" | "_fs." | "-fs." | ".fs." 가 파일명 어디에라도 있으면 SPIFFS로 라우팅됨.
  if (!/spiffs|_fs\.|-fs\.|\.fs\./.test(nameLower)) {
    if (!confirm(`선택한 파일명 "${file.name}" 이 spiffs.bin 규칙에 맞지 않습니다. 그대로 진행하면 firmware 파티션으로 flash 됩니다. 계속?`)) return;
  }
  if (!confirm('웹 UI 자산을 업로드하고 재부팅하시겠습니까?')) return;

  const formData = new FormData();
  formData.append('spiffs', file);   // 필드명은 서버에서 무시. 파일명이 판정 기준.
  sendOTA(formData, 'fs-progress', 'fs-pct', '웹 UI 업로드 완료! 장치가 재부팅됩니다...');
}

// Logs
function loadLogs() {
  fetch('/api/log').then(r => r.json()).then(d => {
    const list = document.getElementById('log-list');
    list.innerHTML = '';
    if (!d.logs) return;
    d.logs.reverse().forEach(l => appendLogEntry(l));
  });
}

function appendLogEntry(l) {
  const list = document.getElementById('log-list');
  const div = document.createElement('div');
  div.className = 'log-entry';
  div.innerHTML = '<span class="time">' + (l.time || l.timestamp) + '</span> ' +
    '<span class="event">' + l.event + '</span> ' + (l.detail || '');
  list.prepend(div);
}

// Auto-load config when settings tab is shown
document.querySelector('[data-tab="settings"]').addEventListener('click', loadConfig);
document.querySelector('[data-tab="log"]').addEventListener('click', loadLogs);

// ═══════════════════════════════════════════
//  Admin Tab — 관리 (Design Ref: §5.4)
// ═══════════════════════════════════════════

function confirmReboot() {
  if (!confirm('기기를 지금 재부팅합니다. 계속하시겠습니까?')) return;
  // Plan SC-1: v2.4.7 응답 flush 전 restart 정책이라 timeout/ConnectionReset은 정상
  fetch('/api/reboot', { method: 'POST' }).catch(() => {});
  alert('재부팅 요청 전송됨. 잠시 후 다시 접속하세요.');
  // saveNetwork/rebootDevice와 동일하게 자동 리로드 (5초 대기)
  setTimeout(() => location.reload(), 5000);
}

function loadRebootSchedules() {
  fetch('/api/schedule').then(r => r.json()).then(d => {
    const list = document.getElementById('reboot-sched-list');
    list.innerHTML = '';
    const items = (d.schedules || []).filter(s => s.action === 'reboot');
    if (items.length === 0) {
      list.innerHTML = '<p style="color:#888">등록된 재부팅 스케줄이 없습니다</p>';
      return;
    }
    items.forEach(s => {
      const days = ['일','월','화','수','목','금','토']
        .filter((_, i) => s.days & (1 << i)).join(',');
      const div = document.createElement('div');
      div.className = 'status-line';
      div.innerHTML = '#' + s.id + ' ' +
        String(s.hour).padStart(2,'0') + ':' + String(s.minute).padStart(2,'0') +
        ' [' + days + '] 🔁 재부팅' +
        (s.enabled ? ' <span style="color:#0f0">활성</span>' : ' <span style="color:#f00">비활성</span>') +
        ' <button onclick="delSchedule(' + s.id + ')" style="margin-left:8px;padding:2px 8px">삭제</button>';
      list.appendChild(div);
    });
  });
}

function addRebootSchedule() {
  const time = document.getElementById('rb-time').value.split(':');
  let days = 0;
  if (document.getElementById('rb-d-sun').checked) days |= 1;
  if (document.getElementById('rb-d-mon').checked) days |= 2;
  if (document.getElementById('rb-d-tue').checked) days |= 4;
  if (document.getElementById('rb-d-wed').checked) days |= 8;
  if (document.getElementById('rb-d-thu').checked) days |= 16;
  if (document.getElementById('rb-d-fri').checked) days |= 32;
  if (document.getElementById('rb-d-sat').checked) days |= 64;
  if (days === 0) { alert('요일을 하나 이상 선택하세요.'); return; }

  fetch('/api/schedule', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      id: 0, enabled: true,
      hour: parseInt(time[0]), minute: parseInt(time[1]),
      days: days,
      action: 'reboot',
      relay: 0
    })
  }).then(() => { loadRebootSchedules(); if (typeof loadSchedules === 'function') loadSchedules(); });
}

document.querySelector('[data-tab="admin"]').addEventListener('click', loadRebootSchedules);
