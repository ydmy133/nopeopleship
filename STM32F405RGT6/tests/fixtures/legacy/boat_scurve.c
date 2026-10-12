#include "boat_scurve.h"

#include "boat_math.h"

#include <math.h>

void BoatScurve_Start(BoatScurve *sc, float length, float v0, float vmax, float amax, float jmax)
{
    if (length < 0.0f) {
        length = 0.0f;
    }
    if (vmax < 0.05f) {
        vmax = 0.05f;
    }
    if (amax < 0.05f) {
        amax = 0.05f;
    }
    if (jmax < 0.05f) {
        jmax = 0.05f;
    }
    sc->length = length;
    sc->v_max = vmax;
    sc->a_max = amax;
    sc->j_max = jmax;
    sc->s = 0.0f;
    sc->v = BoatMath_Constrain(v0, 0.0f, vmax);
    sc->a = 0.0f;
    sc->time_scale = 1.0f;
    sc->finished = 0u;
}

void BoatScurve_SetSpeed(BoatScurve *sc, float vmax)
{
    if (vmax < 0.05f) {
        vmax = 0.05f;
    }
    sc->v_max = vmax;
    if (sc->v > sc->v_max) {
        sc->v = sc->v_max;
    }
}

void BoatScurve_SetLength(BoatScurve *sc, float length)
{
    if (length < sc->s) {
        length = sc->s;
    }
    sc->length = length;
    sc->finished = 0u;
}

void BoatScurve_Advance(BoatScurve *sc, float dt, float time_scale)
{
    float remain;
    float stop_dist;
    float v_target;
    float a_des;
    float jstep;
    float tc;

    if (dt < 0.0f) {
        dt = 0.0f;
    }
    if (dt > 1.0f) {
        dt = 1.0f;
    }
    time_scale = BoatMath_Constrain(time_scale, 0.0f, 1.0f);
    tc = sc->a_max / sc->j_max;
    if (tc < dt) {
        tc = dt;
    }
    if (tc < 0.05f) {
        tc = 0.05f;
    }
    /* 时间缩放慢慢跟上，避免船一落后曲线就突然停住。 */
    sc->time_scale += (time_scale - sc->time_scale) * (dt / tc);
    dt *= sc->time_scale;

    remain = sc->length - sc->s;
    if (remain < 0.0f) {
        remain = 0.0f;
    }
    /* 刹停距离 = v²/2a，再留一段把加速度降下来的余量。 */
    stop_dist = (sc->v * sc->v) / (2.0f * sc->a_max);
    stop_dist += (sc->v * sc->a_max) / sc->j_max;
    if (remain <= 0.05f) {
        v_target = 0.0f;
    } else if (remain <= stop_dist) {
        v_target = sqrtf(2.0f * sc->a_max * remain);
        if (v_target > sc->v_max) {
            v_target = sc->v_max;
        }
    } else {
        v_target = sc->v_max;
    }

    if (dt < 0.02f) {
        a_des = (dt > 0.0f) ? (v_target - sc->v) / 0.02f : 0.0f;
    } else {
        a_des = (v_target - sc->v) / dt;
    }
    a_des = BoatMath_Constrain(a_des, -sc->a_max, sc->a_max);
    jstep = sc->j_max * ((dt > 0.0f) ? dt : 0.02f);
    if (a_des > sc->a + jstep) {
        sc->a += jstep;
    } else if (a_des < sc->a - jstep) {
        sc->a -= jstep;
    } else {
        sc->a = a_des;
    }
    sc->a = BoatMath_Constrain(sc->a, -sc->a_max, sc->a_max);

    sc->v += sc->a * dt;
    /* 曲线速度不为负，这条航段不会规划倒车。 */
    sc->v = BoatMath_Constrain(sc->v, 0.0f, sc->v_max);
    sc->s += sc->v * dt;
    if (sc->s > sc->length) {
        sc->s = sc->length;
    }
    sc->finished = (remain <= 0.05f && sc->v < 0.05f) ? 1u : 0u;
}
