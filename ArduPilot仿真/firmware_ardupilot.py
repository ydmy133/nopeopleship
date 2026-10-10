#!/usr/bin/env python3
"""与 STM32F405RGT6/User/boat 同步的无人船控制律。

结构来自 ArduPilot Rover，参数和产品改动以船上固件为准
（User/boat/boat_config.h、boat_modes.c、boat_app.c）：

  点到点 navigate     航线贴线，进入 2 m 圈后停推力，不切稳泊
  定点稳泊 fixed_point Loiter，LOIT_TYPE=1，只前进不倒车
  定速巡航 cruise_speed 滑条百分比直接出油门，转向 0，不跑速度环
  定向巡航 cruise_dir  锁艏向 + 速度环，不贴地面直线

全模式油门不为负。
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

# ---- ArduPilot 默认参数（源码宏 / Parameters.cpp）----
ATC_SPEED_P = 0.20
ATC_SPEED_I = 0.20
ATC_SPEED_D = 0.00
ATC_SPEED_IMAX = 1.00
ATC_ACCEL_MAX = 0.25          # boat_config.h BOAT_ACCEL_MS2
ATC_DECEL_MAX = 0.00          # 0 = 与加速相同
ATC_STOP_SPEED = 0.10
ATC_BRAKE = 0                 # 禁止负油门
ATC_STR_ANG_P = 2.00
ATC_STR_RAT_P = 0.20
ATC_STR_RAT_I = 0.20
ATC_STR_RAT_D = 0.00
ATC_STR_RAT_FF = 0.20
ATC_STR_RAT_IMAX = 1.00
ATC_STR_ACC_MAX = 30.0        # deg/s^2，船体收过，上游是 120
ATC_STR_RAT_MAX = 40.0        # deg/s
ATC_TURN_MAX_G = 0.15
PSC_POS_P = 0.20
PSC_VEL_P = 1.00
PSC_VEL_I = 0.00
PSC_VEL_D = 0.00
WP_RADIUS = 2.0
WP_SPEED_DEFAULT = 0.60       # 滑条为 0 或更大时的板上默认航速
LOIT_RADIUS = 2.0
LOIT_SPEED_GAIN = 0.5
LOIT_TYPE = 1                 # 始终船头朝锚点前进
PIVOT_ANGLE_DEG = 45.0
PIVOT_RATE_DPS = 25.0
PIVOT_ACCURACY_DEG = 5.0
STEER_SPEED_MIN = 0.30
GRAVITY = 9.80665

HULL_SPEED_MAX = 1.0          # 滑条 100% 对应的速度量级
CRUISE_SPEED = 0.80           # 速度环前馈，定速巡航不用它
CRUISE_THROTTLE = 0.70

NAV_DT = 0.10                 # 船上导航周期 100 ms


def wrap360(deg: float) -> float:
    deg %= 360.0
    if deg < 0.0:
        deg += 360.0
    if deg >= 360.0:
        deg = 0.0
    return deg


def wrap_pi(rad: float) -> float:
    x = (rad + math.pi) % (2.0 * math.pi)
    if x < 0.0:
        x += 2.0 * math.pi
    return x - math.pi


def wrap_180(deg: float) -> float:
    return math.degrees(wrap_pi(math.radians(deg)))


def sat(x: float, lo: float, hi: float) -> float:
    return lo if x < lo else hi if x > hi else x


def hypot2(x: float, y: float) -> float:
    return math.hypot(x, y)


class PID:
    """AC_PID 精简：P+I+D，FF 另取；饱和时积分只缩不涨。"""

    def __init__(self, kp: float, ki: float, kd: float, kff: float, imax: float) -> None:
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.kff = kff
        self.imax = imax
        self.integrator = 0.0
        self._last_err = 0.0
        self._has_last = False

    def reset(self) -> None:
        self.integrator = 0.0
        self._has_last = False

    def update(self, target: float, meas: float, dt: float, limit: bool = False) -> float:
        err = target - meas
        p = self.kp * err
        if dt > 1e-9:
            if limit and self.ki > 0.0:
                # 饱和：只允许积分往减小误差的方向退
                cand = self.integrator + self.ki * err * dt
                if abs(cand) < abs(self.integrator):
                    self.integrator = sat(cand, -self.imax, self.imax)
            else:
                self.integrator = sat(self.integrator + self.ki * err * dt, -self.imax, self.imax)
            d = self.kd * (err - self._last_err) / dt if self._has_last else 0.0
        else:
            d = 0.0
        self._last_err = err
        self._has_last = True
        return p + self.integrator + d

    def ff(self, target: float) -> float:
        return self.kff * target


@dataclass
class Ahrs:
    """AHRS 对外接口：p(N,E)、v、ψ、r_gyro、u_meas。"""

    lat: float = 0.0
    lon: float = 0.0
    n: float = 0.0
    e: float = 0.0
    vn: float = 0.0
    ve: float = 0.0
    heading_deg: float = 0.0
    yaw_rate_rads: float = 0.0  # 正=右转（ArduPilot）
    u_meas: float = 0.0
    valid: bool = False

    def yaw_rad(self) -> float:
        return math.radians(self.heading_deg)

    def body_x(self) -> tuple[float, float]:
        h = self.yaw_rad()
        return math.cos(h), math.sin(h)  # N, E

    def right_ne(self) -> tuple[float, float]:
        h = self.yaw_rad()
        return -math.sin(h), math.cos(h)


class AttitudeControl:
    """AR_AttitudeControl：速度环出 T，航向 P + 转艏率 PID 出 Y。"""

    def __init__(self) -> None:
        self.speed_pid = PID(ATC_SPEED_P, ATC_SPEED_I, ATC_SPEED_D, 0.0, ATC_SPEED_IMAX)
        self.rate_pid = PID(ATC_STR_RAT_P, ATC_STR_RAT_I, ATC_STR_RAT_D, ATC_STR_RAT_FF, ATC_STR_RAT_IMAX)
        self._desired_speed = 0.0
        self._desired_turn_rate = 0.0
        self._speed_active = False
        self._steer_active = False
        self.steer_limit_left = False
        self.steer_limit_right = False
        self.throttle_limit_low = False
        self.throttle_limit_high = False
        self._stop_hold = 0.0

    def reset(self) -> None:
        self.speed_pid.reset()
        self.rate_pid.reset()
        self._speed_active = False
        self._steer_active = False
        self._stop_hold = 0.0

    def get_desired_speed_accel_limited(self, desired_speed: float, dt: float) -> float:
        if not self._speed_active or ATC_ACCEL_MAX <= 0.0:
            return desired_speed
        prev = self._desired_speed
        slowing = abs(desired_speed) < abs(prev) and ATC_DECEL_MAX > 0.0
        amax = ATC_DECEL_MAX if slowing else ATC_ACCEL_MAX
        return sat(desired_speed, prev - amax * dt, prev + amax * dt)

    def get_turn_rate_from_lat_accel(self, lat_accel: float, speed: float) -> float:
        if abs(speed) < STEER_SPEED_MIN:
            speed = -STEER_SPEED_MIN if speed < 0.0 else STEER_SPEED_MIN
        return lat_accel / speed

    def turn_lat_accel_max(self) -> float:
        return max(ATC_TURN_MAX_G, 0.1) * GRAVITY

    def get_turn_rate_from_heading(self, heading_rad: float, yaw_rad: float, rate_max_rads: float) -> float:
        yaw_error = wrap_pi(heading_rad - yaw_rad)
        desired_rate = ATC_STR_ANG_P * yaw_error
        if rate_max_rads > 0.0:
            desired_rate = sat(desired_rate, -rate_max_rads, rate_max_rads)
        alpha_dps = ATC_STR_ACC_MAX if ATC_STR_ACC_MAX > 0.0 else 0.0
        if alpha_dps > 0.0:
            rmax = math.sqrt(max(0.0, 2.0 * abs(yaw_error) * math.radians(alpha_dps)))
            desired_rate = sat(desired_rate, -rmax, rmax)
        return desired_rate

    def get_steering_out_rate(self, desired_rate: float, yaw_rate: float, speed: float, dt: float) -> float:
        dt = sat(dt, 0.0, 1.0)
        if not self._steer_active:
            self.rate_pid.reset()
            self._desired_turn_rate = yaw_rate
            self._steer_active = True
        if ATC_STR_ACC_MAX > 0.0:
            change_max = math.radians(ATC_STR_ACC_MAX) * dt
            desired_rate = sat(desired_rate, self._desired_turn_rate - change_max, self._desired_turn_rate + change_max)
        if ATC_STR_RAT_MAX > 0.0:
            rmax = math.radians(ATC_STR_RAT_MAX)
            desired_rate = sat(desired_rate, -rmax, rmax)
        gmax = max(self.get_turn_rate_from_lat_accel(self.turn_lat_accel_max(), abs(speed)), math.radians(1.0))
        desired_rate = sat(desired_rate, -gmax, gmax)
        self._desired_turn_rate = desired_rate
        limit = self.steer_limit_left or self.steer_limit_right
        out = self.rate_pid.update(desired_rate, yaw_rate, dt, limit)
        out += self.rate_pid.ff(desired_rate)
        return out

    def get_steering_out_heading(self, heading_rad: float, yaw_rad: float, yaw_rate: float, speed: float, dt: float, rate_max_rads: float = 0.0) -> float:
        desired_rate = self.get_turn_rate_from_heading(heading_rad, yaw_rad, rate_max_rads)
        return self.get_steering_out_rate(desired_rate, yaw_rate, speed, dt)

    def get_steering_out_lat_accel(self, desired_accel: float, yaw_rate: float, speed: float, dt: float) -> float:
        desired_accel = sat(desired_accel, -self.turn_lat_accel_max(), self.turn_lat_accel_max())
        desired_rate = self.get_turn_rate_from_lat_accel(desired_accel, speed)
        return self.get_steering_out_rate(desired_rate, yaw_rate, speed, dt)

    def get_throttle_out_speed(self, desired_speed: float, u_meas: float, dt: float, motor_limit: bool = False) -> float:
        dt = sat(dt, 0.0, 1.0)
        if not self._speed_active:
            self.speed_pid.reset()
            self._desired_speed = u_meas
            self._speed_active = True
        self._desired_speed = self.get_desired_speed_accel_limited(desired_speed, dt)
        throttle_base = 0.0
        if CRUISE_SPEED > 0.0 and CRUISE_THROTTLE > 0.0:
            throttle_base = self._desired_speed * (CRUISE_THROTTLE / CRUISE_SPEED)
        limit = motor_limit or self.throttle_limit_low or self.throttle_limit_high
        out = self.speed_pid.update(self._desired_speed, u_meas, dt, limit)
        out += throttle_base
        self.throttle_limit_low = False
        self.throttle_limit_high = False
        if not ATC_BRAKE:
            if self._desired_speed >= 0.0 and out <= 0.0:
                out = 0.0
                self.throttle_limit_low = True
            elif self._desired_speed <= 0.0 and out >= 0.0:
                out = 0.0
                self.throttle_limit_high = True
        # 本船油门下限为 0，任何模式都不倒车
        if out < 0.0:
            out = 0.0
        return sat(out, 0.0, 1.0)

    def get_throttle_out_stop(self, u_meas: float, dt: float) -> tuple[float, bool]:
        desired_limited = self.get_desired_speed_accel_limited(0.0, dt) if self._speed_active else 0.0
        stopped = abs(u_meas) <= ATC_STOP_SPEED and abs(desired_limited) < 1e-6
        if self._stop_hold > 0.0:
            stopped = True
            self._stop_hold = max(0.0, self._stop_hold - dt)
        if stopped:
            self._stop_hold = 0.30
            self.speed_pid.reset()
            self._desired_speed = 0.0
            self._speed_active = True
            return 0.0, True
        self._stop_hold = 0.0
        return self.get_throttle_out_speed(desired_limited, u_meas, dt), False


class PosControl:
    """AR_PosControl：e_p → v_des → a_cmd → u_des / r_des。"""

    def __init__(self, atc: AttitudeControl) -> None:
        self.atc = atc
        self.speed_max = HULL_SPEED_MAX
        self.vel_i_n = 0.0
        self.vel_i_e = 0.0
        self.desired_speed = 0.0
        self.desired_turn_rate = 0.0
        self.desired_lat_accel = 0.0
        self.pos_target_n = 0.0
        self.pos_target_e = 0.0
        self.vel_desired_n = 0.0
        self.vel_desired_e = 0.0
        self.reversed = False

    def reset(self) -> None:
        self.vel_i_n = 0.0
        self.vel_i_e = 0.0

    def set_pos_vel(self, n: float, e: float, vn: float, ve: float) -> None:
        self.pos_target_n = n
        self.pos_target_e = e
        self.vel_desired_n = vn
        self.vel_desired_e = ve

    def update(self, ahrs: Ahrs, dt: float) -> None:
        en = self.pos_target_n - ahrs.n
        ee = self.pos_target_e - ahrs.e
        vn_t = self.vel_desired_n + PSC_POS_P * en
        ve_t = self.vel_desired_e + PSC_POS_P * ee
        sp = math.hypot(vn_t, ve_t)
        if sp > self.speed_max > 0.0:
            vn_t *= self.speed_max / sp
            ve_t *= self.speed_max / sp
        evn = vn_t - ahrs.vn
        eve = ve_t - ahrs.ve
        an = PSC_VEL_P * evn + self.vel_i_n
        ae = PSC_VEL_P * eve + self.vel_i_e
        if PSC_VEL_I > 0.0:
            self.vel_i_n = sat(self.vel_i_n + PSC_VEL_I * evn * dt, -1.0, 1.0)
            self.vel_i_e = sat(self.vel_i_e + PSC_VEL_I * eve * dt, -1.0, 1.0)
        # I 只留侧向（源码把前向 I 投影掉）
        rn, re = ahrs.right_ne()
        lat_i = self.vel_i_n * rn + self.vel_i_e * re
        self.vel_i_n = lat_i * rn
        self.vel_i_e = lat_i * re

        fn, fe = ahrs.body_x()
        accel_fwd = an * fn + ae * fe
        accel_lat = an * rn + ae * re
        vel_fwd = vn_t * fn + ve_t * fe
        stopping = abs(self.vel_desired_n) < 1e-6 and abs(self.vel_desired_e) < 1e-6
        # 目标在船后方时前向分量为负，钳成 0，先转向再前进
        des_speed = vel_fwd if vel_fwd > 0.0 else 0.0
        self.desired_speed = self.atc.get_desired_speed_accel_limited(des_speed, dt)
        self.desired_lat_accel = 0.0 if stopping else accel_lat
        self.desired_turn_rate = self.atc.get_turn_rate_from_lat_accel(self.desired_lat_accel, self.desired_speed)


class WPNav:
    """AR_WPNav 单段航线：沿 A→B 推虚拟目标（加速度限制代替完整 S 曲线）。"""

    def __init__(self, atc: AttitudeControl, pos: PosControl) -> None:
        self.atc = atc
        self.pos = pos
        self.origin_n = 0.0
        self.origin_e = 0.0
        self.dest_n = 0.0
        self.dest_e = 0.0
        self.s = 0.0
        self.s_dot = 0.0
        self.length = 0.0
        self.dir_n = 1.0
        self.dir_e = 0.0
        self.speed_max = HULL_SPEED_MAX
        self.reached = False
        self.valid = False
        self.distance = 0.0
        self.wp_bearing_deg = 0.0
        self.cte = 0.0
        self.desired_speed = 0.0
        self.desired_turn_rate = 0.0

    def set_desired_location(self, origin_n: float, origin_e: float, dest_n: float, dest_e: float, speed_max: float) -> None:
        self.origin_n, self.origin_e = origin_n, origin_e
        self.dest_n, self.dest_e = dest_n, dest_e
        dn = dest_n - origin_n
        de = dest_e - origin_e
        self.length = math.hypot(dn, de)
        if self.length < 1e-3:
            self.dir_n, self.dir_e = 1.0, 0.0
            self.length = 1e-3
        else:
            self.dir_n, self.dir_e = dn / self.length, de / self.length
        self.s = 0.0
        self.s_dot = 0.0
        self.speed_max = max(speed_max, 0.05)
        self.pos.speed_max = self.speed_max
        self.reached = False
        self.valid = True
        self.pos.reset()

    def update(self, ahrs: Ahrs, dt: float) -> None:
        if not self.valid:
            self.desired_speed = 0.0
            self.desired_turn_rate = 0.0
            return
        dn = ahrs.n - self.dest_n
        de = ahrs.e - self.dest_e
        self.distance = math.hypot(dn, de)
        self.wp_bearing_deg = wrap360(math.degrees(math.atan2(self.dest_e - ahrs.e, self.dest_n - ahrs.n)))
        vn = ahrs.n - self.origin_n
        ve = ahrs.e - self.origin_e
        self.cte = ve * self.dir_n - vn * self.dir_e  # 左侧为负（与源码叉积符号一致）

        remaining = max(0.0, self.length - self.s)
        v_stop = math.sqrt(max(0.0, 2.0 * ATC_ACCEL_MAX * remaining))
        v_des = min(self.speed_max, v_stop)
        self.s_dot = sat(v_des, self.s_dot - ATC_ACCEL_MAX * dt, self.s_dot + ATC_ACCEL_MAX * dt)
        self.s += self.s_dot * dt
        if self.s > self.length:
            self.s = self.length
            self.s_dot = 0.0
        pn = self.origin_n + self.dir_n * self.s
        pe = self.origin_e + self.dir_e * self.s
        self.pos.set_pos_vel(pn, pe, self.dir_n * self.s_dot, self.dir_e * self.s_dot)
        self.pos.update(ahrs, dt)
        self.desired_speed = self.pos.desired_speed
        self.desired_turn_rate = self.pos.desired_turn_rate

        # 产品只认 2 m 圈，侧向开过终点不算到达
        if not self.reached and self.distance <= WP_RADIUS:
            self.reached = True


class ArduPilotBoat:
    """四功能调度，对应 Rover ModeAuto / Loiter / Steering / Guided HeadingAndSpeed。"""

    def __init__(self) -> None:
        self.atc = AttitudeControl()
        self.pos = PosControl(self.atc)
        self.wp = WPNav(self.atc, self.pos)
        self.ahrs = Ahrs()
        self.mode = "navigate"
        self.move_out = 0
        self.steer_out = 0
        self.throttle = 0.0
        self.steering_y = 0.0
        self.nav_lat = 0.0
        self.nav_lon = 0.0
        self.gnss_lat = 0.0
        self.gnss_lon = 0.0
        self.heading_meas = 0.0
        self.gz_meas = 0.0
        self._psi_des_rad = 0.0
        self._u_des = 0.0
        self._loiter_n = 0.0
        self._loiter_e = 0.0
        self._loiter_yaw_cd = 0.0
        self._in_loiter = False
        self._loiter_dist = 0.0
        self._wp_speed = HULL_SPEED_MAX
        self._cruise_pct = 0
        self._pivot_active = False
        self.arrived = 0
        self.phase = ""
        self.decision = ""
        self.des_hdg = 0.0
        self.perfect = False
        self._lat0 = 0.0
        self._lon0 = 0.0
        self._m_lat = 111132.92
        self._m_lon = 111412.84 * math.cos(math.radians(30.58))

    def set_origin(self, lat: float, lon: float) -> None:
        self._lat0 = lat
        self._lon0 = lon
        c = math.cos(math.radians(lat))
        self._m_lon = 111412.84 * max(c, 0.2)

    def en(self, lat: float, lon: float) -> tuple[float, float]:
        e = (lon - self._lon0) * self._m_lon
        n = (lat - self._lat0) * self._m_lat
        return e, n

    def latlon(self, e: float, n: float) -> tuple[float, float]:
        return self._lat0 + n / self._m_lat, self._lon0 + e / self._m_lon

    def _apply_heading(self) -> None:
        """航向和角速度直通。船模 gz 左正，控制律右正，所以取反。"""
        self.ahrs.heading_deg = wrap360(self.heading_meas)
        self.ahrs.yaw_rate_rads = math.radians(-self.gz_meas)
        h = self.ahrs.yaw_rad()
        self.ahrs.u_meas = self.ahrs.vn * math.cos(h) + self.ahrs.ve * math.sin(h)

    def _set_fix(self, lat: float, lon: float, vn: float, ve: float) -> None:
        """导航用的就是这一帧位置，不再做经纬度滤波。"""
        e, n = self.en(lat, lon)
        self.gnss_lat, self.gnss_lon = lat, lon
        self.ahrs.n, self.ahrs.e = n, e
        self.ahrs.vn, self.ahrs.ve = vn, ve
        self.ahrs.lat, self.ahrs.lon = lat, lon
        self.ahrs.valid = True
        self.nav_lat, self.nav_lon = lat, lon
        self._apply_heading()

    def push_truth(self, lat: float, lon: float, vn: float, ve: float, heading: float, gz_left: float) -> None:
        self.heading_meas = heading
        self.gz_meas = gz_left
        self._set_fix(lat, lon, vn, ve)
        self.perfect = True

    def push_gnss(self, lat: float, lon: float, vn: float, ve: float) -> None:
        """原始 GNSS 直接进导航。vn/ve 对应模块报的对地速度，不是位置滤波估出来的。"""
        self._set_fix(lat, lon, vn, ve)

    def _speed_from_slider(self, move_speed: int, cap_wp: bool) -> float:
        """滑条 0~100 换成 m/s。点到点再与 WP_SPEED 取较小值；负值当 0。"""
        pct = 0 if move_speed < 0 else move_speed
        if pct > 100:
            pct = 100
        user = pct / 100.0 * HULL_SPEED_MAX
        if cap_wp:
            speed = WP_SPEED_DEFAULT if pct <= 0 else min(WP_SPEED_DEFAULT, user)
        else:
            speed = user
        return max(speed, 0.0)

    def apply_command(self, mode: str, move_speed: int, target_lat: float, target_lon: float, heading_meas: float) -> None:
        self.mode = mode
        self._cruise_pct = 0 if move_speed < 0 else min(100, move_speed)
        self._wp_speed = self._speed_from_slider(move_speed, cap_wp=True)
        self.atc.reset()
        self.pos.reset()
        self._in_loiter = False
        self._pivot_active = False
        self.arrived = 0
        te, tn = self.en(target_lat, target_lon)
        if not self.ahrs.valid:
            self.ahrs.heading_deg = wrap360(heading_meas)
            self.ahrs.valid = True
        if mode == "navigate":
            self.wp.set_desired_location(self.ahrs.n, self.ahrs.e, tn, te, max(self._wp_speed, 0.05))
            self._loiter_n, self._loiter_e = tn, te
        elif mode == "fixed_point":
            self._start_loiter(tn, te, heading_meas, use_current=False)
        elif mode == "cruise_speed":
            self._u_des = self._cruise_pct / 100.0 * HULL_SPEED_MAX
        elif mode == "cruise_dir":
            self._psi_des_rad = math.radians(wrap360(heading_meas))
            self._u_des = self._speed_from_slider(move_speed, cap_wp=False)

    def _start_loiter(self, n: float, e: float, heading_deg: float, use_current: bool) -> None:
        if use_current:
            self._loiter_n, self._loiter_e = self.ahrs.n, self.ahrs.e
        else:
            self._loiter_n, self._loiter_e = n, e
        self._loiter_yaw_cd = wrap360(heading_deg) * 100.0
        self._in_loiter = True
        self.arrived = 1

    def _calc_throttle(self, target_speed: float, dt: float) -> None:
        if abs(target_speed) < 1e-6:
            thr, _stopped = self.atc.get_throttle_out_stop(self.ahrs.u_meas, dt)
        else:
            thr = self.atc.get_throttle_out_speed(target_speed, self.ahrs.u_meas, dt)
        self.throttle = sat(thr, 0.0, 1.0)
        self.move_out = int(round(self.throttle * 100.0))
        if self.move_out < 0:
            self.move_out = 0

    def _set_steering(self, y: float) -> None:
        self.steering_y = sat(y, -1.0, 1.0)
        # ArduPilot Y 正=右转；船模 steer 正=左转
        self.steer_out = int(round(-self.steering_y * 100.0))
        self.steer_out = max(-100, min(100, self.steer_out))

    def _update_loiter(self, dt: float) -> None:
        dn = self._loiter_n - self.ahrs.n
        de = self._loiter_e - self.ahrs.e
        dist = math.hypot(dn, de)
        self._loiter_dist = dist
        if dist <= LOIT_RADIUS:
            u_des = self.atc.get_desired_speed_accel_limited(0.0, dt)
            yaw_des = self._loiter_yaw_cd * 0.01
            self.phase = "hold_idle"
            self.decision = "Loiter 圈内: u_des->0 走 stop, 航向保持切入航向"
        else:
            u_des = min((dist - LOIT_RADIUS) * LOIT_SPEED_GAIN, WP_SPEED_DEFAULT)
            yaw_des = wrap360(math.degrees(math.atan2(de, dn)))
            yaw_err = wrap_180(yaw_des - self.ahrs.heading_deg)
            # LOIT_TYPE=1：始终船头朝锚点，不倒车
            if LOIT_TYPE == 2 or (LOIT_TYPE == 0 and abs(yaw_err) > 90.0):
                yaw_des = wrap360(yaw_des + 180.0)
                yaw_err = wrap_180(yaw_des - self.ahrs.heading_deg)
                u_des = -abs(u_des)
            yaw_error_ratio = 1.0 - sat(abs(yaw_err / 90.0), 0.0, 1.0) * 0.5
            u_des *= yaw_error_ratio
            if u_des < 0.0 or abs(yaw_err) > PIVOT_ANGLE_DEG:
                u_des = 0.0
            self._loiter_yaw_cd = yaw_des * 100.0
            self.phase = "first_exit"
            self.decision = "稳泊圈外: 船头朝锚点前进, 偏差大时先转再走, 不倒车"
        self.des_hdg = yaw_des
        y = self.atc.get_steering_out_heading(
            math.radians(yaw_des), self.ahrs.yaw_rad(), self.ahrs.yaw_rate_rads, self.ahrs.u_meas, dt
        )
        self._set_steering(y)
        self._calc_throttle(u_des, dt)
        self.arrived = 1 if dist <= LOIT_RADIUS else 0

    def _pivot_check(self, desired_deg: float) -> None:
        err = abs(wrap_180(desired_deg - self.ahrs.heading_deg))
        if err > PIVOT_ANGLE_DEG:
            self._pivot_active = True
            return
        if self._pivot_active and err < PIVOT_ACCURACY_DEG:
            self._pivot_active = False

    def _navigate_toward_wp(self, dt: float) -> None:
        """航向偏差大时先把速度收到 0 再转，否则用航线给出的速度和转弯率。"""
        self._pivot_check(self.wp.wp_bearing_deg)
        if self._pivot_active:
            slowed = self.atc.get_desired_speed_accel_limited(0.0, dt)
            if abs(slowed) < 1e-3:
                rate = self.atc.get_turn_rate_from_heading(
                    math.radians(self.wp.wp_bearing_deg),
                    self.ahrs.yaw_rad(),
                    math.radians(PIVOT_RATE_DPS),
                )
            else:
                rate = 0.0
            self._calc_throttle(max(0.0, slowed), dt)
            y = self.atc.get_steering_out_rate(rate, self.ahrs.yaw_rate_rads, self.ahrs.u_meas, dt)
            self._set_steering(y)
            return
        speed = self.wp.desired_speed if self.wp.desired_speed > 0.0 else 0.0
        self._calc_throttle(speed, dt)
        y = self.atc.get_steering_out_rate(
            self.wp.desired_turn_rate, self.ahrs.yaw_rate_rads, self.ahrs.u_meas, dt
        )
        self._set_steering(y)

    def update(self, dt: float) -> None:
        if not self.ahrs.valid:
            self.move_out = 0
            self.steer_out = 0
            return
        mode = self.mode
        if mode == "navigate":
            self.wp.update(self.ahrs, dt)
            if self.wp.reached:
                self.move_out = 0
                self.steer_out = 0
                self.throttle = 0.0
                self.steering_y = 0.0
                self.arrived = 1
                self._pivot_active = False
                self.phase = "arrived_manual"
                self.decision = "进入 2 m 圈，停推力并切手动，不稳泊"
                self.des_hdg = self.ahrs.heading_deg
                return
            self._navigate_toward_wp(dt)
            self.des_hdg = self.wp.wp_bearing_deg
            self.phase = "approach"
            self.decision = "点到点: 沿航线贴线，偏差大时先原地转再前进"
            self.arrived = 0
        elif mode == "fixed_point":
            if not self._in_loiter:
                self._start_loiter(self.ahrs.n, self.ahrs.e, self.ahrs.heading_deg, use_current=True)
            self._update_loiter(dt)
        elif mode == "cruise_speed":
            # 与船上 CRUISE_SPEED 一致：开环油门，不读对地速度
            self.throttle = sat(self._cruise_pct / 100.0, 0.0, 1.0)
            self.move_out = int(self._cruise_pct)
            self.steer_out = 0
            self.steering_y = 0.0
            self.des_hdg = self.ahrs.heading_deg
            self.phase = "cruise_speed"
            self.decision = "定速巡航开环: 滑条直接出油门, 转向 0, 不跟对地速度"
            self.arrived = 0
        elif mode == "cruise_dir":
            y = self.atc.get_steering_out_heading(
                self._psi_des_rad, self.ahrs.yaw_rad(), self.ahrs.yaw_rate_rads, self.ahrs.u_meas, dt
            )
            self._set_steering(y)
            self._calc_throttle(self._u_des, dt)
            self.des_hdg = wrap360(math.degrees(self._psi_des_rad))
            self.phase = "cruise_dir"
            self.decision = "Guided HeadingAndSpeed: 锁航向, 不看横偏"
            self.arrived = 0

    def get_hold_trace(self) -> dict:
        return {
            "in_hold": self._in_loiter,
            "nav_dist": float(self._loiter_dist if self._in_loiter else self.wp.distance),
            "into_heading": float(self.des_hdg),
            "into_valid": self._in_loiter and self._loiter_dist > LOIT_RADIUS,
            "hold_phase": 1 if self._in_loiter else 0,
            "desired_heading": float(self.des_hdg),
            "c_mag": 0.0,
        }
