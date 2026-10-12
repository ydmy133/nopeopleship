"""Build and exercise actual baseline/current C yaw controllers on fitted plants."""
import ctypes
import itertools
import json
import math
import shutil
import subprocess
from pathlib import Path
from evaluate_yaw import ROOT, Controller, OLD, effective

SOURCE=Path('E:/无人船/STM32F405RGT6/App/Control/Boat')
VCVARS=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
HARNESS=r'''
#include "boat_attitude.h"
#include "boat_vehicle.h"
#include "boat_config.h"
__declspec(dllexport) void YawReset(void) { BoatAttitude_Init(); }
__declspec(dllexport) float YawStep(float error_deg, float rate_deg, float dt, unsigned int now_ms) {
    BoatVehicle v = {0};
    v.heading_deg = -error_deg;
    v.yaw_rate_rad = rate_deg * BOAT_DEG2RAD;
    v.pos_valid = v.heading_valid = v.speed_valid = 1;
    v.now_ms = now_ms;
    BoatVehicle_Set(&v);
    return BoatAttitude_SteerFromHeading(0.0f, 0.0f, dt);
}
'''


def compile_controller(label, source):
    dest=ROOT/'c_verification'/label
    dest.mkdir(parents=True,exist_ok=True)
    for p in source.iterdir():
        if p.suffix in ('.c','.h'):
            shutil.copy2(p,dest/p.name)
    (dest/'harness.c').write_text(HARNESS,encoding='utf-8')
    cmd=f'@echo off\ncall "{VCVARS}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /LD /O2 /utf-8 harness.c boat_attitude.c boat_pid.c boat_math.c boat_vehicle.c /link /OUT:yaw.dll\n'
    (dest/'build.cmd').write_text(cmd,encoding='ascii')
    result=subprocess.run(['cmd','/d','/c','build.cmd'],cwd=dest,capture_output=True)
    (dest/'compile.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout.decode(errors='replace')+result.stderr.decode(errors='replace'))
    dll=ctypes.CDLL(str(dest/'yaw.dll'))
    dll.YawReset.argtypes=[]
    dll.YawReset.restype=None
    dll.YawStep.argtypes=[ctypes.c_float,ctypes.c_float,ctypes.c_float,ctypes.c_uint]
    dll.YawStep.restype=ctypes.c_float
    return dll


def quantize(x):
    return math.copysign(math.floor(abs(x)*100+.5),x)


def run(dll, model, target=179, dt=.1, sensor_delay=0, duration=120):
    dll.YawReset()
    yaw=rate=0.0
    ys=[yaw];rs=[rate];us=[0.0]
    traces=[]
    inertia=math.exp(-dt/model['tau'])
    hold=round(model['delay']/dt)
    sense=round(sensor_delay/dt)
    now=1000
    for i in range(round(duration/dt)):
        h=ys[max(0,len(ys)-1-sense)];r=rs[max(0,len(rs)-1-sense)]
        now+=round(dt*1000)
        u=quantize(dll.YawStep(target-h,r,dt,now))
        us.append(u)
        applied=us[max(0,len(us)-1-hold)]
        next_rate=inertia*rate+(1-inertia)*(model['gain']*effective(applied,model['deadband'])+model['bias'])
        yaw+=(rate+next_rate)*.5*dt
        rate=next_rate
        ys.append(yaw);rs.append(rate)
        traces.append(dict(t=round((i+1)*dt,3),error=(target-yaw+180)%360-180,rate=rate,output=u,yaw=yaw))
    overshoot=max(0,max(ys)-target) if target>0 else max(0,target-min(ys))
    bad=[v['t'] for v in traces if abs(v['error'])>5 or abs(v['rate'])>2]
    settling=(bad[-1]+dt) if bad and bad[-1]<traces[-1]['t'] else (None if bad else 0)
    metrics=dict(overshoot_deg=overshoot,peak_rate_dps=max(abs(v['rate']) for v in traces),
                 settling_s=settling,last30_peak_error=max(abs(v['error']) for v in traces if v['t']>=duration-30))
    return metrics,traces


def verify_equations(dll,p):
    # Exercise sign reversal, ramp limits, integral state and timeout reset.
    c=Controller(p);dll.YawReset();now=1000;max_diff=0
    for i in range(600):
        dt=(.1,.2,.21)[(i//100)%3]
        error=160*math.sin(i*.031)
        rate=28*math.sin(i*.017)
        now+=round(dt*1000)
        a=c.update(error,rate,dt)
        b=quantize(dll.YawStep(error,rate,dt,now))
        max_diff=max(max_diff,abs(a-b))
    if max_diff>1:
        raise AssertionError(f'Python/C mismatch: {max_diff} percentage points')
    return dict(samples=600,max_quantized_difference=max_diff)


def main():
    chosen=json.loads((ROOT/'selected_yaw_params.json').read_text())
    nominal=json.loads((ROOT/'yaw_candidate_evaluation.json').read_text())['nominal_model']
    baseline=compile_controller('baseline',ROOT/'baseline_source')
    current=compile_controller('current',SOURCE)
    result=dict(python_c_equivalence={'baseline':verify_equations(baseline,OLD),'current':verify_equations(current,chosen)},
                nominal_model=nominal,nominal={},robustness={})
    for label,dll in [('baseline',baseline),('current',current)]:
        result['nominal'][label],trace=run(dll,nominal)
        (ROOT/f'{label}_c_step_trace.json').write_text(json.dumps(trace),encoding='utf-8')
        checks=[]
        for tau,gain,delay,sense,dt,bias in itertools.product((3,4,5),(2.3,2.7,3.1),(0,.3),(0,.2),(.1,.2),(-1,0,1)):
            model=dict(tau=tau,gain=gain,delay=delay,deadband=2,bias=bias)
            m,_=run(dll,model,dt=dt,sensor_delay=sense)
            checks.append(m)
        result['robustness'][label]=dict(scenarios=len(checks),worst_overshoot=max(v['overshoot_deg'] for v in checks),
                worst_last30_error=max(v['last30_peak_error'] for v in checks),
                unsettled=sum(v['settling_s'] is None for v in checks))
        result.setdefault('small_steps',{})[label]={str(angle):run(dll,nominal,target=angle)[0] for angle in (-45,-10,10,45)}
        result.setdefault('slow_cadence',{})[label]={str(dt):run(dll,nominal,dt=dt,sensor_delay=.2)[0] for dt in (.21,.3)}
    (ROOT/'c_yaw_verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    main()
