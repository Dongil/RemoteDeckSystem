// Tab navigation
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
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
  const dot = document.getElementById('pc-dot');
  const txt = document.getElementById('pc-text');
  dot.className = 'dot ' + (d.pc_on ? 'on' : 'off');
  txt.textContent = 'PC: ' + (d.pc_on ? 'ON' : 'OFF');

  document.getElementById('r1').textContent = d.relay1 ? 'ON' : 'OFF';
  document.getElementById('r2').textContent = d.relay2 ? 'ON' : 'OFF';
  if (d.gpio) document.getElementById('gpio-val').textContent = d.gpio.join(', ');
  document.getElementById('ip-addr').textContent = d.ip || '--';
  if (d.uptime !== undefined) {
    const h = Math.floor(d.uptime / 3600);
    const m = Math.floor((d.uptime % 3600) / 60);
    document.getElementById('uptime').textContent = h + '시간 ' + m + '분';
  }
  document.getElementById('mqtt-st').textContent = d.mqtt_connected ? '연결됨' : '연결 안됨';
  document.getElementById('ntp-time').textContent = d.time || '--';
  document.getElementById('fw-ver').textContent = 'v' + (d.fw_ver || '--');
  document.getElementById('ota-ver').textContent = d.fw_ver || '--';
  if (d.device_name) {
    document.getElementById('dev-name').textContent = d.device_name;
    document.getElementById('device-name-header').textContent = d.device_name;
  }
  if (d.ip) document.getElementById('dev-id').textContent = d.ip;
}

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
  });
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
      const actionMap = {on:'켜기', off:'끄기', toggle:'토글'};
      const div = document.createElement('div');
      div.className = 'status-line';
      div.innerHTML = '#' + s.id + ' ' +
        String(s.hour).padStart(2,'0') + ':' + String(s.minute).padStart(2,'0') +
        ' [' + days + '] 릴레이' + s.relay + ' ' + (actionMap[s.action] || s.action) +
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
  fetch('/api/schedule?id=' + id, {method: 'DELETE'}).then(() => loadSchedules());
}

// Config
function loadConfig() {
  fetch('/api/config').then(r => r.json()).then(d => {
    const form = document.getElementById('config-form');
    form.innerHTML = '';
    function addField(label, key, val) {
      const html = '<div class="cfg-group"><label>' + label + '</label>' +
        '<input type="text" data-key="' + key + '" value="' + (val || '') + '"></div>';
      form.innerHTML += html;
    }
    addField('장치 ID', 'device_id', d.device_id);
    addField('네트워크 모드', 'network.mode', d.network?.mode);
    addField('이더넷 IP', 'network.ethernet.ip', d.network?.ethernet?.ip);
    addField('게이트웨이', 'network.ethernet.gateway', d.network?.ethernet?.gateway);
    addField('서브넷 마스크', 'network.ethernet.subnet', d.network?.ethernet?.subnet);
    addField('DNS', 'network.ethernet.dns1', d.network?.ethernet?.dns1);
    addField('WiFi SSID', 'network.wifi.ssid', d.network?.wifi?.ssid);
    addField('MQTT 브로커', 'mqtt.broker', d.mqtt?.broker);
    addField('MQTT 포트', 'mqtt.port', d.mqtt?.port);
    addField('MQTT 사용자', 'mqtt.user', d.mqtt?.user);
    addField('짧은 펄스 (ms)', 'relay.pulse_short_ms', d.relay?.pulse_short_ms);
    addField('긴 펄스 (ms)', 'relay.pulse_long_ms', d.relay?.pulse_long_ms);
    addField('폴링 주기 (ms)', 'monitor.pcled_poll_interval_ms', d.monitor?.pcled_poll_interval_ms);
    addField('WOL 대상 MAC', 'wol_target_mac', d.wol_target_mac);
    addField('NTP 서버', 'ntp.server', d.ntp?.server);
    addField('시간대', 'ntp.timezone', d.ntp?.timezone);
  });
}

function saveConfig() {
  if (!confirm('설정을 저장하고 장치를 재부팅하시겠습니까?')) return;
  const inputs = document.querySelectorAll('#config-form input');
  const cfg = {};
  inputs.forEach(inp => {
    const keys = inp.dataset.key.split('.');
    let obj = cfg;
    for (let i = 0; i < keys.length - 1; i++) {
      if (!obj[keys[i]]) obj[keys[i]] = {};
      obj = obj[keys[i]];
    }
    let val = inp.value;
    if (!isNaN(val) && val !== '') val = Number(val);
    obj[keys[keys.length - 1]] = val;
  });

  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(cfg)
  }).then(() => {
    alert('저장 완료! 장치가 재부팅됩니다.');
    setTimeout(() => location.reload(), 5000);
  });
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

// OTA Upload
function uploadOTA() {
  const file = document.getElementById('ota-file').files[0];
  if (!file) return alert('.bin 파일을 선택하세요');
  if (!confirm('펌웨어를 업로드하고 재부팅하시겠습니까?')) return;

  const formData = new FormData();
  formData.append('firmware', file);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/ota');
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded / e.total * 100);
      document.getElementById('ota-progress').style.width = pct + '%';
      document.getElementById('ota-pct').textContent = pct + '%';
    }
  };
  xhr.onload = () => {
    alert('업로드 완료! 장치가 재부팅됩니다...');
    setTimeout(() => location.reload(), 10000);
  };
  xhr.send(formData);
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
