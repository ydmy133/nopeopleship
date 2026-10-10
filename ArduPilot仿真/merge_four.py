#!/usr/bin/env python3
"""把「传感全准」与「真实数据」各两套环境合成一页 2x2 对照。

用法:
    python sim/merge_four.py
"""

from __future__ import annotations

import json
import re
from pathlib import Path

_HERE = Path(__file__).resolve().parent
OUT = _HERE / "usv_sim_four.html"

PANELS = [
    {
        "id": "perfect_ideal",
        "path": _HERE / "传感器全准的仿真" / "usv_sim_perfect_ideal.html",
        "href": "传感器全准的仿真/usv_sim_perfect_ideal.html",
        "sense": "传感全准",
        "env": "无风无流",
        "short": "全准静",
        "title": "传感全准 · 无风无流",
    },
    {
        "id": "perfect_wind",
        "path": _HERE / "传感器全准的仿真" / "usv_sim_perfect_current_wind.html",
        "href": "传感器全准的仿真/usv_sim_perfect_current_wind.html",
        "sense": "传感全准",
        "env": "有风有流",
        "short": "全准风",
        "title": "传感全准 · 有风有流",
    },
    {
        "id": "real_ideal",
        "path": _HERE / "真实数据" / "usv_sim_ideal.html",
        "href": "真实数据/usv_sim_ideal.html",
        "sense": "湖测残差",
        "env": "无风无流",
        "short": "残差静",
        "title": "湖测残差 · 无风无流",
    },
    {
        "id": "real_wind",
        "path": _HERE / "真实数据" / "usv_sim_current_wind.html",
        "href": "真实数据/usv_sim_current_wind.html",
        "sense": "湖测残差",
        "env": "有风有流",
        "short": "残差风",
        "title": "湖测残差 · 有风有流",
    },
]

SCENARIO_ORDER = [
    "navigate",
    "fixed_point",
    "cruise_speed",
    "cruise_dir",
    "fixed_point_decay",
]


def extract_data(html_path: Path) -> list:
    text = html_path.read_text(encoding="utf-8")
    m = re.search(r"const DATA = (\[.*?\]);\s*\nlet ", text, re.S)
    if not m:
        m = re.search(r"const DATA = (\[.*?\]);", text, re.S)
    if not m:
        raise RuntimeError("找不到 DATA: %s" % html_path)
    return json.loads(m.group(1))


HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>ArduPilot 无人船四仿真对照</title>
<style>
  :root { --bg:#f4f1ea; --ink:#1c2430; --muted:#5c6570; --line:#d7d1c4; --water:#d9e4ea; --panel:#fffdf8; }
  * { box-sizing: border-box; }
  body { margin:0; font:13px/1.45 "Segoe UI","Microsoft YaHei",sans-serif; color:var(--ink); background:var(--bg); }
  header { padding:16px 20px 6px; }
  h1 { font-size:20px; font-weight:650; margin:0 0 6px; }
  .sub { color:var(--muted); max-width:980px; }
  .links { margin:8px 0 0; font-size:12px; }
  .links a { color:#1f4b73; margin-right:12px; }
  .tabs, .legend, .controls { display:flex; gap:8px; padding:0 20px; flex-wrap:wrap; align-items:center; }
  .legend { padding-top:8px; color:var(--muted); }
  .controls { padding:8px 20px; }
  .tab { border:1px solid var(--line); background:transparent; padding:7px 11px; cursor:pointer; color:var(--ink); }
  .tab.active { background:var(--ink); color:#fff; border-color:var(--ink); }
  .tab .ok { margin-left:6px; font-size:11px; }
  button, input[type=range] { accent-color:var(--ink); }
  button { background:#fff; border:1px solid var(--line); padding:6px 12px; cursor:pointer; }
  .sw { display:inline-block; width:18px; height:3px; margin-right:6px; vertical-align:middle; }
  .page { display:grid; grid-template-columns: 1fr 340px; gap:14px; padding:12px 20px 24px; }
  .grid { display:grid; grid-template-columns: 1fr 1fr; gap:10px; }
  .cell { background:var(--panel); border:1px solid var(--line); padding:8px; cursor:pointer; }
  .cell.active { outline:2px solid var(--ink); }
  .cell h3 { margin:0 0 6px; font-size:13px; display:flex; justify-content:space-between; gap:8px; }
  .cell canvas.map { width:100%; height:260px; background:var(--water); border:1px solid var(--line); display:block; }
  .cell canvas.dist { width:100%; height:88px; margin-top:6px; background:#fffdf8; border:1px solid var(--line); display:block; }
  .mini { display:flex; gap:10px; flex-wrap:wrap; margin-top:6px; color:var(--muted); font-variant-numeric: tabular-nums; font-size:12px; }
  .empty { height:354px; display:flex; align-items:center; justify-content:center; color:var(--muted); background:var(--water); border:1px solid var(--line); }
  aside { background:var(--panel); border:1px solid var(--line); padding:12px 14px; max-height:calc(100vh - 180px); overflow:auto; }
  aside h2 { font-size:13px; margin:12px 0 6px; }
  aside h2:first-child { margin-top:0; }
  .metric { display:grid; grid-template-columns: 1.15fr repeat(4, 0.7fr); gap:4px; padding:3px 0; border-bottom:1px solid var(--line); font-variant-numeric: tabular-nums; font-size:11px; }
  .metric b { font-weight:600; color:var(--muted); }
  .metric span, .metric b { white-space: nowrap; }
  .pass { color:#1f7a46; } .fail { color:#b42318; }
  ul { margin:6px 0 0; padding-left:18px; color:var(--muted); }
  #liveDecision { background:#eef3f6; border:1px solid var(--line); padding:8px 10px; margin:0 0 8px; min-height:2.6em; }
  #events { list-style:none; padding:0; max-height:180px; overflow:auto; }
  #events li { padding:6px 8px; margin:0 0 6px; border-left:4px solid var(--line); background:#fff; font-size:12px; color:var(--ink); cursor:pointer; }
  #events li.active { outline:1px solid var(--ink); }
  #events li[data-kind=hold] { border-left-color:#6b7280; }
  #events li[data-kind=exit1] { border-left-color:#c05621; }
  #events li[data-kind=back] { border-left-color:#2f7d57; }
  #events li[data-kind=exit2] { border-left-color:#7b2d8e; }
  #events li[data-kind=nocur] { border-left-color:#1d4ed8; }
  #events .t { font-variant-numeric: tabular-nums; color:var(--muted); margin-right:6px; }
  @media (max-width: 1100px) { .page { grid-template-columns: 1fr; } aside { max-height:none; } }
  @media (max-width: 760px) { .grid { grid-template-columns: 1fr; } }
</style>
</head>
<body>
<header>
  <h1>ArduPilot 无人船四仿真对照</h1>
  <p class="sub">控制律与 STM32F405RGT6/User/boat 同步。上排传感全准，下排湖测 GNSS 残差回放；左列无风无流，右列有风有流。点到点进入 2 m 圈后停桨回手动；定点稳泊只前进不倒车；定速巡航是开环油门；定向只锁艏向、不贴地面直线。点功能页后四格同步播放。</p>
  <p class="links">原始单页：
    <a href="传感器全准的仿真/usv_sim_perfect_ideal.html">全准无风流</a>
    <a href="传感器全准的仿真/usv_sim_perfect_current_wind.html">全准有风流</a>
    <a href="真实数据/usv_sim_ideal.html">残差无风流</a>
    <a href="真实数据/usv_sim_current_wind.html">残差有风流</a>
  </p>
</header>
<div class="tabs" id="tabs"></div>
<div class="legend">
  <span><i class="sw" style="background:#1f4b73"></i>真实轨迹</span>
  <span><i class="sw" style="background:#b0893a"></i>GNSS 原始</span>
  <span><i class="sw" style="background:#2f7d57"></i>导航位置（原始 GNSS，无滤波）</span>
  <span><i class="sw" style="background:#b42318"></i>目标 / 2 m 圈</span>
  <span id="legend-decay" style="display:none">
    <i class="sw" style="background:#1f4b73"></i>满风流
    <i class="sw" style="background:#8a5a2b;margin-left:10px"></i>风流缓降
    <i class="sw" style="background:#2f7d57;margin-left:10px"></i>无风无流
  </span>
</div>
<div class="controls">
  <button id="play">播放 / 暂停</button>
  <label>进度 <input id="scrub" type="range" min="0" max="1" step="0.001" value="1" style="width:240px"></label>
  <span id="clock"></span>
</div>
<div class="page">
  <div class="grid" id="grid"></div>
  <aside>
    <h2 id="sideTitle"></h2>
    <p id="purpose" class="sub"></p>
    <h2>当前决策</h2>
    <p id="liveDecision"></p>
    <h2>稳泊节点</h2>
    <ul id="events"></ul>
    <h2>四条件指标</h2>
    <div id="metrics"></div>
    <h2>选中格备注</h2>
    <ul id="notes"></ul>
  </aside>
</div>
<script>
const BUNDLE = __DATA__;
let si = 0, pi = 0, playing = false, cursor = 1;
const KEYS = [
  ['min_true_dist_m','最近真实距离 m'],
  ['final_true_dist_m','结束真实距离 m'],
  ['time_to_arrive_s','arrived 时刻 s'],
  ['hold_entry_true_dist_m','切稳泊真距 m'],
  ['hold_rms_m','稳泊 RMS m'],
  ['true_in_2m_frac','真实在 2m 内'],
  ['true_in_3m_frac','真实在 3m 内'],
  ['filter_lag_rms_m','导航位置相对真值 RMS m'],
  ['max_true_after_arrive_m','进圈后最远 m'],
  ['mean_abs_crosstrack_m','平均横向 m'],
  ['heading_drift_deg','航向漂移 °'],
  ['east_run_m','东向位移 m'],
  ['fly_count','飞点数'],
];

function sc() { return BUNDLE.scenarios[si]; }
function runOf(i) { return BUNDLE.panels[i]; }
function findData(run, key) { return (run.data || []).find(d => d.key === key) || null; }
function nidx(d) { return Math.max(2, Math.floor(cursor * (d.true.length-1)) + 1); }
function fmt(v) { return (v === undefined || v === null) ? '—' : v; }
function fmtFrac(k, v) {
  if ((k||'').indexOf('frac') >= 0 && typeof v === 'number') return Math.round(v*100) + '%';
  return fmt(v);
}
function phaseColor(p) {
  if (p === 'hold_idle') return '#6b7280';
  if (p === 'first_exit') return '#c05621';
  if (p === 'back_in') return '#2f7d57';
  if (p === 'exit_again') return '#7b2d8e';
  if (p === 'no_cur_out') return '#1d4ed8';
  if (p === 'no_cur_in') return '#64748b';
  return '#1f4b73';
}
function kindColor(k) {
  if (k === 'hold') return '#6b7280';
  if (k === 'exit1') return '#c05621';
  if (k === 'back') return '#2f7d57';
  if (k === 'exit2') return '#7b2d8e';
  if (k === 'nocur') return '#1d4ed8';
  return '#1c2430';
}
function envColor(s) {
  if (s >= 0.99) return '#1f4b73';
  if (s <= 0.01) return '#2f7d57';
  return '#8a5a2b';
}

const grid = document.getElementById('grid');
BUNDLE.panels.forEach((run, i) => {
  const cell = document.createElement('div');
  cell.className = 'cell' + (i===0 ? ' active' : '');
  cell.dataset.i = String(i);
  cell.innerHTML = '<h3><span>'+run.title+'</span><span class="ok" id="ok-'+run.id+'"></span></h3>' +
    '<canvas class="map" width="700" height="260"></canvas>' +
    '<canvas class="dist" width="700" height="88"></canvas>' +
    '<div class="empty" style="display:none">该环境无此页（仅有风有流）</div>' +
    '<div class="mini" id="mini-'+run.id+'"></div>';
  cell.onclick = () => {
    pi = i;
    [...grid.children].forEach(x => x.classList.remove('active'));
    cell.classList.add('active');
    renderSide();
    drawAll();
  };
  grid.appendChild(cell);
});

BUNDLE.scenarios.forEach((s, i) => {
  const b = document.createElement('button');
  b.className = 'tab' + (i===0 ? ' active' : '');
  b.textContent = s.title;
  b.onclick = () => {
    si = i; cursor = 1;
    document.getElementById('scrub').value = 1;
    const key = BUNDLE.scenarios[si].key;
    if (!findData(runOf(pi), key)) {
      const j = BUNDLE.panels.findIndex(p => findData(p, key));
      if (j >= 0) {
        pi = j;
        [...grid.children].forEach((x, k) => x.classList.toggle('active', k===pi));
      }
    }
    [...document.getElementById('tabs').children].forEach(x => x.classList.remove('active'));
    b.classList.add('active');
    renderSide();
    drawAll();
  };
  document.getElementById('tabs').appendChild(b);
});

function seekToTime(t) {
  const d = findData(runOf(pi), sc().key);
  const tmax = d ? (d.t[d.t.length-1] || 1) : 1;
  cursor = Math.max(0, Math.min(1, t / tmax));
  document.getElementById('scrub').value = cursor;
  playing = false;
  drawAll();
}

function renderSide() {
  const key = sc().key;
  const d = findData(runOf(pi), key);
  const run = runOf(pi);
  document.getElementById('sideTitle').textContent = run.title + ' · ' + sc().title;
  document.getElementById('purpose').textContent = (d && d.purpose) ? d.purpose : sc().purpose;
  const m = document.getElementById('metrics');
  const heads = BUNDLE.panels.map(p => p.short);
  m.innerHTML = '<div class="metric"><b>量</b>' + heads.map(h => '<b>'+h+'</b>').join('') + '</div>';
  const row = (name, vals) => {
    const el = document.createElement('div');
    el.className = 'metric';
    el.innerHTML = '<span>'+name+'</span>' + vals.map(v => '<span>'+v+'</span>').join('');
    m.appendChild(el);
  };
  row('判定', BUNDLE.panels.map(p => {
    const x = findData(p, key);
    if (!x) return '—';
    return x.passed ? '<span class="pass">通过</span>' : '<span class="fail">未通过</span>';
  }));
  KEYS.forEach(([k, name]) => {
    const vals = BUNDLE.panels.map(p => {
      const x = findData(p, key);
      if (!x || x.metrics[k] === undefined) return '—';
      return fmtFrac(k, x.metrics[k]);
    });
    if (vals.every(v => v === '—')) return;
    row(name, vals);
  });
  const ul = document.getElementById('notes');
  ul.innerHTML = d ? d.notes.map(n => '<li>'+n+'</li>').join('') : '<li>该条件没有这条仿真。</li>';
  const ev = document.getElementById('events');
  const events = (d && d.events) || [];
  if (!d) {
    ev.innerHTML = '<li class="sub">该条件没有这条仿真。</li>';
  } else if (!events.length) {
    ev.innerHTML = '<li class="sub">该页没有稳泊出圈节点（巡航页或未出 2 m）。</li>';
  } else {
    ev.innerHTML = events.map((e,i) =>
      '<li data-kind="'+e.kind+'" data-i="'+i+'"><span class="t">'+e.t.toFixed(1)+' s</span>'+e.text+
      '<div class="sub">导航距 '+e.nav_dist+' m · 真距 '+e.true_dist+' m · T='+e.move+' · |c|='+e.c_mag+' m/s</div></li>'
    ).join('');
    [...ev.children].forEach(li => {
      li.onclick = (evclick) => { evclick.stopPropagation(); seekToTime(events[parseInt(li.dataset.i,10)].t); };
    });
  }
}

function bounds(cv, d) {
  const pts = d.true.concat(d.gnss, d.filt, d.tgt, [[0,0]]);
  let minE=1e9,maxE=-1e9,minN=1e9,maxN=-1e9;
  pts.forEach(p => { minE=Math.min(minE,p[0]); maxE=Math.max(maxE,p[0]); minN=Math.min(minN,p[1]); maxN=Math.max(maxN,p[1]); });
  minE-=8; maxE+=8; minN-=8; maxN+=8;
  const span = Math.max(maxE-minE, maxN-minN, 20);
  const cx=(minE+maxE)/2, cy=(minN+maxN)/2;
  return {minE:cx-span/2, maxE:cx+span/2, minN:cy-span/2, maxN:cy+span/2};
}
function xy(cv, b, e, n) {
  const x = (e - b.minE) / (b.maxE - b.minE) * cv.width;
  const y = cv.height - (n - b.minN) / (b.maxN - b.minN) * cv.height;
  return [x,y];
}
function strokePath(cv, ctx, b, pts, n, color, width) {
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  for (let i=0;i<n;i++) {
    const [x,y] = xy(cv, b, pts[i][0], pts[i][1]);
    if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  }
  ctx.stroke();
}
function strokeTrue(cv, ctx, b, d, n) {
  const pts = d.true;
  const phases = d.phase;
  const holdPhases = {hold_idle:1, first_exit:1, back_in:1, exit_again:1, no_cur_out:1, no_cur_in:1};
  const hasHold = phases && phases.some(p => holdPhases[p]);
  if (hasHold) {
    for (let i = 1; i < n; i++) {
      const a = xy(cv, b, pts[i-1][0], pts[i-1][1]);
      const c = xy(cv, b, pts[i][0], pts[i][1]);
      ctx.beginPath();
      ctx.strokeStyle = phaseColor(phases[i]);
      ctx.lineWidth = phases[i]==='exit_again' ? 3.2 : 2.3;
      ctx.moveTo(a[0], a[1]);
      ctx.lineTo(c[0], c[1]);
      ctx.stroke();
    }
    return;
  }
  const scales = d.env_scale;
  const varies = scales && scales.some(s => s < 0.99);
  if (!varies) { strokePath(cv, ctx, b, pts, n, '#1f4b73', 2.3); return; }
  for (let i = 1; i < n; i++) {
    const a = xy(cv, b, pts[i-1][0], pts[i-1][1]);
    const c = xy(cv, b, pts[i][0], pts[i][1]);
    ctx.beginPath();
    ctx.strokeStyle = envColor(scales[i]);
    ctx.lineWidth = 2.3;
    ctx.moveTo(a[0], a[1]);
    ctx.lineTo(c[0], c[1]);
    ctx.stroke();
  }
}

function drawMap(cv, ctx, d, n, highlightEvents) {
  const b = bounds(cv, d);
  ctx.clearRect(0,0,cv.width,cv.height);
  ctx.strokeStyle = '#c5d0d6';
  ctx.lineWidth = 1;
  for (let g=-200; g<=200; g+=10) {
    const a = xy(cv,b,g,b.minN), c = xy(cv,b,g,b.maxN);
    ctx.beginPath(); ctx.moveTo(a[0],a[1]); ctx.lineTo(c[0],c[1]); ctx.stroke();
    const e = xy(cv,b,b.minE,g), f = xy(cv,b,b.maxE,g);
    ctx.beginPath(); ctx.moveTo(e[0],e[1]); ctx.lineTo(f[0],f[1]); ctx.stroke();
  }
  const t0 = xy(cv, b, d.tgt[Math.min(n-1,d.tgt.length-1)][0], d.tgt[Math.min(n-1,d.tgt.length-1)][1]);
  const r = 2 / (b.maxE-b.minE) * cv.width;
  ctx.strokeStyle = '#b42318';
  ctx.setLineDash([4,3]);
  ctx.beginPath(); ctx.arc(t0[0], t0[1], r, 0, Math.PI*2); ctx.stroke();
  ctx.setLineDash([]);
  ctx.beginPath(); ctx.fillStyle='#b42318'; ctx.arc(t0[0], t0[1], 3, 0, Math.PI*2); ctx.fill();
  strokePath(cv, ctx, b, d.gnss, n, '#b0893a', 1);
  strokePath(cv, ctx, b, d.filt, n, '#2f7d57', 1.8);
  strokeTrue(cv, ctx, b, d, n);
  const events = d.events || [];
  const tNow = d.t[n-1];
  events.forEach((e, i) => {
    if (e.t > tNow + 1e-6) return;
    const [x,y] = xy(cv, b, e.e, e.n);
    ctx.beginPath();
    ctx.fillStyle = kindColor(e.kind);
    ctx.arc(x, y, e.kind==='exit2' ? 6 : 5, 0, Math.PI*2);
    ctx.fill();
    ctx.fillStyle = '#fff';
    ctx.font = '10px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(String(i+1), x, y);
  });
  if (highlightEvents) {
    const elist = document.getElementById('events');
    [...elist.children].forEach((li, i) => {
      if (!events[i]) return;
      li.classList.toggle('active', Math.abs(events[i].t - tNow) < 0.15 || (events[i].t <= tNow && (i===events.length-1 || events[i+1].t > tNow)));
    });
  }
  const p = d.true[n-1];
  const [bx,by] = xy(cv, b, p[0], p[1]);
  const h = d.heading[n-1] * Math.PI/180;
  if (d.des_hdg && d.des_hdg.length) {
    const deg = d.des_hdg[n-1] * Math.PI/180;
    ctx.beginPath();
    ctx.strokeStyle = '#7b2d8e';
    ctx.lineWidth = 2;
    ctx.setLineDash([5,3]);
    ctx.moveTo(bx, by);
    ctx.lineTo(bx + 22 * Math.sin(deg), by - 22 * Math.cos(deg));
    ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.save();
  ctx.translate(bx,by);
  ctx.rotate(h - Math.PI/2);
  ctx.beginPath();
  ctx.fillStyle = '#1c2430';
  ctx.moveTo(10,0); ctx.lineTo(-7,5); ctx.lineTo(-7,-5); ctx.closePath(); ctx.fill();
  ctx.restore();
  (d.fly||[]).forEach((f,i) => {
    if (!f || i>=n) return;
    const [fx,fy] = xy(cv, b, d.gnss[i][0], d.gnss[i][1]);
    ctx.beginPath(); ctx.strokeStyle='#b42318'; ctx.arc(fx,fy,3.5,0,Math.PI*2); ctx.stroke();
  });
}

function drawDist(cv, ctx, d, n) {
  ctx.clearRect(0,0,cv.width,cv.height);
  const tmax = d.t[d.t.length-1] || 1;
  const trueDist = d.true.map((p,i) => {
    const tg = d.tgt[Math.min(i, d.tgt.length-1)];
    return Math.hypot(p[0]-tg[0], p[1]-tg[1]);
  });
  const navDist = d.dist || trueDist;
  let dmax = 3;
  for (let i=0;i<n;i++) dmax = Math.max(dmax, trueDist[i]||0, navDist[i]||0);
  dmax = Math.min(Math.max(dmax*1.15, 4), 40);
  function x(t) { return 40 + t/tmax * (cv.width-48); }
  function y(v) { return cv.height-16 - v/dmax * (cv.height-28); }
  ctx.strokeStyle='#ddd6c8'; ctx.lineWidth=1;
  ctx.setLineDash([4,3]);
  ctx.beginPath(); ctx.moveTo(40,y(2)); ctx.lineTo(cv.width-6,y(2)); ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle='#8a8173'; ctx.font='10px sans-serif';
  ctx.fillText('2 m', 6, y(2)+3);
  function line(arr, nn, color, w) {
    ctx.beginPath(); ctx.strokeStyle=color; ctx.lineWidth=w;
    for (let i=0;i<nn;i++) {
      const xx=x(d.t[i]), yy=y(arr[i]||0);
      if (i===0) ctx.moveTo(xx,yy); else ctx.lineTo(xx,yy);
    }
    ctx.stroke();
  }
  line(navDist, n, '#2f7d57', 1.4);
  line(trueDist, n, '#1f4b73', 2);
  (d.events||[]).forEach(e => {
    if (e.t > d.t[n-1] + 1e-6) return;
    ctx.beginPath();
    ctx.strokeStyle = kindColor(e.kind);
    ctx.lineWidth = e.kind==='exit2' ? 2 : 1;
    ctx.moveTo(x(e.t), 4);
    ctx.lineTo(x(e.t), cv.height-16);
    ctx.stroke();
  });
}

function drawAll() {
  const key = sc().key;
  let clock = sc().title;
  let varies = false;
  BUNDLE.panels.forEach((run, i) => {
    const cell = grid.children[i];
    const map = cell.querySelector('.map');
    const dist = cell.querySelector('.dist');
    const empty = cell.querySelector('.empty');
    const ok = document.getElementById('ok-'+run.id);
    const mini = document.getElementById('mini-'+run.id);
    const d = findData(run, key);
    if (!d) {
      map.style.display = 'none';
      dist.style.display = 'none';
      empty.style.display = 'flex';
      ok.textContent = '';
      mini.textContent = '';
      if (i === pi) document.getElementById('liveDecision').textContent = '—';
      return;
    }
    map.style.display = 'block';
    dist.style.display = 'block';
    empty.style.display = 'none';
    ok.textContent = d.passed ? '通过' : '未通过';
    ok.className = 'ok ' + (d.passed ? 'pass' : 'fail');
    const n = nidx(d);
    drawMap(map, map.getContext('2d'), d, n, i===pi);
    drawDist(dist, dist.getContext('2d'), d, n);
    const bits = [];
    const mt = d.metrics || {};
    if (mt.final_true_dist_m !== undefined) bits.push('终距 '+mt.final_true_dist_m+' m');
    if (mt.min_true_dist_m !== undefined) bits.push('最近 '+mt.min_true_dist_m+' m');
    if (mt.hold_rms_m !== undefined) bits.push('RMS '+mt.hold_rms_m+' m');
    if (mt.mean_abs_crosstrack_m !== undefined) bits.push('横向 '+mt.mean_abs_crosstrack_m+' m');
    mini.textContent = bits.join(' · ');
    if (i === pi) {
      clock = d.t[n-1].toFixed(1) + ' s / ' + d.t[d.t.length-1].toFixed(1) + ' s  ·  ' + run.title;
      if (d.env_scale && d.env_scale.some(s => s < 0.99)) clock += '  环境×' + d.env_scale[n-1].toFixed(2);
      if (d.c_mag && d.c_mag.length) clock += '  |c|=' + d.c_mag[n-1].toFixed(3) + ' m/s';
      document.getElementById('liveDecision').textContent = (d.decision && d.decision[n-1]) ? d.decision[n-1] : '—';
    }
    if (d.env_scale && d.env_scale.some(s => s < 0.99)) varies = true;
  });
  document.getElementById('clock').textContent = clock;
  document.getElementById('legend-decay').style.display = varies ? 'inline' : 'none';
}

document.getElementById('play').onclick = () => { playing = !playing; if (playing && cursor>=0.999) cursor=0; };
document.getElementById('scrub').oninput = (e) => { cursor = parseFloat(e.target.value); playing=false; drawAll(); };
function tick() {
  if (playing) {
    cursor = Math.min(1, cursor + 0.004);
    document.getElementById('scrub').value = cursor;
    if (cursor>=1) playing=false;
    drawAll();
  }
  requestAnimationFrame(tick);
}
renderSide();
drawAll();
tick();
</script>
</body>
</html>
"""


def main() -> int:
    panels = []
    titles: dict[str, str] = {}
    purposes: dict[str, str] = {}
    for spec in PANELS:
        if not spec["path"].exists():
            raise FileNotFoundError(spec["path"])
        data = extract_data(spec["path"])
        for d in data:
            titles.setdefault(d["key"], d["title"])
            purposes.setdefault(d["key"], d["purpose"])
        panels.append(
            {
                "id": spec["id"],
                "title": spec["title"],
                "sense": spec["sense"],
                "env": spec["env"],
                "short": spec["short"],
                "href": spec["href"],
                "data": data,
            }
        )
        print("读入 %s  %d 条" % (spec["path"].name, len(data)))

    keys = []
    for key in SCENARIO_ORDER:
        if any(any(d["key"] == key for d in p["data"]) for p in panels):
            keys.append(key)
    scenarios = [{"key": k, "title": titles.get(k, k), "purpose": purposes.get(k, "")} for k in keys]

    bundle = {"panels": panels, "scenarios": scenarios}
    payload = json.dumps(bundle, ensure_ascii=False).replace("</", "<\\/")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(HTML.replace("__DATA__", payload), encoding="utf-8")
    print("对照页: %s  (%.1f MB)" % (OUT, OUT.stat().st_size / 1e6))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
