"""Compare the actual pre-refactor control functions to the new module.

Separate executables isolate the unchanged BoatApp static state. The baseline
uses recorded legacy source, not a reimplementation of the expected controller.
"""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'tests/fixtures/legacy/main.c').read_text(encoding='utf-8')
def function(name):
    m=re.search(r'static [^\n]+ '+name+r'\([^)]*\)\s*\{',source);assert m,name
    begin=source.index('{',m.start());end=begin+1;depth=1
    while depth:
        if source[end]=='{':depth+=1
        if source[end]=='}':depth-=1
        end+=1
    return source[m.start():end]
names=['System_SetSpeed','System_ClosedLoop','System_SpeedPercent','System_FillSense','System_ProcessCommand','System_CheckAutoReturn','System_UpdateBoat','System_ApplySensorProtection']
variables=source[source.index('static int8_t  s_move_speed;'):source.index('/* USER CODE END PV */')]
prototypes='\n'.join(function(n).split('{',1)[0].strip()+';' for n in names)
baseline='''#include "control_manager.h"
#include "sensor_protection.h"
#include "command_protection.h"
#include "app_config.h"
#include <string.h>
#define NAV_UPDATE_INTERVAL_MS 100
#define MODE_NAMES_COUNT 7
#define printf(...) ((void)0)
'''+variables+'''
static uint32_t now_ms; static int64_t age_ms; static uint8_t pending;
static uint32_t HAL_GetTick(void) {return now_ms;}
static uint8_t Protocol_IsTimeout(void) {return age_ms>=0 && age_ms>5200;}
static uint8_t Protocol_GetLatestCommand(PCCommand_t *cmd) {if(!pending)return 0;*cmd=s_rx_command;pending=0;return 1;}
'''+prototypes+'\n'+'\n'.join(function(n) for n in names)+'''
void ControlManager_Init(void) {BoatApp_Init();s_rx_command.control_mode=CONTROL_MODE_INVALID;}
void ControlManager_Step(const SensorSnapshot *s,const PCCommand_t *cmd,int64_t age,uint32_t now,uint8_t fault,ControlSnapshot *out) {
    now_ms=now;age_ms=age;s_gnss=s->gnss;s_heading=s->heading;s_battery_level=s->battery;
    s_gnss_valid=SensorProtection_GNSSValid(s,now);s_heading_valid=SensorProtection_HeadingValid(s,now);
    if(cmd) {s_rx_command=*cmd;pending=1;}
    System_ProcessCommand();System_CheckAutoReturn();System_UpdateBoat();System_ApplySensorProtection();
    if(s_disconnect_stopped || fault)System_SetSpeed(0,0);
    out->received=s_rx_command;out->effective=s_command;out->boat=s_boat;out->move=s_move_speed;out->steer=s_steer_speed;
    out->returning=s_return_home_active||s_low_battery_return_active;
}
'''
harness=r'''
#include "control_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(int argc,char **argv) {
    int scenario=argc>1?atoi(argv[1]):0;ControlSnapshot out={0};SensorSnapshot s={0};PCCommand_t cmd={0};
    s.gnss.latitude=22.976105;s.gnss.longitude=113.3780845;s.gnss.speed=2.0f;s.heading.yaw=37;
    s.gnss_seen=s.heading_seen=1;s.battery=80;s.battery_valid=1;
    cmd.control_mode=scenario==0?CONTROL_MODE_MANUAL:scenario==1?CONTROL_MODE_CRUISE_SPEED:
        scenario==3?CONTROL_MODE_STABLE_ANCHOR:scenario==4?CONTROL_MODE_CRUISE_DIR:scenario==5?CONTROL_MODE_TRACK_HOLD:CONTROL_MODE_NAVIGATE;
    cmd.move_speed=scenario==0?-15:37;cmd.steer_speed=20;cmd.target_lat=s.gnss.latitude+0.0001;cmd.target_lon=s.gnss.longitude;
    cmd.return_lat=s.gnss.latitude+0.0002;cmd.return_lon=s.gnss.longitude+0.0001;cmd.dc_prot=scenario==6;cmd.lb_prot=scenario==7;
    if(scenario==8) {cmd.target_lat=s.gnss.latitude;cmd.target_lon=s.gnss.longitude;}
    ControlManager_Init();
    for(unsigned i=0;i<1000;i++) {
        uint32_t now=i*10;const PCCommand_t *incoming=(i%10==0)?&cmd:NULL;int64_t age=0;
        s.gnss_ok_ms=s.heading_ok_ms=now;
        if(scenario==0 && i==100)cmd.move_speed=20;
        if(scenario==1 && i==100)cmd.move_speed=51;
        if(scenario==2 && i==300)s.gnss.latitude=cmd.target_lat;
        if(scenario==3 && i==200)s.gnss.latitude+=0.00008;
        if(scenario==3 && i==400)s.heading.yaw=180;
        if(scenario==4 && i==200)cmd.move_speed=51;
        if(scenario==4 && i==300)s.heading.yaw=40;
        if(scenario==5 && i==200) {
            s.gnss.latitude+=160.0*cos(37.0*3.141592653589793/180.0)/111319.49;
            s.gnss.longitude+=160.0*sin(37.0*3.141592653589793/180.0)/(111319.49*cos(22.976105*3.141592653589793/180.0));
        }
        if(scenario==6 && i>100) {incoming=NULL;age=(int64_t)now-1000;}
        if(scenario==7 && i==100)s.battery=5;
        if(scenario==7 && i==300)cmd.move_speed=20;
        if(scenario==9) {if(i==200)s.gnss_seen=0;if(i==300)s.gnss_seen=1;}
        if(scenario==10 && i>100) {incoming=NULL;age=(int64_t)now-1000;}
        if(scenario==11 && i==300)cmd.target_lon+=0.0001;
        ControlManager_Step(&s,incoming,age,now,0,&out);
        /* Match the periodic task's no-command maintenance pass. */
        ControlManager_Step(&s,NULL,age,now,0,&out);
        printf("%u %d %d %d %d %u %.9f %.9f %.6f %.6f %.6f\n",now,out.move,out.steer,out.effective.control_mode,out.received.control_mode,out.returning,
            out.boat.target_lat,out.boat.target_lon,out.boat.distance_m,out.boat.heading_error_deg,out.boat.cross_track_m);
    }
    return 0;
}
'''
vcvars='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
includes=['App/Config','App/Control','App/State','App/Sensors','App/Protocol','App/Safety','App/Control/Boat']
with tempfile.TemporaryDirectory(prefix='usv_control_baseline_') as temp:
    work=Path(temp);(work/'baseline.c').write_text(baseline,encoding='utf-8');(work/'harness.c').write_text(harness,encoding='utf-8')
    common=[root/'App/Safety/sensor_protection.c',root/'App/Safety/command_protection.c',*sorted((root/'App/Control/Boat').glob('*.c'))]
    commands=[]
    for name,manager in [('baseline',work/'baseline.c'),('current',root/'App/Control/control_manager.c')]:
        commands.append('cl /nologo /std:c11 /utf-8 /D_CRT_SECURE_NO_WARNINGS '+' '.join('/I"'+str(root/x)+'"' for x in includes)+' '+' '.join('"'+str(x)+'"' for x in [work/'harness.c',manager,*common])+' /Fe:'+name+'.exe')
    script=work/'build.cmd';script.write_text('@echo off\ncall "'+vcvars+'" >nul\n'+'\nif errorlevel 1 exit /b 1\n'.join(commands)+'\n',encoding='utf-8')
    build=subprocess.run(['cmd','/c',str(script)],cwd=work,capture_output=True)
    (root/'docs/validation/control_build.log').write_text((build.stdout+build.stderr).decode('utf-8',errors='replace'),encoding='utf-8')
    if build.returncode:raise RuntimeError((build.stdout+build.stderr).decode(errors='replace'))
    for scenario in range(12):
        before=subprocess.check_output([str(work/'baseline.exe'),str(scenario)],cwd=work)
        after=subprocess.check_output([str(work/'current.exe'),str(scenario)],cwd=work)
        assert before==after,f'control behavior differs in scenario {scenario}'
        rows=[line.split() for line in after.decode().splitlines()]
        if scenario in [2,8]:assert rows[-1][3]=='0' and rows[-1][1]=='0','arrival did not return to manual/stop'
        if scenario==5:assert rows[-1][6:8]!=rows[10][6:8],'track was not extended'
text='PASS: 12 scenarios x 1000 steps identical to recorded legacy control functions; all five modes, arrival, duplicate target, target change, heading lock, track extension, sensor loss/recovery, disconnect stop/return and low-battery return cancellation.\n'
print(text.strip());(root/'docs/validation/control_baseline.log').write_text(text,encoding='utf-8')
