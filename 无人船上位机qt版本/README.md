# 无人船控制系统（Qt / C++）

Qt Quick 上位机：进程内直连 **MQTT** 或 **UDP**，协议为《无人船通信协议》UTF-8 JSON。不再依赖 Electron / Spring Boot。

## 功能

- 全屏 HUD：重点船舶系统地图背景（天地图/卫星图）、手动十字舵、航向条、罗盘、SOG/COG/HDG、经纬 DMM、北斗在线状态
- 控制模式：`manual` / `cruise_speed` / `cruise_dir` / `track_hold` / `stable_anchor` / `navigate`
- 约 5Hz 遥测接收与空闲心跳（含 `dc_prot` / `lb_prot` 与返航点）
- 预置位 1–255（SQLite），到达判定：`nav_reached=1` 且距离 ≤ 1.5 m
- 设置页运行时切换 MQTT / UDP；地图图层可切换天地图 / 卫星图

地图底图联网请求广东省重点船舶服务系统的 `kinth-map` 瓦片（默认 `http://210.76.81.182`，无需高德/百度 Key、无需登录 token）。现场需能访问该地址；AIS 等业务接口本期未接。

## 依赖

- Qt **6.5+**（也可用 Qt **5.15**）
- 模块：Core、Gui、Quick、Qml、Network、Sql
- Windows：MSVC 2019/2022 或 MinGW；CMake 3.16+
- MQTT 为内置 3.1.1 客户端（`QTcpSocket` / `QSslSocket`），**不需要** Qt MQTT 组件

## 编译（Windows / VS 2022）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=D:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

使用官方 Qt（MSVC 64 位），不要用 Anaconda 的 Qt。现场打包：

```bat
D:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release --qmldir qml build\bin\Release\usv_gcs.exe
```

产物：`build/bin/Release/usv_gcs.exe`（需与 Qt 运行库同目录，可用 `windeployqt --qmldir qml` 部署）。

把打包目录拷到现场上位机、装运行库、配防火墙和 MQTT/UDP 的步骤见 **[部署说明.md](部署说明.md)**。

## 联调

### MQTT（默认）

设置页填写 Broker、用户名、密码、船号。平台账号与现有后端一致时：

- 发布：`usv/{boatId}/cmd/control`
- 订阅：`usv/+/telemetry/#`
- Client ID 形如 `usv-qt-gcs-xxxxxxxx`，避免与 Java 后端互踢

可用 MQTTX：船端账号订阅 `usv/usv_001/cmd/#`，平台账号向 `usv/usv_001/telemetry/state` 发遥测 JSON。

### UDP

默认本机 `192.168.1.81:10193` 收，发往 `192.168.1.80:10123`。指定地址绑定失败时回退 `0.0.0.0`。报文为控制/遥测 **payload JSON**（无帧头）。

### 下行控制 JSON

```json
{
  "control_mode": "manual",
  "move_speed": 0,
  "steer_speed": 0,
  "target_lat": 0.0,
  "target_lon": 0.0,
  "return_lat": 0.0,
  "return_lon": 0.0,
  "dc_prot": 1,
  "lb_prot": 1
}
```

### 上行遥测 JSON（约 5Hz / 200ms）

```json
{
  "data_valid": 1,
  "nav_reached": 0,
  "heading": 12.30,
  "battery_level": 0,
  "speed": 0.35,
  "gnss_yaw": 15.20,
  "gz": -1.80,
  "raw_lat": 31.2304000,
  "raw_lon": 121.4737000,
  "target_lat": 31.2304160,
  "target_lon": 121.4737010,
  "dist_error": 8.20,
  "heading_error": -4.50
}
```

兼容 envelope（`proto_ver/seq/ts_ms/payload/crc32`）以及旧字段 `dev_lat` / `dev_lon`。

低电强制解锁：设置页「强制退出低电保护」会下发 `move_speed=20`。

## 快捷键

| 键 | 作用 |
|----|------|
| WASD / 方向键 | 手动前进/后退/左转/右转 |
| Space | 停止 |
| Ctrl+Alt+Q | 退出 |
| F11 / Esc | 全屏切换；设置页中 Esc 返回 |

配置、预置位库和日志在 exe 同级的 `data` 目录（`usv_gcs.ini` / `usvdb.sqlite` / `usv_gcs.log`）。**不要把 Broker 密码提交进仓库。**
