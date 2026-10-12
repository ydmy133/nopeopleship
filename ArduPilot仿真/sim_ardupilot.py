#!/usr/bin/env python3
"""ArduPilot Rover 无人船四功能软件在环。

控制律：firmware_ardupilot.py（移植自 ArduPilot/ardupilot Rover）
船体 / 风流 / 湖测 GNSS 残差：STM32F405RGT6/sim

四组：
  1. 传感器全准，无风无流
  2. 传感器全准，有风有流
  3. 真实 GNSS（湖测残差回放），无风无流
  4. 真实 GNSS，有风有流

结果写到本目录。
"""

from __future__ import annotations

import json
import math
import random
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_SIM = Path(r"E:\zhudian\STM32F405RGT6\sim")
if str(_SIM) not in sys.path:
    sys.path.append(str(_SIM))
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from firmware_ardupilot import (  # noqa: E402
    HULL_SPEED_MAX,
    LOIT_RADIUS,
    NAV_DT,
    WP_RADIUS,
    ArduPilotBoat,
)
from sim_real import build_gnss_error_tape, load_real_specs  # noqa: E402
from sim_usv import (  # noqa: E402
    ORIGIN_LAT,
    ORIGIN_LON,
    DT_PHYS,
    GNSS_PERIOD,
    HEADING_PERIOD,
    Boat,
    EnvFlags,
    GnssNoise,
    Sample,
    ScenarioResult,
    SenseFlags,
    build_hold_decay_spec,
    build_specs,
    en_to_along_cross,
    env_force_scale,
    latlon_to_en,
    measure_gnss,
    sample_error_tape,
    wrap360,
    write_html,
)

SEED = 20260921
OUT_DIR = _HERE


def boat_ne_vel(boat: Boat) -> tuple[float, float]:
    h = math.radians(boat.heading)
    ve = boat.u * math.sin(h) + boat.current_e + boat.wind_e
    vn = boat.u * math.cos(h) + boat.current_n + boat.wind_n
    return ve, vn


def describe(fw: ArduPilotBoat, ever_out: bool, ever_back: bool) -> tuple[str, str]:
    if fw.mode == "cruise_speed":
        return "cruise_speed", fw.decision
    if fw.mode == "cruise_dir":
        return "cruise_dir", fw.decision
    if fw.mode == "navigate" and fw.arrived and not fw._in_loiter:
        return "arrived_manual", fw.decision
    if fw._in_loiter:
        if fw._loiter_dist > LOIT_RADIUS:
            phase = "exit_again" if ever_back else "first_exit"
            return phase, fw.decision
        if ever_out:
            return "back_in", "Loiter 已回到圈内: u_des->0, 航向保持"
        return "hold_idle", fw.decision
    return "approach", fw.decision


def apply_meas(fw: ArduPilotBoat, boat: Boat, spec: dict, sense: SenseFlags, t: float, dt: float, rng, noise) -> int:
    ve, vn = boat_ne_vel(boat)
    fly = 0
    if sense.perfect:
        fw.push_truth(boat.lat, boat.lon, vn, ve, boat.heading, boat.gz_left)
        return 0
    fw.heading_meas = boat.heading
    fw.gz_meas = boat.gz_left
    if sense.real_error:
        de, dn, dh, dgz, fly = sample_error_tape(spec.get("gnss_error") or [], t)
        fw.heading_meas = wrap360(boat.heading + dh)
        fw.gz_meas = boat.gz_left + dgz
        te, tn = latlon_to_en(boat.lat, boat.lon)
        lat, lon = _en_to_ll(te + de, tn + dn)
        fw.push_gnss(lat, lon, vn, ve)
        return fly
    if sense.clean:
        fw.push_gnss(boat.lat, boat.lon, vn, ve)
        return 0
    g_lat, g_lon, fly = measure_gnss(boat, noise, rng, dt)
    fw.heading_meas = wrap360(boat.heading + rng.gauss(0.0, 0.35))
    fw.gz_meas = boat.gz_left + rng.gauss(0.0, 0.6)
    fw.push_gnss(g_lat, g_lon, vn, ve)
    return fly


def _en_to_ll(e: float, n: float) -> tuple[float, float]:
    from sim_usv import en_to_latlon

    return en_to_latlon(e, n)


def run_scenario(spec: dict, rng: random.Random, env: EnvFlags, sense: SenseFlags) -> ScenarioResult:
    base_ce = spec["current_e"] if env.current else 0.0
    base_cn = spec["current_n"] if env.current else 0.0
    base_we = spec.get("wind_e", 0.0) if env.wind else 0.0
    base_wn = spec.get("wind_n", 0.0) if env.wind else 0.0
    base_wyaw = spec.get("wind_yaw_dps", 0.0) if env.wind else 0.0
    boat = Boat(
        lat=spec["start_lat"],
        lon=spec["start_lon"],
        heading=spec["start_heading"],
        current_e=base_ce,
        current_n=base_cn,
        wind_e=base_we,
        wind_n=base_wn,
        wind_yaw_dps=base_wyaw,
    )
    fw = ArduPilotBoat()
    fw.set_origin(ORIGIN_LAT, ORIGIN_LON)
    noise = GnssNoise()
    apply_meas(fw, boat, spec, sense, 0.0, 0.1, rng, noise)
    fw.apply_command(spec["mode"], spec["move_speed"], spec["target_lat"], spec["target_lon"], fw.heading_meas)

    duration = float(spec["duration_s"])
    t = 0.0
    t_gnss = 0.0
    t_heading = 0.0
    t_log = 0.0
    t_nav = 0.0
    samples: list[Sample] = []
    first_arrived_t = None
    hold_entry_true_dist = None
    min_true_dist = 1e9
    fly_count = 0
    last_fly = 0
    lag_sum = 0.0
    lag_n = 0
    ever_out = False
    ever_back = False
    prev_phase = ""
    hold_events: list[dict] = []
    u_meas_sum = 0.0
    u_meas_n = 0

    eval_e, eval_n = latlon_to_en(spec["eval_lat"], spec["eval_lon"])
    target_lat, target_lon = spec["target_lat"], spec["target_lon"]

    while t <= duration + 1e-9:
        scale = env_force_scale(t, spec)
        boat.current_e = base_ce * scale
        boat.current_n = base_cn * scale
        boat.wind_e = base_we * scale
        boat.wind_n = base_wn * scale
        boat.wind_yaw_dps = base_wyaw * scale
        boat.step(fw.move_out, fw.steer_out, DT_PHYS)
        t += DT_PHYS

        if t - t_heading >= HEADING_PERIOD:
            t_heading = t
            if sense.perfect or sense.clean:
                fw.heading_meas = boat.heading
                fw.gz_meas = boat.gz_left
            elif sense.real_error:
                _de, _dn, dh, dgz, _fly = sample_error_tape(spec.get("gnss_error") or [], t)
                fw.heading_meas = wrap360(boat.heading + dh)
                fw.gz_meas = boat.gz_left + dgz
            else:
                fw.heading_meas = wrap360(boat.heading + rng.gauss(0.0, 0.35))
                fw.gz_meas = boat.gz_left + rng.gauss(0.0, 0.6)

        if sense.perfect:
            ve, vn = boat_ne_vel(boat)
            fw.push_truth(boat.lat, boat.lon, vn, ve, boat.heading, boat.gz_left)
        else:
            # 两次 GNSS 之间保持上一帧原始经纬度，只更新航向。船上没有位置滤波。
            fw._apply_heading()

        if t - t_gnss >= GNSS_PERIOD:
            dtg = t - t_gnss
            t_gnss = t
            if not sense.perfect:
                last_fly = apply_meas(fw, boat, spec, sense, t, dtg, rng, noise)
                fly_count += last_fly

        # 与船上一致，导航 10 Hz，两次调用之间保持上一拍油门和舵
        if t - t_nav >= NAV_DT - 1e-9:
            t_nav = t
            fw.update(NAV_DT)
        u_meas_sum += fw.ahrs.u_meas
        u_meas_n += 1

        te, tn = latlon_to_en(boat.lat, boat.lon)
        true_dist = math.hypot(te - eval_e, tn - eval_n)
        min_true_dist = min(min_true_dist, true_dist)
        arrived = int(fw.arrived)
        if arrived and first_arrived_t is None and spec["mode"] in ("navigate", "fixed_point"):
            first_arrived_t = t
            hold_entry_true_dist = true_dist

        if t - t_log >= 0.10:
            t_log = t
            ge, gn = latlon_to_en(fw.gnss_lat, fw.gnss_lon)
            fe, fn = latlon_to_en(fw.nav_lat, fw.nav_lon)
            xe, xn = latlon_to_en(target_lat, target_lon)
            te0, tn0 = te - eval_e, tn - eval_n
            ge, gn = ge - eval_e, gn - eval_n
            fe, fn = fe - eval_e, fn - eval_n
            xe, xn = xe - eval_e, xn - eval_n
            lag_sum += math.hypot(te0 - fe, tn0 - fn)
            lag_n += 1
            tr = fw.get_hold_trace()
            if tr.get("into_valid"):
                ever_out = True
            if ever_out and float(tr.get("nav_dist") or 99) <= LOIT_RADIUS:
                ever_back = True
            phase, decision = describe(fw, ever_out, ever_back)
            if phase != prev_phase:
                kind_map = {
                    "hold_idle": "hold",
                    "first_exit": "exit1",
                    "back_in": "back",
                    "exit_again": "exit2",
                }
                kind = kind_map.get(phase, "")
                if kind:
                    hold_events.append(
                        {
                            "t": round(t, 2),
                            "e": round(te0, 2),
                            "n": round(tn0, 2),
                            "kind": kind,
                            "phase": phase,
                            "text": decision,
                            "nav_dist": round(float(tr.get("nav_dist") or 0.0), 3),
                            "true_dist": round(true_dist, 3),
                            "c_mag": 0.0,
                            "into_hdg": round(float(tr.get("into_heading") or 0.0), 1),
                            "move": int(fw.move_out),
                        }
                    )
                prev_phase = phase
            dist_nav = float(tr.get("nav_dist") or true_dist)
            samples.append(
                Sample(
                    t=round(t, 2),
                    true_e=te0,
                    true_n=tn0,
                    gnss_e=ge,
                    gnss_n=gn,
                    filt_e=fe,
                    filt_n=fn,
                    tgt_e=xe,
                    tgt_n=xn,
                    heading=boat.heading,
                    dist=dist_nav,
                    arrived=arrived,
                    move=fw.move_out,
                    steer=fw.steer_out,
                    fly=last_fly,
                    env_scale=env_force_scale(t, spec),
                    phase=phase,
                    decision=decision,
                    c_mag=0.0,
                    into_hdg=round(float(tr.get("into_heading") or 0.0), 1),
                    des_hdg=round(float(fw.des_hdg or boat.heading), 1),
                )
            )
            last_fly = 0

    result = ScenarioResult(
        key=spec["key"],
        title=spec["title"],
        purpose=spec["purpose"],
        duration_s=duration,
        samples=samples,
        hold_events=hold_events,
    )
    result.metrics["sense"] = sense.tag()
    result.metrics["env"] = env.tag()
    result.metrics["controller"] = "ardupilot_rover"
    result.metrics["min_true_dist_m"] = round(min_true_dist, 3)
    result.metrics["fly_count"] = fly_count
    result.metrics["filter_lag_rms_m"] = round(lag_sum / max(1, lag_n), 3)
    result.metrics["final_true_e"] = round(samples[-1].true_e, 2) if samples else 0.0
    result.metrics["final_true_n"] = round(samples[-1].true_n, 2) if samples else 0.0
    result.metrics["final_heading_deg"] = round(samples[-1].heading, 2) if samples else 0.0
    result.metrics["mean_u_meas"] = round(u_meas_sum / max(1, u_meas_n), 3)
    result.metrics["u_des"] = round(abs(spec["move_speed"]) / 100.0 * HULL_SPEED_MAX, 3)
    last_true = true_dist
    result.metrics["final_true_dist_m"] = round(last_true, 3)
    _judge(result, spec, sense, env, samples, first_arrived_t, hold_entry_true_dist, min_true_dist, last_true)
    return result


def _judge(
    result: ScenarioResult,
    spec: dict,
    sense: SenseFlags,
    env: EnvFlags,
    samples: list[Sample],
    first_arrived_t,
    hold_entry_true_dist,
    min_true_dist: float,
    last_true: float,
) -> None:
    mode = spec["mode"]
    if mode in ("navigate", "fixed_point"):
        result.metrics["time_to_arrive_s"] = None if first_arrived_t is None else round(first_arrived_t, 1)
        result.metrics["arrived_flag"] = 0 if first_arrived_t is None else 1
        result.metrics["hold_entry_true_dist_m"] = (
            None if hold_entry_true_dist is None else round(hold_entry_true_dist, 3)
        )
        true_in = [math.hypot(s.true_e - s.tgt_e, s.true_n - s.tgt_n) for s in samples]
        if mode == "navigate" and first_arrived_t is not None:
            hold_samples = [s for s in samples if s.t >= first_arrived_t]
            if hold_samples:
                true_in = [math.hypot(s.true_e - s.tgt_e, s.true_n - s.tgt_n) for s in hold_samples]
        n = max(1, len(true_in))
        frac2 = sum(1 for d in true_in if d <= 2.0) / n
        frac3 = sum(1 for d in true_in if d <= 3.0) / n
        rms = math.sqrt(sum(x * x for x in true_in) / n)
        result.metrics["hold_rms_m"] = round(rms, 3)
        result.metrics["true_in_2m_frac"] = round(frac2, 3)
        result.metrics["true_in_3m_frac"] = round(frac3, 3)
        if mode == "navigate":
            result.notes.append(
                "船上点到点: 贴航线前进，进入 WP_RADIUS=2 m 后停推力并切手动，不稳泊。有流时到点后会漂。"
            )
            result.passed = first_arrived_t is not None and min_true_dist <= WP_RADIUS + 0.5
            result.notes.append(
                "最近 %.2f m，到点时刻 %s，结束时 %.2f m。"
                % (
                    min_true_dist,
                    "未到" if hold_entry_true_dist is None else "%.2f m" % hold_entry_true_dist,
                    last_true,
                )
            )
        else:
            result.notes.append(
                "船上定点稳泊: 圈内收油，圈外船头朝锚点前进顶回，R=2 m，不倒车。"
            )
            result.passed = frac3 >= 0.55 and rms <= 5.0
            result.notes.append("真实在 2 m 内 %.0f%%、3 m 内 %.0f%%，RMS=%.2f m。" % (frac2 * 100, frac3 * 100, rms))
    elif mode == "cruise_speed":
        u_des = abs(spec["move_speed"]) / 100.0 * HULL_SPEED_MAX
        mean_u = float(result.metrics["mean_u_meas"])
        heading_drift = abs(_ang_diff(samples[-1].heading, spec["start_heading"])) if samples else 0.0
        east_run = samples[-1].true_e - samples[0].true_e if samples else 0.0
        start_e, start_n = latlon_to_en(spec["start_lat"], spec["start_lon"])
        eval_e, eval_n = latlon_to_en(spec["eval_lat"], spec["eval_lon"])
        start_e -= eval_e
        start_n -= eval_n
        h0 = spec["start_heading"]
        along0, _ = en_to_along_cross(samples[0].true_e - start_e, samples[0].true_n - start_n, h0) if samples else (0.0, 0.0)
        along1, _ = en_to_along_cross(samples[-1].true_e - start_e, samples[-1].true_n - start_n, h0) if samples else (0.0, 0.0)
        result.metrics["heading_drift_deg"] = round(heading_drift, 2)
        result.metrics["east_run_m"] = round(east_run, 1)
        result.metrics["along_run_m"] = round(along1 - along0, 1)
        result.metrics["mean_move_cmd"] = round(sum(s.move for s in samples) / max(1, len(samples)), 1)
        result.metrics["mean_abs_steer"] = round(sum(abs(s.steer) for s in samples) / max(1, len(samples)), 2)
        cmd = abs(spec["move_speed"])
        result.passed = abs(result.metrics["mean_move_cmd"] - cmd) <= 2.0 and result.metrics["mean_abs_steer"] <= 1.0
        result.notes.append(
            "船上定速巡航: 滑条 %d 直接出油门，转向固定 0，不根据对地速度改 PWM。" % cmd
        )
        result.notes.append(
            "平均油门 %.1f，平均|舵| %.2f，平均对地速度 %.2f m/s（有流时允许偏离），航向漂移 %.1f°。"
            % (result.metrics["mean_move_cmd"], result.metrics["mean_abs_steer"], mean_u, heading_drift)
        )
    else:
        start_e, start_n = latlon_to_en(spec["start_lat"], spec["start_lon"])
        eval_e, eval_n = latlon_to_en(spec["eval_lat"], spec["eval_lon"])
        start_e -= eval_e
        start_n -= eval_n
        h0 = spec["start_heading"]
        along_cross = [en_to_along_cross(s.true_e - start_e, s.true_n - start_n, h0) for s in samples]
        mean_cross = sum(abs(c) for _, c in along_cross) / max(1, len(along_cross))
        net_along = (along_cross[-1][0] - along_cross[0][0]) if along_cross else 0.0
        heading_rmse = math.sqrt(
            sum(_ang_diff(s.heading, spec["start_heading"]) ** 2 for s in samples) / max(1, len(samples))
        )
        result.metrics["mean_abs_crosstrack_m"] = round(mean_cross, 2)
        result.metrics["net_along_m"] = round(net_along, 1)
        result.metrics["heading_rmse_deg"] = round(heading_rmse, 2)
        result.passed = net_along > 20.0 and heading_rmse < 25.0
        result.notes.append(
            "船上定向巡航: 锁切入艏向，速度走速度环。不贴地面直线，有侧流时位置可以漂。"
        )
        result.notes.append("沿航向净进 %.1f m，平均横向 %.2f m，航向 RMSE %.1f°。" % (net_along, mean_cross, heading_rmse))


def _ang_diff(a: float, b: float) -> float:
    d = (a - b + 180.0) % 360.0 - 180.0
    return d


def patch_html(path: Path, sense: SenseFlags, env: EnvFlags) -> None:
    text = path.read_text(encoding="utf-8")
    text = text.replace("无人船四功能软件在环仿真", "ArduPilot 无人船四功能软件在环仿真")
    old = (
        "控制律来自现有 <code>gnss_pos_filter</code> 与 <code>navigation_controller</code>。"
        "传感：<b>__SENSE__</b>。环境：<b>__ENV__</b>。"
        "点到点开到目标点后再切定点稳泊。定点稳泊锁下发锚点：无风流静止，有风流先被吹出 2 m，"
        "再按漂移合力锁向并顶回目标、持续顶流。稳泊位置用短 τ 滤波。有风有流时多一页 100–120 s 合力缓降。"
        "坐标以目标点为原点的东-北平面（米）。"
    )
    # write_html already substituted __SENSE__/__ENV__
    import re

    text = re.sub(
        r"控制律来自现有.*东-北平面（米）。",
        (
            "控制律与 <code>STM32F405RGT6/App/Control/Boat</code> 同步："
            "点到点到 2 m 圈后停桨回手动，定点稳泊只前进不倒车，定速巡航开环油门，定向只锁艏向。"
            "船体/风流/湖测残差沿用原仿真场景。"
            "传感：<b>%s</b>。环境：<b>%s</b>。"
            "坐标以目标点为原点的东-北平面（米）。" % (sense.label(), env.label())
        ),
        text,
        count=1,
        flags=re.S,
    )
    text = text.replace("滤波后", "导航位置(原始GNSS)")
    text = text.replace("青绿=滤波距", "青绿=导航距")
    text = text.replace("滤波距 ", "导航距 ")
    path.write_text(text, encoding="utf-8")


def run_batch(specs: list[dict], env: EnvFlags, sense: SenseFlags, out_html: Path) -> list[ScenarioResult]:
    rng = random.Random(SEED)
    run_specs = list(specs)
    if env.current and env.wind and any(s["key"] == "fixed_point" for s in specs):
        hold = next(s for s in specs if s["key"] == "fixed_point")
        decay = build_hold_decay_spec()
        extra = dict(hold)
        extra["key"] = decay["key"]
        extra["title"] = decay["title"]
        extra["purpose"] = "ArduPilot Loiter；100–120 s 风流合力降到 0"
        extra["duration_s"] = decay["duration_s"]
        extra["env_decay_start_s"] = decay["env_decay_start_s"]
        extra["env_decay_end_s"] = decay["env_decay_end_s"]
        extra["mode"] = "fixed_point"
        run_specs.append(extra)
    results = [run_scenario(spec, rng, env, sense) for spec in run_specs]
    out_html.parent.mkdir(parents=True, exist_ok=True)
    write_html(results, out_html, env, sense)
    patch_html(out_html, sense, env)
    return results


def print_run(label: str, results: list[ScenarioResult]) -> None:
    print(label)
    for r in results:
        flag = "PASS" if r.passed else "FAIL"
        print("  [%s] %s" % (flag, r.title))
        keys = [
            "min_true_dist_m",
            "final_true_dist_m",
            "time_to_arrive_s",
            "hold_rms_m",
            "true_in_2m_frac",
            "mean_u_meas",
            "u_des",
            "heading_drift_deg",
            "mean_abs_crosstrack_m",
            "net_along_m",
            "heading_rmse_deg",
            "filter_lag_rms_m",
        ]
        bits = ["%s=%s" % (k, r.metrics[k]) for k in keys if k in r.metrics]
        if bits:
            print("    " + ", ".join(bits))
        for n in r.notes[:2]:
            print("    - %s" % n)


def pack_summary(results: list[ScenarioResult]) -> list[dict]:
    return [{"key": r.key, "passed": r.passed, "metrics": r.metrics, "notes": r.notes} for r in results]


def main() -> int:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    specs_perfect = build_specs()
    error_tape, err_info = build_gnss_error_tape()
    specs_real, raw = load_real_specs(error_tape)

    jobs = [
        (
            "传感器全准的仿真/usv_sim_perfect_ideal.html",
            specs_perfect,
            EnvFlags(False, False),
            SenseFlags(perfect=True),
            "1 传感器全准 · 无风无流",
        ),
        (
            "传感器全准的仿真/usv_sim_perfect_current_wind.html",
            specs_perfect,
            EnvFlags(True, True),
            SenseFlags(perfect=True),
            "2 传感器全准 · 有风有流",
        ),
        (
            "真实数据/usv_sim_ideal.html",
            specs_real,
            EnvFlags(False, False),
            SenseFlags(real_error=True),
            "3 真实 GNSS · 无风无流",
        ),
        (
            "真实数据/usv_sim_current_wind.html",
            specs_real,
            EnvFlags(True, True),
            SenseFlags(real_error=True),
            "4 真实 GNSS · 有风有流",
        ),
    ]

    all_runs = {}
    for rel, specs, env, sense, label in jobs:
        print("=" * 60)
        print(label)
        results = run_batch(specs, env, sense, OUT_DIR / rel)
        print_run(label, results)
        all_runs[rel] = results

    summary = {
        "controller": "STM32F405RGT6 App/Control/Boat synced",
        "source": r"E:\无人船\STM32F405RGT6\App\Control\Boat",
        "hull_from": r"E:\zhudian\STM32F405RGT6\sim",
        "origin_sim": {"lat": ORIGIN_LAT, "lon": ORIGIN_LON},
        "origin_real": raw.get("origin"),
        "env_from_still": raw.get("env_from_still"),
        "sense_real": err_info,
        "params": {
            "WP_RADIUS": WP_RADIUS,
            "LOIT_RADIUS": LOIT_RADIUS,
            "HULL_SPEED_MAX": HULL_SPEED_MAX,
            "note": "参数与 boat_config.h 一致：不倒车，到点不稳泊，定速开环。",
        },
        "perfect_ideal": pack_summary(all_runs["传感器全准的仿真/usv_sim_perfect_ideal.html"]),
        "perfect_current_wind": pack_summary(all_runs["传感器全准的仿真/usv_sim_perfect_current_wind.html"]),
        "real_gnss_ideal": pack_summary(all_runs["真实数据/usv_sim_ideal.html"]),
        "real_gnss_current_wind": pack_summary(all_runs["真实数据/usv_sim_current_wind.html"]),
    }
    (OUT_DIR / "usv_sim_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    import importlib.util

    merge_path = _HERE / "merge_four.py"
    spec = importlib.util.spec_from_file_location("ardupilot_merge_four", merge_path)
    merge_mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(merge_mod)
    merge_mod.main()
    print("报告目录: %s" % OUT_DIR)
    ok = all(r.passed for rs in all_runs.values() for r in rs)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
