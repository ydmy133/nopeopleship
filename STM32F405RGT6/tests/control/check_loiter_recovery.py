"""用当前真实控制模块核对稳泊锚点恢复行为，不修改固件。

每个场景独立进程，隔离控制器的函数静态状态。模拟有效性/命令时间
而非模拟 UART 波形；检查真正存储的锚点及对外控制快照。
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
harness = r'''
#include "control_manager.h"
#include "boat_modes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const double A_LAT=22.976105, A_LON=113.3780845;
static const double B_LAT=22.976205, B_LON=113.3781845;
static void step(SensorSnapshot *s, const PCCommand_t *cmd, unsigned now,
                 long long age, ControlSnapshot *out) {
    s->heading_ok_ms=now;
    if(s->gnss_seen) s->gnss_ok_ms=now;
    ControlManager_Step(s,cmd,age,now,0,out);
}
static void anchor(double lat,double lon) {
    double actual_lat,actual_lon;
    BoatModes_LoiterAnchor(&actual_lat,&actual_lon);
    assert(actual_lat==lat && actual_lon==lon);
}
int main(int argc,char **argv) {
    int scenario=argc>1?atoi(argv[1]):0;
    SensorSnapshot s={0}; PCCommand_t cmd={0}; ControlSnapshot out={0};
    s.gnss.latitude=A_LAT; s.gnss.longitude=A_LON;
    s.gnss_seen=s.heading_seen=1;
    s.heading.yaw=37; s.battery=80; s.battery_valid=1;
    cmd.control_mode=CONTROL_MODE_STABLE_ANCHOR;
    cmd.move_speed=37; cmd.dc_prot=0; cmd.lb_prot=0;
    ControlManager_Init();
    if(scenario==2) {
        s.gnss_seen=0; s.gnss.latitude=s.gnss.longitude=0;
        step(&s,&cmd,100,0,&out); anchor(0,0);
        assert(out.move==0 && out.steer==0);
        s.gnss_seen=1; s.gnss.latitude=B_LAT; s.gnss.longitude=B_LON;
        step(&s,&cmd,200,0,&out); anchor(0,0);
        assert(out.boat.target_lat==0 && out.boat.target_lon==0);
        puts("REPRODUCED initial no-fix entry: anchor is (0,0), valid recovery does not repair it.");
        return 0;
    }
    step(&s,&cmd,100,0,&out); anchor(A_LAT,A_LON);
    if(scenario==0) {
        s.gnss_seen=0; s.gnss.latitude=0; s.gnss.longitude=0;
        /* 模拟 NO_FIX 并持续接收同模式有效命令；失效时必须零输出且保留锚点。 */
        for(unsigned now=200;now<=7000;now+=100) {
            step(&s,&cmd,now,0,&out);
            anchor(A_LAT,A_LON);
            assert(out.effective.control_mode==CONTROL_MODE_STABLE_ANCHOR);
            assert(out.move==0 && out.steer==0);
        }
        s.gnss_seen=1; s.gnss.latitude=B_LAT; s.gnss.longitude=B_LON;
        step(&s,&cmd,7100,0,&out); anchor(A_LAT,A_LON);
        assert(out.boat.target_lat==A_LAT && out.boat.target_lon==A_LON);
        printf("PASS GNSS-only recovery: position=(%.7f,%.7f), anchor=(%.7f,%.7f) retained.\n",
               B_LAT,B_LON,out.boat.target_lat,out.boat.target_lon);
    } else {
        /* 默认策略 + dc_prot=0：命令年龄严格超过 5200 ms 切手动并停输出。 */
        s.gnss_seen=0;
        step(&s,NULL,5401,5301,&out);
        assert(out.effective.control_mode==CONTROL_MODE_MANUAL);
        assert(out.move==0 && out.steer==0); anchor(A_LAT,A_LON);
        s.gnss_seen=1; s.gnss.latitude=B_LAT; s.gnss.longitude=B_LON;
        step(&s,&cmd,5501,0,&out); anchor(B_LAT,B_LON);
        assert(out.boat.target_lat==B_LAT && out.boat.target_lon==B_LON);
        printf("REPRODUCED command timeout then stable_anchor: anchor moved from A to B=(%.7f,%.7f).\n",
               out.boat.target_lat,out.boat.target_lon);
    }
    return 0;
}
'''
vcvars = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
includes = ['App/Config','App/Control','App/Control/Boat','App/State','App/Sensors','App/Protocol','App/Safety']
sources = [root/'App/Control/control_manager.c', root/'App/Safety/sensor_protection.c',
           root/'App/Safety/command_protection.c', *sorted((root/'App/Control/Boat').glob('*.c'))]
with tempfile.TemporaryDirectory(prefix='usv_loiter_recovery_') as directory:
    work=Path(directory)
    (work/'check.c').write_text(harness,encoding='utf-8')
    command='cl /nologo /std:c11 /utf-8 /D_CRT_SECURE_NO_WARNINGS '+ ' '.join('/I"'+str(root/p)+'"' for p in includes)
    command+=' '+ ' '.join('"'+str(p)+'"' for p in [work/'check.c',*sources])+' /Fe:check.exe'
    (work/'build.cmd').write_text('@echo off\ncall "'+str(vcvars)+'" >nul\nif errorlevel 1 exit /b 1\n'+command+'\n',encoding='utf-8')
    built=subprocess.run(['cmd','/c',str(work/'build.cmd')],cwd=work,capture_output=True)
    if built.returncode:
        raise RuntimeError((built.stdout+built.stderr).decode('utf-8',errors='replace'))
    outputs=[]
    for scenario in range(3):
        result=subprocess.run([str(work/'check.exe'),str(scenario)],cwd=work,capture_output=True,text=True)
        assert result.returncode==0,(scenario,result.stdout,result.stderr)
        outputs.append(result.stdout)
output=''.join(outputs)
print(output,end='')
(root/'docs/validation/loiter_recovery.log').write_text(output,encoding='utf-8')
