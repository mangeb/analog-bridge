#!/usr/bin/env node
/**
 *  Analog Bridge — Demo CSV Generator
 *
 *  Generates a deterministic demo CSV from the Potrero Hill → 280 → Portola
 *  Valley route. Uses the same waypoints and scenarios as the log analyzer
 *  (tools/log-analyzer.html loadSyntheticData), but with a seeded PRNG so
 *  output is identical across runs (clean git diffs).
 *
 *  Outputs:
 *    csv/potrero_280_portola_demo.csv       — standalone CSV for log analyzer
 *    firmware/web-ui/src/demo-data.js       — ES module wrapping CSV string
 *
 *  Usage:  node scripts/generate-demo-csv.js
 */

const fs = require('fs');
const path = require('path');

const SAMPLE_MS = 80; // 12.5 Hz, same as firmware

// ── Seeded PRNG (Mulberry32) ──────────────────────────────────────────
let _seed = 42;
function srand(s) { _seed = s; }
function rand() {
  _seed |= 0;
  _seed = (_seed + 0x6D2B79F5) | 0;
  let t = Math.imul(_seed ^ (_seed >>> 15), 1 | _seed);
  t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}
function rnd(lo, hi) { return lo + rand() * (hi - lo); }
function lerp(a, b, f) { return a + (b - a) * f; }

// ── GPS Waypoints: Potrero Hill → 280 → Portola Valley ───────────────
const waypoints = [
  { lat: 37.7598, lon: -122.3927 }, // Potrero Hill - 18th & Wisconsin
  { lat: 37.7558, lon: -122.3945 }, // Heading south on Wisconsin
  { lat: 37.7510, lon: -122.3988 }, // Down the hill to Cesar Chavez
  { lat: 37.7465, lon: -122.4035 }, // Cesar Chavez west
  { lat: 37.7380, lon: -122.4025 }, // 101 on-ramp area
  { lat: 37.7245, lon: -122.4050 }, // 101 south
  { lat: 37.7100, lon: -122.4120 }, // 101/280 junction
  { lat: 37.6950, lon: -122.4280 }, // 280 south past Daly City
  { lat: 37.6700, lon: -122.4420 }, // 280 Pacifica area
  { lat: 37.6400, lon: -122.4450 }, // 280 San Bruno Mtn
  { lat: 37.6100, lon: -122.4380 }, // 280 south
  { lat: 37.5800, lon: -122.4200 }, // 280 Crystal Springs
  { lat: 37.5500, lon: -122.3950 }, // 280 Woodside
  { lat: 37.5350, lon: -122.3800 }, // Sand Hill Rd exit
  { lat: 37.5200, lon: -122.3650 }, // Alpine Rd
  { lat: 37.5050, lon: -122.3580 }, // Portola Rd
  { lat: 37.4900, lon: -122.3500 }, // Portola Valley center
];

// ── Scenario segments ─────────────────────────────────────────────────
// Extended from tools/log-analyzer.html loadSyntheticData() scenarios
// Added engine-off cold start (5s) and coolant temp spike (at ~10s)
const scenarios = [
  // Engine OFF — key on, no RPM, no oil pressure, cold coolant
  { dur: 5, spd: 0, map: 0, afr: 0, accx: 0, accy: 0, wp: [0,0], engineOff: true },
  // Engine just started — idle, oil building, coolant spike at ~10s mark
  { dur: 3, spd: 0, map: 20, afr: 13.8, accx: 0, accy: 0, wp: [0,0], tempSpike: true },
  // Potrero Hill — idle at top, temps settling
  { dur: 6, spd: 0, map: 20, afr: 13.8, accx: 0, accy: 0, wp: [0,0], kf: true },
  // Pull out, gentle accel down Wisconsin
  { dur: 4, spd: [0, 25], map: [20, 10], afr: 12.8, accx: [0.1, 0.2], accy: 0, wp: [0,1] },
  // Steep downhill Potrero — braking, turns
  { dur: 6, spd: [25, 30], map: [16, 18], afr: 14.5, accx: [-0.3, -0.15], accy: [-0.2, 0.2], wp: [1,2] },
  // Stop sign at Cesar Chavez
  { dur: 3, spd: [30, 0], map: [18, 20], afr: 15.0, accx: [-0.4, -0.1], accy: 0, wp: [2,2] },
  { dur: 4, spd: 0, map: 20, afr: 13.7, accx: 0, accy: 0, wp: [2,2] },
  // Light — pull out with authority, BURNOUT!
  { dur: 3, spd: [0, 15], map: [20, 2], afr: 11.8, accx: [0.4, 0.6], accy: [-0.05, 0.05], wp: [2,3], slip: 20 },
  // Continue accel to 101 on-ramp
  { dur: 5, spd: [15, 45], map: [4, 6], afr: 12.4, accx: [0.2, 0.35], accy: [-0.1, 0.1], wp: [3,4], kf: true },
  // 101 merge — hard accel
  { dur: 5, spd: [45, 70], map: [6, 2], afr: 12.2, accx: [0.3, 0.5], accy: [0, 0.15], wp: [4,5] },
  // 101 cruise
  { dur: 8, spd: 65, map: 10, afr: 13.7, accx: 0, accy: [-0.05, 0.05], wp: [5,6] },
  // 101/280 junction — slight decel and curve
  { dur: 4, spd: [65, 55], map: [10, 15], afr: 14.2, accx: [-0.1, 0], accy: [-0.25, -0.1], wp: [6,7] },
  // 280 south — open throttle, WOT run!
  { dur: 4, spd: [55, 85], map: [15, 1.5], afr: 12.3, accx: [0.3, 0.55], accy: [0, 0.1], wp: [7,8], kf: true },
  // 280 WOT continued
  { dur: 5, spd: [85, 95], map: 1.5, afr: 12.5, accx: [0.15, 0.3], accy: [-0.05, 0.05], wp: [8,9] },
  // 280 high speed cruise
  { dur: 10, spd: 75, map: 8, afr: 13.5, accx: [-0.02, 0.02], accy: [-0.08, 0.08], wp: [9,11] },
  // 280 sweeping curves — Crystal Springs
  { dur: 8, spd: 70, map: 9, afr: 13.6, accx: [-0.05, 0.05], accy: [-0.35, 0.35], wp: [11,12] },
  // Hard braking for Sand Hill exit — tires skid briefly
  { dur: 3, spd: [70, 35], map: [9, 20], afr: 15.5, accx: [-0.6, -0.35], accy: [0, 0.1], wp: [12,13], kf: true, slip: -8 },
  // Exit ramp decel
  { dur: 4, spd: [35, 15], map: [20, 18], afr: 14.8, accx: [-0.2, -0.1], accy: [-0.15, 0.15], wp: [13,14] },
  // Alpine Rd — moderate cruise through trees
  { dur: 8, spd: 35, map: 14, afr: 14.3, accx: [-0.05, 0.05], accy: [-0.2, 0.2], wp: [14,15] },
  // Portola Rd — light cruise, rolling hills
  { dur: 6, spd: 30, map: 16, afr: 14.5, accx: [-0.03, 0.08], accy: [-0.12, 0.12], wp: [15,16] },
  // Pull over, idle in Portola Valley
  { dur: 3, spd: [30, 0], map: [16, 20], afr: 15.0, accx: [-0.3, -0.1], accy: 0, wp: [16,16] },
  { dur: 5, spd: 0, map: 20, afr: 13.8, accx: 0, accy: 0, wp: [16,16], kf: true },
];

// ── Generate data rows ────────────────────────────────────────────────
function generate() {
  srand(42); // deterministic seed
  const rows = [];
  let t = 0;
  let kfNum = 1;

  for (const sc of scenarios) {
    const n = Math.round(sc.dur * 1000 / SAMPLE_MS);
    const wpStart = waypoints[sc.wp[0]];
    const wpEnd = waypoints[sc.wp[1]];
    const hasKf = sc.kf && n > 2;
    const kfAt = hasKf ? Math.floor(n / 2) : -1;

    for (let i = 0; i < n; i++) {
      const frac = i / Math.max(1, n - 1);
      const s = Array.isArray(sc.spd) ? lerp(sc.spd[0], sc.spd[1], frac) : sc.spd;
      const m = (Array.isArray(sc.map) ? lerp(sc.map[0], sc.map[1], frac) : sc.map) + rnd(-0.3, 0.3);
      const afrBase = sc.afr + rnd(-0.2, 0.2);

      // GPS: interpolate between waypoints with slight wander
      const lat = lerp(wpStart.lat, wpEnd.lat, frac) + rnd(-0.0001, 0.0001);
      const lon = lerp(wpStart.lon, wpEnd.lon, frac) + rnd(-0.0001, 0.0001);
      const dir = Math.atan2(wpEnd.lon - wpStart.lon, wpEnd.lat - wpStart.lat) * 180 / Math.PI + rnd(-3, 3);

      // Acceleration
      const ax = Array.isArray(sc.accx) ? lerp(sc.accx[0], sc.accx[1], frac) + rnd(-0.03, 0.03) : rnd(-0.03, 0.03);
      const ay = Array.isArray(sc.accy) ? lerp(sc.accy[0], sc.accy[1], frac) + rnd(-0.02, 0.02) : rnd(-0.03, 0.03);

      // VSS: normally matches GPS speed, but slip scenarios diverge
      let vss = s + rnd(-1, 1);
      if (sc.slip) {
        const slipPeak = Math.abs(sc.slip);
        const slipFrac = Math.sin(frac * Math.PI);
        vss = s + Math.sign(sc.slip) * slipPeak * slipFrac + rnd(-1, 1);
      }

      // Engine-off scenario: zero AFR, zero oil, cold coolant
      let rowAfr, rowAfr1, rowOilp, rowCoolant, rowMap;
      if (sc.engineOff) {
        rowAfr = 0;
        rowAfr1 = 0;
        rowOilp = 0;
        rowCoolant = 120 + rnd(0, 3);  // ambient-ish, engine cold
        rowMap = 0;
      } else if (sc.tempSpike) {
        // Just started — oil building up, coolant spikes then settles
        const spikeFrac = Math.sin(frac * Math.PI); // peaks mid-segment
        rowAfr = afrBase + rnd(-0.1, 0.1);
        rowAfr1 = afrBase + 0.15 + rnd(-0.1, 0.1);
        rowOilp = lerp(3, 45, frac) + rnd(-2, 2);   // oil building from low
        rowCoolant = 160 + spikeFrac * 65 + rnd(0, 3); // spikes to ~225 F
        rowMap = Math.max(0, m);
      } else {
        rowAfr = afrBase + rnd(-0.1, 0.1);
        rowAfr1 = afrBase + 0.15 + rnd(-0.1, 0.1);
        rowOilp = 55 + s * 0.08 + rnd(-2, 2);
        rowCoolant = 185 + rnd(0, 5);
        rowMap = Math.max(0, m);
      }

      rows.push({
        time: t,
        lat,
        lon,
        speed: s,
        alt: lerp(90, 120, frac) + rnd(-2, 2),
        dir: ((dir % 360) + 360) % 360,
        sats: 10 + Math.floor(rnd(0, 4)),
        accx: ax,
        accy: ay,
        accz: -0.98 + rnd(-0.02, 0.02),
        rotx: rnd(-3, 3),
        roty: rnd(-3, 3),
        rotz: rnd(-1, 1),
        magx: 20 + rnd(0, 5),
        magy: -10 + rnd(0, 3),
        magz: 40 + rnd(0, 5),
        imuTemp: 28 + rnd(0, 2),
        afr: rowAfr,
        afr1: rowAfr1,
        vss: Math.max(0, vss),
        map: rowMap,
        oilp: rowOilp,
        coolant: rowCoolant,
        gpsStale: 0,
        keyframe: (i === kfAt) ? kfNum : 0,
      });

      if (i === kfAt) kfNum++;
      t += SAMPLE_MS / 1000;
    }
  }

  return rows;
}

// ── Format a row as CSV ───────────────────────────────────────────────
function rowToCSV(r) {
  return [
    r.time.toFixed(3),
    r.lat.toFixed(7),
    r.lon.toFixed(7),
    r.speed.toFixed(1),
    r.alt.toFixed(1),
    r.dir.toFixed(1),
    r.sats,
    r.accx.toFixed(3),
    r.accy.toFixed(3),
    r.accz.toFixed(3),
    r.rotx.toFixed(2),
    r.roty.toFixed(2),
    r.rotz.toFixed(2),
    r.magx.toFixed(1),
    r.magy.toFixed(1),
    r.magz.toFixed(1),
    r.imuTemp.toFixed(1),
    r.afr.toFixed(1),
    r.afr1.toFixed(1),
    r.vss.toFixed(1),
    r.map.toFixed(1),
    r.oilp.toFixed(1),
    r.coolant.toFixed(1),
    r.gpsStale,
    r.keyframe,
  ].join(',');
}

// ── Main ──────────────────────────────────────────────────────────────
const rows = generate();
const header = 'time,lat,lon,speed,alt,dir,sats,accx,accy,accz,rotx,roty,rotz,magx,magy,magz,imuTemp,afr,afr1,vss,map,oilp,coolant,gpsStale,keyframe';
const units  = '(s),(deg),(deg),(mph),(ft),(deg),(#),(g),(g),(g),(dps),(dps),(dps),(uT),(uT),(uT),(C),(afr),(afr),(mph),(inHgVac),(psig),(F),(flag),(#)';
const csvLines = [header, units, ...rows.map(rowToCSV)];
const csvContent = csvLines.join('\n') + '\n';

const dur = rows[rows.length - 1].time;
console.log(`Generated ${rows.length} rows, ${dur.toFixed(1)}s duration, 12.5Hz`);

// Resolve paths relative to this script (scripts/ → project root)
const root = path.resolve(__dirname, '..');
const csvPath = path.join(root, 'csv', 'potrero_280_portola_demo.csv');
const jsPath  = path.join(root, 'firmware', 'web-ui', 'src', 'demo-data.js');

// Ensure directories exist
fs.mkdirSync(path.dirname(csvPath), { recursive: true });
fs.mkdirSync(path.dirname(jsPath),  { recursive: true });

// Write CSV
fs.writeFileSync(csvPath, csvContent);
console.log(`CSV:  ${csvPath} (${(csvContent.length / 1024).toFixed(1)} KB)`);

// Write JS module
const jsModule = [
  '// Auto-generated by scripts/generate-demo-csv.js — DO NOT EDIT',
  `// Route: Potrero Hill \u2192 101S \u2192 280S \u2192 Portola Valley`,
  `// Rows: ${rows.length}, Duration: ${dur.toFixed(1)}s, Sample rate: 12.5Hz`,
  '',
  'export const DEMO_CSV = `' + csvContent.replace(/`/g, '\\`').replace(/\$/g, '\\$') + '`;',
  '',
].join('\n');
fs.writeFileSync(jsPath, jsModule);
console.log(`JS:   ${jsPath} (${(jsModule.length / 1024).toFixed(1)} KB)`);
