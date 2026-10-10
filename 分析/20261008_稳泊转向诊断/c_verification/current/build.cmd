@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /LD /O2 /utf-8 harness.c boat_attitude.c boat_pid.c boat_math.c boat_vehicle.c /link /OUT:yaw.dll
