"""Compile actual main.c sensor/guard functions with simulated sensors.

Run in an x64 Visual Studio Developer shell: python tests/test_sensor_protection.py
No hardware or firmware files are written by this test.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Core/Src/main.c').read_text(encoding='utf-8')

def function(name):
    import re
    match = re.search(r'static [^\n]+ ' + name + r'\([^)]*\)\s*\{', source)
    assert match, name
    start = match.start()
    pos = source.index('{', start)
    depth = 1
    end = pos + 1
    while depth:
        if source[end] == '{': depth += 1
        if source[end] == '}': depth -= 1
        end += 1
    return source[start:end]

prelude = r'''
#include <assert.h>
#include <stdio.h>
#include "protocol.h"
#include "gnss_sensor.h"
#include "heading_sensor.h"
#include "bms_sensor.h"
#define SENSOR_ANOMALY_LIMIT 3
#define BMS_UPDATE_INTERVAL_MS 1000
static uint32_t ticks;
static PCCommand_t s_command;
static ControlMode_t s_last_mode;
static GNSSSensor_Data_t s_gnss, sample;
static HeadingSensor_Data_t s_heading;
static GNSSSensor_Status_t s_gnss_status, next_gnss = GNSS_SENSOR_NO_FIX;
static HeadingSensor_Status_t s_heading_status, next_heading = HEADING_SENSOR_OK;
static uint8_t s_gnss_valid, s_heading_valid, s_battery_valid;
static uint8_t s_return_home_active, s_low_battery_return_active;
static float s_battery_level;
static int8_t s_move_speed, s_steer_speed;
uint32_t HAL_GetTick(void) { return ticks; }
GNSSSensor_Status_t GNSSSensor_Update(void) { return next_gnss; }
GNSSSensor_Data_t GNSSSensor_GetData(void) { return sample; }
HeadingSensor_Status_t HeadingSensor_Update(void) { return next_heading; }
HeadingSensor_Data_t HeadingSensor_GetData(void) { return s_heading; }
BMSSensor_Status_t BMSSensor_Update(void) { return BMS_SENSOR_OK; }
uint8_t BMSSensor_GetData(void) { return 50; }
'''
tests = r'''
static void step(unsigned count) {
    while(count--) { ticks += 50; System_UpdateSensors(); System_ApplySensorProtection(); }
}
int main(void) {
    const ControlMode_t open[] = {CONTROL_MODE_MANUAL, CONTROL_MODE_CRUISE_SPEED};
    const ControlMode_t closed[] = {CONTROL_MODE_NAVIGATE, CONTROL_MODE_STABLE_ANCHOR,
        CONTROL_MODE_CRUISE_DIR, CONTROL_MODE_TRACK_HOLD};
    unsigned i;
    s_command.control_mode=CONTROL_MODE_MANUAL;
    System_SetSpeed(-15,-20);
    assert(s_move_speed==-15 && s_steer_speed==-20);
    step(40); assert(s_move_speed==-15 && s_steer_speed==-20);
    for(i=0;i<4;i++) {
        s_command.control_mode=closed[i]; System_SetSpeed(-15,20);
        assert(s_move_speed==0);
    }
    s_command.control_mode=CONTROL_MODE_CRUISE_SPEED;
    System_SetSpeed(-15,0); assert(s_move_speed==0);
    s_command.control_mode=CONTROL_MODE_MANUAL;
    s_return_home_active=1; System_SetSpeed(-15,0); assert(s_move_speed==0);
    s_return_home_active=0; s_low_battery_return_active=1;
    System_SetSpeed(-15,0); assert(s_move_speed==0); s_low_battery_return_active=0;
    puts("PASS: manual reverse allowed; automatic modes and return prohibit reverse");
    for(i=0;i<2;i++) {
        s_command.control_mode=s_last_mode=open[i];
        System_SetSpeed(15,-20); step(40);
        assert(!s_gnss_valid && s_heading_valid);
        assert(s_command.control_mode==open[i] && s_last_mode==open[i]);
        assert(s_move_speed==15 && s_steer_speed==-20);
        next_gnss=GNSS_SENSOR_ERR_UART; next_heading=HEADING_SENSOR_TIMEOUT;
        step(40);
        assert(s_move_speed==15 && s_steer_speed==-20);
        next_gnss=GNSS_SENSOR_NO_FIX; next_heading=HEADING_SENSOR_OK;
    }
    puts("PASS: manual/open-loop cruise retain output during no-fix and sensor faults");
    for(i=0;i<4;i++) {
        s_command.control_mode=s_last_mode=closed[i];
        s_command.target_lat=22.976105; s_command.target_lon=113.3780845;
        System_SetSpeed(15,20); step(4);
        assert(s_move_speed==0 && s_steer_speed==0);
        assert(s_command.control_mode==closed[i] && s_last_mode==closed[i]);
        assert(s_command.target_lat==22.976105 && s_command.target_lon==113.3780845);
        next_gnss=GNSS_SENSOR_OK; sample.latitude=22.976105; sample.longitude=113.3780845;
        step(4); assert(s_gnss_valid && s_heading_valid);
        System_SetSpeed(15,20); System_ApplySensorProtection();
        assert(s_move_speed==15 && s_steer_speed==20);
        next_heading=HEADING_SENSOR_TIMEOUT; step(12);
        assert(s_move_speed==0 && s_steer_speed==0);
        assert(s_command.control_mode==closed[i]);
        next_heading=HEADING_SENSOR_OK; step(4);
        /* NO_FIX immediately invalidates a previously valid fix, even when data is retained. */
        next_gnss=GNSS_SENSOR_NO_FIX; System_SetSpeed(15,20); step(2);
        assert(!s_gnss_valid && s_move_speed==0 && s_steer_speed==0);
    }
    puts("PASS: closed-loop modes stop on missing fix/heading and retain mode/target on recovery");
    s_command.control_mode=CONTROL_MODE_MANUAL;
    s_return_home_active=1; System_SetSpeed(15,20); System_ApplySensorProtection();
    assert(s_move_speed==0 && s_steer_speed==0 && s_return_home_active);
    s_return_home_active=0; s_low_battery_return_active=1;
    System_SetSpeed(15,20); System_ApplySensorProtection();
    assert(s_move_speed==0 && s_steer_speed==0 && s_low_battery_return_active);
    puts("PASS: automatic return retains sensor protection with a manual command");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='usv_sensor_protection_') as temp:
    work = Path(temp)
    code = work/'sensor_protection.c'
    code.write_text(prelude + '\n' + '\n'.join(function(n) for n in [
        'System_SetSpeed', 'System_ClosedLoop', 'System_UpdateSensors', 'System_ApplySensorProtection']) + tests, encoding='utf-8')
    subprocess.run(['cl', '/nologo', '/utf-8', '/W4',
        '/I'+str(root/'tests/gnss_receive/mocks'), '/I'+str(root/'User'),
        str(code), '/Fe:'+str(work/'test.exe'), '/Fo:'+str(work/'test.obj')], cwd=work, check=True)
    subprocess.run([str(work/'test.exe')], check=True)
