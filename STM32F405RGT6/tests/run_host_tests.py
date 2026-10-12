"""编译真实固件模块，以确定性 HAL/RTOS 替身验证事务和保护规则。

先将 19 个算法与冻结原件做 token 对照，再用 MSVC 编译集成检查。
临时目录隔离测试目标；550 个 GNSS 样本是寄存器回放，不是线缆原始包。
日志保存到 docs/validation；事件替身不验证真实内核调度或串口波形。
"""
from pathlib import Path
import subprocess
import tempfile
import sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from source_tokens import verify_algorithms
vcvars=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
assert vcvars.exists(), 'Visual Studio C compiler setup not found'
algorithm_files=verify_algorithms(root)
includes=['tests/mocks','App/Config','App/State','App/Sensors','App/Protocol','App/Control','App/Safety','App/Actuators','App/Diagnostics','Platform/Inc','App/Control/Boat','Middlewares/Third_Party/cJSON']
sources=['tests/mocks/host_runtime.c','tests/integration/test_firmware.c','Platform/Src/board_uart.c','Platform/Src/rx_cursor.c',
         'App/State/app_state.c','App/Safety/sensor_protection.c','App/Safety/command_protection.c','App/Safety/health_policy.c','App/Actuators/drive_output.c','App/Actuators/drive_mailbox.c','App/Tasks/drive_task.c','App/Tasks/command_task.c',
         'App/Sensors/modbus_codec.c','App/Sensors/gnss_sensor.c','App/Sensors/heading_sensor.c','App/Sensors/bms_sensor.c','App/Protocol/protocol.c','App/Protocol/json_framer.c',
         'Middlewares/Third_Party/cJSON/cJSON.c','App/Control/control_manager.c']
sources += [str(p.relative_to(root)).replace('\\','/') for p in sorted((root/'App/Control/Boat').glob('*.c'))]
with tempfile.TemporaryDirectory(prefix='usv_freertos_host_') as temp:
    work=Path(temp)
    command='cl /nologo /std:c11 /utf-8 /W4 /D_CRT_SECURE_NO_WARNINGS /DCJSON_NESTING_LIMIT=16 '+' '.join('/I"'+str(root/x)+'"' for x in includes)+' '+' '.join('"'+str(root/x)+'"' for x in sources)+' /Fe:host_checks.exe'
    script=work/'build.cmd';script.write_text('@echo off\ncall "'+str(vcvars)+'" >nul\nif errorlevel 1 exit /b 1\n'+command+'\n',encoding='utf-8')
    result=subprocess.run(['cmd','/c',str(script)],cwd=work,capture_output=True)
    log=result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace')
    (root/'docs/validation/host_build.log').write_text(log,encoding='utf-8')
    if result.returncode: print(log);sys.exit(result.returncode)
    result=subprocess.run([str(work/'host_checks.exe'),str(root/'tests/fixtures/wit_gps_register_snapshots.bin')],cwd=work,capture_output=True,text=True)
    output=result.stdout+result.stderr;print(output)
    (root/'docs/validation/host_tests.log').write_text(output,encoding='utf-8')
    if result.returncode:sys.exit(result.returncode)
print(f'PASS: all {algorithm_files} App/Control/Boat algorithm tokens match frozen original sources')
