"""Exercise actual C speed and yaw controllers; simulations are not sea trials."""
import ctypes
import itertools
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path
from full_scan import ROOT
from speed_bench import OLD, SpeedController, simulate

sys.path.insert(0,str(ROOT.parent/'20261008_稳泊转向诊断'))
from verify_c_yaw import run as run_yaw

SOURCE=Path('E:/无人船/STM32F405RGT6/App/Control/Boat')
VCVARS='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
HARNESS=r'''
#include "boat_attitude.h"
#include "boat_vehicle.h"
#include "boat_config.h"
__declspec(dllexport) void YawReset(void) { BoatAttitude_Init(); }
static void sense(float forward, float error, float rate, unsigned int now) {
    BoatVehicle v = {0};
    v.heading_deg=-error;v.yaw_rate_rad=rate*BOAT_DEG2RAD;v.forward_mps=forward;
    v.speed_valid=v.heading_valid=v.pos_valid=1;v.now_ms=now;BoatVehicle_Set(&v);
}
__declspec(dllexport) float YawStep(float error,float rate,float dt,unsigned int now) {
    sense(0,error,rate,now);return BoatAttitude_SteerFromHeading(0,0,dt);
}
__declspec(dllexport) float SpeedStep(float target,float speed,float dt,unsigned int now) {
    sense(speed,0,0,now);
    return target<=0.000001f ? BoatAttitude_ThrottleStop(dt) : BoatAttitude_ThrottleFromSpeed(target,dt);
}
'''


def build(label,src):
    dest=ROOT/'c_verification'/label;dest.mkdir(parents=True,exist_ok=True)
    for f in src.iterdir():
        if f.suffix in ('.c','.h'):shutil.copy2(f,dest/f.name)
    (dest/'harness.c').write_text(HARNESS,encoding='utf-8')
    cmd=f'@echo off\ncall "{VCVARS}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /LD /O2 /utf-8 harness.c boat_attitude.c boat_pid.c boat_math.c boat_vehicle.c /link /OUT:control.dll\n'
    (dest/'build.cmd').write_text(cmd,encoding='ascii')
    result=subprocess.run(['cmd','/d','/c','build.cmd'],cwd=dest,capture_output=True)
    (dest/'compile.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(result.stderr.decode(errors='replace'))
    dll=ctypes.CDLL(str(dest/'control.dll'))
    dll.YawReset.argtypes=[];dll.YawReset.restype=None
    dll.YawStep.argtypes=dll.SpeedStep.argtypes=[ctypes.c_float,ctypes.c_float,ctypes.c_float,ctypes.c_uint]
    dll.YawStep.restype=dll.SpeedStep.restype=ctypes.c_float
    return dll


class CSpeed:
    def __init__(self,dll):self.dll=dll;dll.YawReset()
    def update(self,target,speed,dt,now):
        u=self.dll.SpeedStep(target,speed,dt,1000+round(now*1000))
        assert math.isfinite(u) and 0<=u<=1
        return math.floor(u*100+.5)


def equivalent(dll,p):
    py=SpeedController(p);c=CSpeed(dll);max_diff=0
    for i in range(700):
        t=(i+1)*.1;target=.4 if i<200 else .2 if i<400 else 0
        measured=max(0,.3+.2*math.sin(t*.4)) if i<550 else 0
        max_diff=max(max_diff,abs(py.update(target,measured,.1,t)-c.update(target,measured,.1,t)))
    assert max_diff<=1,max_diff
    return dict(samples=700,max_quantized_difference=max_diff)


def main():
    chosen=json.loads((ROOT/'selected_speed_params.json').read_text())
    old=build('before_speed',ROOT/'before_source');new=build('current',SOURCE)
    original=build('original_yaw',ROOT.parent/'20261008_稳泊转向诊断/baseline_source')
    models=[dict(tau=tau,gain=gain,delay=delay,bias=bias,deadband=db,dt=dt)
            for tau,gain,delay,bias,db,dt in itertools.product((.5,2,4,8),(1.1,2,3.6,4.5),(0,.5),(-.03,.03),(0,.1),(.1,.2))]
    result={'speed':{},'yaw':{},'speed_equivalence':{'before':equivalent(old,OLD),'current':equivalent(new,chosen)}}
    for label,dll,p in [('before',old,OLD),('current',new,chosen)]:
        vals=[simulate(p,m,controller=CSpeed(dll))[0] for m in models]
        result['speed'][label]=dict(scenarios=len(vals),mean_iae=sum(v['iae'] for v in vals)/len(vals),
              worst_iae=max(v['iae'] for v in vals),worst_overspeed=max(v['peak_overspeed_mps'] for v in vals),
              worst_hold_error=max(v['last_hold_mean_error'] for v in vals),
              mean_output_variation=sum(v['output_total_variation'] for v in vals)/len(vals))
        for gain in (1.1,3.6):
            m=dict(tau=2,gain=gain,delay=.2,bias=0,deadband=.05,dt=.1)
            met,trace=simulate(p,m,controller=CSpeed(dll),trace=True)
            (ROOT/f'speed_{label}_gain{gain}.json').write_text(json.dumps(trace),encoding='utf-8')
    fitted=json.loads((ROOT/'full_model_fit.json').read_text())['yaw_top_fits']
    for label,dll in [('original',original),('current',new)]:
        tests=[]
        for m,angle,dt,delay in itertools.product(fitted,(-179,-45,-10,10,45,179),(.1,.2,.3),(0,.2)):
            plant={k:m[k] for k in ('tau','gain','delay','bias','deadband')};plant['bias']=-plant['bias']
            metric,_=run_yaw(dll,plant,target=angle,dt=dt,sensor_delay=delay)
            tests.append(metric)
        result['yaw'][label]=dict(scenarios=len(tests),unsettled=sum(m['settling_s'] is None for m in tests),
                worst_overshoot=max(m['overshoot_deg'] for m in tests),worst_last30_error=max(m['last30_peak_error'] for m in tests))
        m={k:fitted[0][k] for k in ('tau','gain','delay','bias','deadband')};m['bias']=-m['bias']
        result['yaw'][label]['nominal'],trace=run_yaw(dll,m)
        (ROOT/f'yaw_{label}.json').write_text(json.dumps(trace),encoding='utf-8')
    (ROOT/'joint_c_verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
