/**
 *  Analog Bridge — Live Dashboard WebSocket Client
 *
 *  Connects to ws://<host>/ws, receives JSON at 5Hz,
 *  updates all gauge elements and the G-force canvas.
 *
 *  Falls back to demo mode (simulated Potrero Hill → Portola Valley
 *  route data) when WebSocket connection is unavailable — e.g. when
 *  opened as a standalone file or served from a dev server.
 */

import { startDemo, stopDemo, isDemoRunning } from './demo.js';

// DOM element cache
const el = {
  wsDot:    document.getElementById('ws-dot'),
  wsStatus: document.getElementById('ws-status'),
  recBadge: document.getElementById('rec-badge'),
  gpsSats:  document.getElementById('gps-sats'),
  heap:     document.getElementById('heap'),
  // AFR
  afrCard:  document.getElementById('afr-card'),
  afrAvg:   document.getElementById('afr-avg-val'),
  afr1:     document.getElementById('afr1-val'),
  afr2:     document.getElementById('afr2-val'),
  // Engine vitals
  oilCard:  document.getElementById('oil-card'),
  oil:      document.getElementById('oil-val'),
  cltCard:  document.getElementById('clt-card'),
  clt:      document.getElementById('clt-val'),
  cltC:     document.getElementById('clt-c-val'),
  // Speed + MAP
  vss:      document.getElementById('vss-val'),
  gpsSpd:   document.getElementById('gps-spd-val'),
  map:      document.getElementById('map-val'),
  // G-force
  gCanvas:  document.getElementById('g-canvas'),
  gLat:     document.getElementById('g-lat'),
  gFwd:     document.getElementById('g-fwd'),
  // IMU detail
  ax: document.getElementById('imu-ax'), ay: document.getElementById('imu-ay'), az: document.getElementById('imu-az'),
  gx: document.getElementById('imu-gx'), gy: document.getElementById('imu-gy'), gz: document.getElementById('imu-gz'),
  mx: document.getElementById('imu-mx'), my: document.getElementById('imu-my'), mz: document.getElementById('imu-mz'),
  tmp: document.getElementById('imu-tmp'),
  // Recording
  recInfo:  document.getElementById('rec-info'),
  recFile:  document.getElementById('rec-file'),
  recRows:  document.getElementById('rec-rows'),
  recDur:   document.getElementById('rec-dur'),
  recKf:    document.getElementById('rec-kf'),
};

const gCtx = el.gCanvas.getContext('2d');

//----------------------------------------------------------------
// AFR color coding
//----------------------------------------------------------------
function afrClass(val) {
  if (val <= 0) return 'afr-stoich';
  if (val < 12.0) return 'afr-rich';
  if (val <= 13.5) return 'afr-target';
  if (val <= 15.0) return 'afr-stoich';
  return 'afr-lean';
}

//----------------------------------------------------------------
// Coolant temp color coding
//----------------------------------------------------------------
function cltClass(val) {
  if (val >= 210) return 'clt-hot';
  if (val >= 200) return 'clt-warm';
  if (val >= 180) return 'clt-normal';
  return 'clt-cold';
}

//----------------------------------------------------------------
// G-force canvas drawing
//----------------------------------------------------------------
function drawGForce(ax, ay) {
  const w = el.gCanvas.width;
  const h = el.gCanvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const scale = w / 4; // 2g full scale

  gCtx.clearRect(0, 0, w, h);

  // Background grid
  gCtx.strokeStyle = '#1e293b';
  gCtx.lineWidth = 1;

  // Crosshair
  gCtx.beginPath();
  gCtx.moveTo(cx, 0); gCtx.lineTo(cx, h);
  gCtx.moveTo(0, cy); gCtx.lineTo(w, cy);
  gCtx.stroke();

  // 1g circle
  gCtx.beginPath();
  gCtx.arc(cx, cy, scale, 0, Math.PI * 2);
  gCtx.stroke();

  // G dot (lateral = right on screen, forward = up on screen)
  const dotX = cx + ay * scale;  // ay = lateral (right positive)
  const dotY = cy - ax * scale;  // ax = forward (up positive)

  // Trail shadow
  gCtx.fillStyle = 'rgba(79, 195, 247, 0.15)';
  gCtx.beginPath();
  gCtx.arc(dotX, dotY, 8, 0, Math.PI * 2);
  gCtx.fill();

  // Dot
  gCtx.fillStyle = '#4fc3f7';
  gCtx.beginPath();
  gCtx.arc(dotX, dotY, 4, 0, Math.PI * 2);
  gCtx.fill();
}

//----------------------------------------------------------------
// Update UI from JSON frame
//----------------------------------------------------------------
function update(d) {
  // AFR — combined average is the big number, banks are smaller
  const afr1 = d.eng.afr;
  const afr2 = d.eng.afr1;
  const hasAfr1 = afr1 > 0;
  const hasAfr2 = afr2 > 0;
  let afrAvg = 0;
  if (hasAfr1 && hasAfr2) afrAvg = (afr1 + afr2) / 2;
  else if (hasAfr1) afrAvg = afr1;
  else if (hasAfr2) afrAvg = afr2;

  const afrNoData = afrAvg <= 0;
  el.afrAvg.textContent = afrNoData ? '--' : afrAvg.toFixed(1);
  el.afrAvg.className = 'gauge-value ' + (afrNoData ? 'warn-oil' : afrClass(afrAvg));
  el.afrCard.classList.toggle('alarm-active', afrNoData);
  el.afr1.textContent = hasAfr1 ? afr1.toFixed(1) : '--';
  el.afr2.textContent = hasAfr2 ? afr2.toFixed(1) : '--';

  // Oil pressure — alarm if under 5 psi (including 0 = engine off)
  const oilVal = d.eng.oil;
  el.oil.textContent = Math.round(oilVal);
  const oilAlarm = oilVal < 5;
  el.oil.className = 'gauge-value' + (oilAlarm ? ' warn-oil' : '');
  el.oilCard.classList.toggle('alarm-active', oilAlarm);

  // Coolant — color gradient + alarm if over 210 F
  const cltVal = d.eng.clt;
  el.clt.textContent = cltVal > 0 ? Math.round(cltVal) : '--';
  const cltAlarm = cltVal >= 210;
  el.clt.className = 'gauge-value ' + (cltVal > 0 ? cltClass(cltVal) : 'clt-cold');
  el.cltC.textContent = cltVal > 0 ? Math.round((cltVal - 32) * 5 / 9) : '--';
  el.cltCard.classList.toggle('alarm-active', cltAlarm);

  // Speed — GPS is primary (big), VSS is secondary (small)
  el.gpsSpd.textContent = d.gps.spd > 0 ? d.gps.spd.toFixed(0) : '0';
  el.vss.textContent = d.eng.vss > 0 ? d.eng.vss.toFixed(0) : '0';
  el.map.textContent = d.eng.map !== undefined ? d.eng.map.toFixed(1) : '--';

  // GPS
  el.gpsSats.textContent = d.gps.sat + ' sat' + (d.gps.stale ? '!' : '');
  el.gpsSats.className = d.gps.stale ? 'text-red-400' : 'text-slate-500';

  // G-force
  drawGForce(d.imu.ax, d.imu.ay);
  el.gLat.textContent = d.imu.ay.toFixed(2);
  el.gFwd.textContent = d.imu.ax.toFixed(2);

  // IMU detail
  el.ax.textContent = d.imu.ax.toFixed(2);
  el.ay.textContent = d.imu.ay.toFixed(2);
  el.az.textContent = d.imu.az.toFixed(2);
  el.gx.textContent = d.imu.gx.toFixed(1);
  el.gy.textContent = d.imu.gy.toFixed(1);
  el.gz.textContent = d.imu.gz.toFixed(1);
  el.mx.textContent = d.imu.mx.toFixed(0);
  el.my.textContent = d.imu.my.toFixed(0);
  el.mz.textContent = d.imu.mz.toFixed(0);
  el.tmp.textContent = d.imu.tmp.toFixed(1);

  // Recording
  if (d.rec.on) {
    el.recBadge.classList.remove('hidden');
    el.recInfo.classList.remove('hidden');
    el.recFile.textContent = d.rec.file;
    el.recRows.textContent = d.rec.rows;
    el.recDur.textContent = d.rec.dur.toFixed(1);
    el.recKf.textContent = d.rec.kf;
  } else {
    el.recBadge.classList.add('hidden');
    el.recInfo.classList.add('hidden');
  }
}

//----------------------------------------------------------------
// Serial command buttons — send single-char over WebSocket
//----------------------------------------------------------------
document.querySelectorAll('.cmd-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    const cmd = btn.dataset.cmd;
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(cmd);
      // Brief visual feedback
      btn.classList.add('!bg-sky-900');
      setTimeout(() => btn.classList.remove('!bg-sky-900'), 200);
    } else {
      // Flash red if not connected
      btn.classList.add('!bg-red-900');
      setTimeout(() => btn.classList.remove('!bg-red-900'), 300);
    }
  });
});

//----------------------------------------------------------------
// WebSocket connection with auto-reconnect + demo fallback
//----------------------------------------------------------------
let ws = null;
let reconnectTimer = null;
let failCount = 0;
const DEMO_AFTER_FAILS = 2; // Enter demo after 2 failed WS attempts (~4s)

function enterDemoMode() {
  el.wsDot.className = 'w-2 h-2 rounded-full bg-amber-500 transition-colors';
  el.wsStatus.textContent = 'Demo Mode';
  startDemo(update);
}

function connect() {
  // file:// can't do WebSocket — go straight to demo
  if (location.protocol === 'file:') {
    enterDemoMode();
    return;
  }

  const url = `ws://${location.host}/ws`;
  ws = new WebSocket(url);

  ws.onopen = () => {
    failCount = 0;
    // If demo was running, stop it — live data takes over
    if (isDemoRunning()) stopDemo();

    el.wsDot.className = 'w-2 h-2 rounded-full bg-green-500 transition-colors';
    el.wsStatus.textContent = 'Connected';
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
  };

  ws.onclose = () => {
    failCount++;
    el.wsDot.className = 'w-2 h-2 rounded-full bg-red-500 transition-colors';
    el.wsStatus.textContent = 'Disconnected';

    if (failCount >= DEMO_AFTER_FAILS && !isDemoRunning()) {
      enterDemoMode();
    } else if (!isDemoRunning()) {
      reconnectTimer = setTimeout(connect, 2000);
    }
  };

  ws.onerror = () => {
    ws.close();
  };

  ws.onmessage = (event) => {
    try {
      const d = JSON.parse(event.data);
      update(d);
    } catch (e) {
      // Ignore parse errors
    }
  };
}

connect();
