/**
 *  Analog Bridge — Demo Mode Replay
 *
 *  Parses the embedded demo CSV (Potrero Hill → Portola Valley) and
 *  replays it through the same update() function used by the live
 *  WebSocket feed. Activates automatically when WS connection fails
 *  or when the page is opened via file://.
 *
 *  Replay rate: 200ms (5Hz) matching WS_BROADCAST_MS.
 *  CSV sample rate: 80ms (12.5Hz), so we advance ~2 rows per tick.
 */

import { DEMO_CSV } from './demo-data.js';

let demoRows = null;
let demoIdx = 0;
let demoTimer = null;
let maxKf = 0;

const REPLAY_MS = 200;    // 5Hz, matches WebSocket broadcast rate
const CSV_MS = 80;         // 12.5Hz, sample rate in the CSV
const SKIP = Math.round(REPLAY_MS / CSV_MS); // ~2-3 rows per tick

/**
 * Parse the CSV string into an array of WebSocket-format JSON objects.
 * Column order must match sd_logger.cpp:
 *   time,lat,lon,speed,alt,dir,sats,accx,accy,accz,rotx,roty,rotz,
 *   magx,magy,magz,imuTemp,afr,afr1,vss,map,oilp,coolant,gpsStale,keyframe
 */
function parseDemoData() {
  const lines = DEMO_CSV.trim().split('\n');
  const rows = [];

  // Skip header (line 0) and units (line 1)
  for (let i = 2; i < lines.length; i++) {
    const c = lines[i].split(',');
    if (c.length < 25) continue;

    rows.push({
      t:   parseFloat(c[0]),
      gps: {
        lat:   parseFloat(c[1]),
        lon:   parseFloat(c[2]),
        spd:   parseFloat(c[3]),
        alt:   parseFloat(c[4]),
        dir:   parseFloat(c[5]),
        sat:   parseInt(c[6], 10),
        stale: c[23] === '1',
      },
      imu: {
        ax:  parseFloat(c[7]),
        ay:  parseFloat(c[8]),
        az:  parseFloat(c[9]),
        gx:  parseFloat(c[10]),
        gy:  parseFloat(c[11]),
        gz:  parseFloat(c[12]),
        mx:  parseFloat(c[13]),
        my:  parseFloat(c[14]),
        mz:  parseFloat(c[15]),
        tmp: parseFloat(c[16]),
      },
      eng: {
        afr:  parseFloat(c[17]),
        afr1: parseFloat(c[18]),
        vss:  parseFloat(c[19]),
        map:  parseFloat(c[20]),
        oil:  parseFloat(c[21]),
        clt:  parseFloat(c[22]),
      },
      rec: {
        on:   true,
        file: 'potrero_280_portola_demo.csv',
        rows: i - 1,
        dur:  parseFloat(c[0]),
        kf:   parseInt(c[24], 10) || 0,
      },
    });
  }

  return rows;
}

/**
 * Start demo replay, calling updateFn with each frame at 5Hz.
 * Loops indefinitely until stopDemo() is called.
 */
export function startDemo(updateFn) {
  if (!demoRows) demoRows = parseDemoData();
  demoIdx = 0;
  maxKf = 0;

  demoTimer = setInterval(() => {
    if (demoIdx >= demoRows.length) {
      // Loop — brief visual reset
      demoIdx = 0;
      maxKf = 0;
    }

    const row = demoRows[demoIdx];

    // Track cumulative keyframe count (rec.kf in WS is running total)
    if (row.rec.kf > 0 && row.rec.kf > maxKf) maxKf = row.rec.kf;
    row.rec.kf = maxKf;
    row.rec.rows = demoIdx;

    updateFn(row);
    demoIdx += SKIP;
  }, REPLAY_MS);
}

/**
 * Stop demo replay.
 */
export function stopDemo() {
  if (demoTimer) {
    clearInterval(demoTimer);
    demoTimer = null;
  }
}

/**
 * Check if demo is currently running.
 */
export function isDemoRunning() {
  return demoTimer !== null;
}
