'use strict';

// ── State ─────────────────────────────────────────────────────────────────────
let selectedDevice = null;
let ws = null;
let connected = false;

// ── DOM refs ──────────────────────────────────────────────────────────────────
const connIndicator = document.getElementById('conn-indicator');
const connLabel     = document.getElementById('conn-label');
const deviceList    = document.getElementById('device-list');
const connectRow    = document.getElementById('connect-row');
const posDisplay    = document.getElementById('pos-display');
const velDisplay    = document.getElementById('vel-display');
const velSlider     = document.getElementById('vel-slider');
const eventLog      = document.getElementById('event-log');

const btnScan      = document.getElementById('btn-scan');
const btnConnect   = document.getElementById('btn-connect');
const btnDisconnect= document.getElementById('btn-disconnect');
const btnHome      = document.getElementById('btn-home');
const btnMoveAbsMm = document.getElementById('btn-move-abs-mm');
const btnSetVel    = document.getElementById('btn-set-vel');
const btnStop      = document.getElementById('btn-stop');
const btnApplyCfg  = document.getElementById('btn-apply-cfg');
const btnClearLog  = document.getElementById('btn-clear-log');

const absInputMm   = document.getElementById('abs-input-mm');
const jogButtons   = [...document.querySelectorAll('[data-rel-mm]')];

// ── API helpers ───────────────────────────────────────────────────────────────
async function api(method, path, body) {
  const opts = { method, headers: { 'Content-Type': 'application/json' } };
  if (body !== undefined) opts.body = JSON.stringify(body);
  const res = await fetch(path, opts);
  if (!res.ok) {
    const err = await res.json().catch(() => ({ detail: res.statusText }));
    throw new Error(err.detail || res.statusText);
  }
  return res.json();
}

// ── Connection state ──────────────────────────────────────────────────────────
function setConnected(isConnected, addr) {
  connected = isConnected;
  connIndicator.className = isConnected ? 'connected' : '';
  connLabel.textContent = isConnected ? `Connected · ${addr}` : 'Disconnected';
  btnConnect.disabled    = isConnected;
  btnDisconnect.disabled = !isConnected;

  const motorButtons = [btnMoveAbsMm, btnSetVel, btnStop, btnHome, btnApplyCfg, ...jogButtons];
  motorButtons.forEach(b => b.disabled = !isConnected);
  velSlider.disabled = !isConnected;

  if (!isConnected) {
    posDisplay.innerHTML = '—<span>°</span>';
    document.getElementById('pos-display-mm').innerHTML = '—<span style="font-size:16px">mm</span>';
  }
}

// ── Scan ──────────────────────────────────────────────────────────────────────
btnScan.addEventListener('click', async () => {
  btnScan.textContent = '⟳ Scanning…';
  btnScan.disabled = true;
  deviceList.innerHTML = '<li style="color:var(--text-muted);font-size:12px">Scanning 5 s…</li>';
  try {
    const devices = await api('GET', '/api/scan?duration=5');
    deviceList.innerHTML = '';
    if (!devices.length) {
      deviceList.innerHTML = '<li style="color:var(--text-muted);font-size:12px">No devices found</li>';
    } else {
      devices.forEach(d => {
        const li = document.createElement('li');
        li.innerHTML = `
          <div>
            <div class="dev-name">${d.name}</div>
            <div class="dev-addr">${d.address}</div>
          </div>
          <div class="dev-rssi">${d.rssi} dBm</div>`;
        li.addEventListener('click', () => {
          [...deviceList.querySelectorAll('li')].forEach(el => el.classList.remove('selected'));
          li.classList.add('selected');
          selectedDevice = d;
          connectRow.style.display = 'flex';
        });
        deviceList.appendChild(li);
      });
    }
  } catch (e) {
    logEvent({ type: 'error', msg: 'Scan failed: ' + e.message });
  } finally {
    btnScan.textContent = '🔍 Scan for Devices';
    btnScan.disabled = false;
  }
});

// ── Connect ───────────────────────────────────────────────────────────────────
btnConnect.addEventListener('click', async () => {
  if (!selectedDevice) return;
  btnConnect.textContent = 'Connecting…';
  btnConnect.disabled = true;
  try {
    await api('POST', '/api/connect', { address: selectedDevice.address });
    setConnected(true, selectedDevice.address);
    startWebSocket();
    loadConfig();
  } catch (e) {
    logEvent({ type: 'error', msg: 'Connect failed: ' + e.message });
    btnConnect.disabled = false;
  } finally {
    btnConnect.textContent = 'Connect';
  }
});

// ── Disconnect ────────────────────────────────────────────────────────────────
btnDisconnect.addEventListener('click', async () => {
  await api('POST', '/api/disconnect').catch(() => {});
  if (ws) { ws.close(); ws = null; }
  setConnected(false, null);
});

// ── Motion ────────────────────────────────────────────────────────────────────
btnMoveAbsMm.addEventListener('click', () => {
  api('POST', '/api/move/absolute_mm', { mm: parseFloat(absInputMm.value) })
    .catch(e => logEvent({ type: 'error', msg: e.message }));
});

jogButtons.forEach(btn => {
  btn.addEventListener('click', () => {
    const delta = parseFloat(btn.dataset.relMm);
    api('POST', '/api/move/relative_mm', { mm: delta })
      .catch(e => logEvent({ type: 'error', msg: e.message }));
  });
});

velSlider.addEventListener('input', () => {
  velDisplay.textContent = velSlider.value;
});

btnSetVel.addEventListener('click', () => {
  api('POST', '/api/velocity_mm', { mm_per_s: parseFloat(velSlider.value) })
    .catch(e => logEvent({ type: 'error', msg: e.message }));
});

btnStop.addEventListener('click', () => {
  velSlider.value = 0;
  velDisplay.textContent = '0';
  api('POST', '/api/stop').catch(e => logEvent({ type: 'error', msg: e.message }));
});

btnHome.addEventListener('click', () => {
  if (!confirm('Start homing sequence? Motor will move toward endstop.')) return;
  api('POST', '/api/home').catch(e => logEvent({ type: 'error', msg: e.message }));
});

// ── Configuration ─────────────────────────────────────────────────────────────
async function loadConfig() {
  try {
    const [cfg, belt] = await Promise.all([
      api('GET', '/api/config'),
      api('GET', '/api/belt'),
    ]);
    document.getElementById('cfg-current').value        = cfg.current_ma   ?? 800;
    document.getElementById('cfg-microsteps').value     = cfg.microsteps   ?? 16;
    document.getElementById('cfg-speed').value          = cfg.speed_sps    ?? 800;
    document.getElementById('cfg-closed-loop').value    = cfg.closed_loop  ?? 1;
    document.getElementById('cfg-dir').value            = cfg.mapping_dir  ?? 1;
    document.getElementById('cfg-endstop-mode').value   = cfg.endstop_mode ?? 'NO';
    document.getElementById('cfg-mm-per-rev').value     = belt.mm_per_rev  ?? 40;
  } catch (_) { /* device may not respond immediately */ }
}

btnApplyCfg.addEventListener('click', async () => {
  const body = {
    current_ma:       parseInt(document.getElementById('cfg-current').value),
    microsteps:       parseInt(document.getElementById('cfg-microsteps').value),
    speed_sps:        parseInt(document.getElementById('cfg-speed').value),
    closed_loop_type: parseInt(document.getElementById('cfg-closed-loop').value),
    mapping_direction:parseInt(document.getElementById('cfg-dir').value),
  };
  try {
    await api('POST', '/api/configure', body);
    const mode = document.getElementById('cfg-endstop-mode').value;
    await api('POST', '/api/endstop_mode', { normally_closed: mode === 'NC' });
    const mmPerRev = parseFloat(document.getElementById('cfg-mm-per-rev').value);
    await api('POST', '/api/belt', { mm_per_rev: mmPerRev });
    logEvent({ type: 'ack', msg: `Config applied — ${mmPerRev}mm/rev, endstop: ${mode}` });
  } catch(e) {
    logEvent({ type: 'error', msg: e.message });
  }
});

// ── WebSocket ──────────────────────────────────────────────────────────────────
function startWebSocket() {
  if (ws) ws.close();
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(`${proto}://${location.host}/ws`);

  ws.onmessage = ev => {
    const msg = JSON.parse(ev.data);
    handleMessage(msg);
  };

  ws.onclose = () => {
    // Attempt reconnect after 2 s if still supposed to be connected
    if (connected) setTimeout(startWebSocket, 2000);
  };

  ws.onerror = () => ws.close();
}

const voltageDisplay = document.getElementById('voltage-display');

const posMmDisplay = document.getElementById('pos-display-mm');

function handleMessage(msg) {
  if (msg.pos_deg !== undefined) {
    posDisplay.innerHTML = msg.pos_deg.toFixed(2) + '<span>°</span>';
  }
  if (msg.pos_mm !== undefined) {
    posMmDisplay.innerHTML = msg.pos_mm.toFixed(3) + '<span style="font-size:16px">mm</span>';
  }
  if (msg.vel_mm_s !== undefined) {
    velDisplay.textContent = msg.vel_mm_s.toFixed(2);
  }

  // Update voltage display
  if (msg.voltage_v !== undefined) {
    voltageDisplay.textContent = msg.voltage_v.toFixed(2) + ' V';
  }

  // Only log non-status events (or status on explicit request)
  if (msg.type !== 'status') {
    logEvent(msg);
  } else {
    // Silently update displays from periodic status
  }

  // Flash position display green on position_reached
  if (msg.type === 'position_reached') {
    posDisplay.style.color = 'var(--green)';
    setTimeout(() => { posDisplay.style.color = 'var(--accent)'; }, 1500);
  }

  // Flash red on stall
  if (msg.type === 'stall') {
    posDisplay.style.color = 'var(--red)';
    setTimeout(() => { posDisplay.style.color = 'var(--accent)'; }, 2000);
  }
}

// ── Event log ─────────────────────────────────────────────────────────────────
function logEvent(msg) {
  const entry = document.createElement('div');
  entry.className = 'log-entry';

  const ts   = new Date().toLocaleTimeString('en', { hour12: false });
  const type = msg.type || 'unknown';

  let detail = '';
  if (msg.pos_deg    !== undefined) detail += ` pos=${msg.pos_deg.toFixed(2)}°`;
  if (msg.target_deg !== undefined) detail += ` tgt=${msg.target_deg.toFixed(2)}°`;
  if (msg.msg        !== undefined) detail += ` ${msg.msg}`;
  if (msg.cmd        !== undefined) detail += ` cmd=${msg.cmd}`;

  entry.innerHTML = `
    <span class="log-ts">${ts}</span>
    <span class="log-type ${type}">${type}</span>
    <span class="log-msg">${detail.trim()}</span>`;

  eventLog.appendChild(entry);
  eventLog.scrollTop = eventLog.scrollHeight;

  // Cap log at 200 entries
  while (eventLog.children.length > 200) {
    eventLog.removeChild(eventLog.firstChild);
  }
}

btnClearLog.addEventListener('click', () => { eventLog.innerHTML = ''; });

// ── Init: poll API status to restore state after page reload ─────────────────
(async () => {
  try {
    const s = await api('GET', '/api/status');
    if (s.connected) {
      setConnected(true, s.address);
      startWebSocket();
      loadConfig();
    }
  } catch (_) { /* API not ready yet */ }
})();
