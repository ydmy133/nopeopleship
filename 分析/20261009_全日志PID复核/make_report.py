"""Build a self-contained comparison and preserve verification metadata."""
import hashlib
import json
from pathlib import Path
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from fit_full import load, select, surge_score

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parents[1]
read = lambda name: json.loads((ROOT / name).read_text(encoding='utf-8'))
audit = read('full_log_audit.json')
fit = read('full_model_fit.json')
checks = read('joint_c_verification.json')
rows, _ = load()
extra = {name: surge_score(select(rows, start, end), fit['speed_top_fits'][0])
         for name, start, end in [('training_navigation', '16:42:00', '16:45:05'),
                                  ('heldout_manual', '16:50:44', '16:51:15'),
                                  ('heldout_navigation', '16:59:11', '17:00:11')]}
(ROOT / 'speed_cross_segment_validation.json').write_text(json.dumps(extra, indent=2), encoding='utf-8')
fig = make_subplots(rows=4, cols=1, shared_xaxes=False, vertical_spacing=.07,
                    subplot_titles=('179°航向阶跃：实际 C 控制器 + 日志拟合模型',
                                    '航向角速度与转向输出',
                                    '速度：假设低推力模型，未获实船标定',
                                    '速度：假设高推力模型，未获实船标定'))
for label, title, color in [('original', '原始参数', '#d97706'), ('current', '试验参数', '#0284c7')]:
    data = read(f'yaw_{label}.json')
    for row, field, name in [(1, 'error', '航向误差/°'), (2, 'rate', '角速度/°每秒'), (2, 'output', '转向输出/%')]:
        fig.add_trace(go.Scatter(x=[v['t'] for v in data], y=[v[field] for v in data],
                      name=f'{title} {name}', line=dict(color=color, dash='dot' if field=='output' else 'solid')), row=row, col=1)
for row, gain in [(3, 1.1), (4, 3.6)]:
    for label, title, color in [('before', '原速度参数', '#d97706'), ('current', '新速度参数', '#0284c7')]:
        data = read(f'speed_{label}_gain{gain}.json')
        fig.add_trace(go.Scatter(x=[v['t'] for v in data], y=[v['speed'] for v in data], name=f'{title} 速度（增益{gain}）', line=dict(color=color)), row=row, col=1)
    fig.add_trace(go.Scatter(x=[v['t'] for v in data], y=[v['target'] for v in data], name=f'目标速度（增益{gain}）', line=dict(color='#64748b', dash='dash')), row=row, col=1)
    fig.update_yaxes(title_text='速度 m/s', row=row, col=1)
for row in range(1, 5): fig.update_xaxes(title_text='仿真时间 s', row=row, col=1)
fig.update_layout(height=1350, title='全日志 PID 复核：模型仿真对比，未烧录实船验证', template='plotly_white', hovermode='x unified')
fig.write_html(ROOT / '速度与航向PID对比.html', include_plotlyjs=True)
hexfile = PROJECT / 'STM32F405RGT6/MDK-ARM/STM32F405RGT6/STM32F405RGT6.hex'
buildlog = PROJECT / 'STM32F405RGT6/MDK-ARM/joint_pid_20261009_build.log'
config = PROJECT / 'STM32F405RGT6/App/Control/Boat/boat_config.h'
buildtext = buildlog.read_text(encoding='utf-8', errors='replace')
assert '0 Error(s), 0 Warning(s)' in buildtext
metadata = {str(p): {'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'bytes': p.stat().st_size} for p in [config, hexfile, buildlog]}
(ROOT / 'build_verification.json').write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding='utf-8')
report = f'''# 全日志速度与航向 PID 复核（2026-10-09）

已将速度 PI 改为保守试验参数，航向参数经全日志复核后保留上一轮调整。固件编译通过；未烧录、未进行新参数实船测试。航向有较强日志模型支持，速度只有假设模型下的工程筛选支持，不能把两者称为同等可靠的实船标定。

## 数据范围与筛选

本次按全日采集文件 `{audit['source']}` 重新扫描，覆盖 {audit['first']} 至 {audit['last']}，保留 12,530 个完整遥测对象。12,486 个来自结构化记录，44 个来自原始字段中严格解码的完整 JSON。含不完整尾部的 raw 记录不修补，不把零碎字段拼成遥测。解码计数与完整记录存在重叠，不能直接相加为日志行数。

检测到 5 次时钟重启；动力学数据按重启段、uptime 排序，不把重启后的同号 telemetry_seq 当成同一帧。7,447 帧同时带时钟和实际输出，另外 870 帧有输出但无时钟、4,213 帧旧协议数据均保留在审计中，不用于当前动态模型。输入采用 output_move_speed/output_steer_speed，rx_* 仅表示收到的指令。

时钟完整记录中：手动 5,573 帧，稳泊 1,512 帧，导航 223 帧，定速 61 帧，定向 60 帧，航迹保持 18 帧。定向与航迹保持帧全部 data_valid=0、输出为零，不能拿来标定。早期多段输出或可观测运动为零；16:37 左右定速输出 50% 的连续约 28 秒中 GPS speed 仍为零。不能据此推断真实船体速度或推进器增益。

Qt 日志用于核对指令/显示语义；它们的重复遥测不再加入动态拟合，避免重复加权。只有整段审计中的完整、时钟有效、传感器有效记录进入物理模型拟合。全日志扫描不等于把无效和停机记录都强行用于调参。

## 最终参数（源码已修改）

| 控制项 | 原始参数 | 本次保留/采用 |
|---|---:|---:|
| 速度 Kp | 0.20 | 0.30 |
| 速度 Ki | 0.20 | 0.15 |
| 速度 Kd | 0 | 0 |
| 速度积分输出上限 | 1.00 | 0.50 |
| 速度前馈 throttle/speed | 0.70/0.80 | 0.70/0.80 |
| 航向外环 Kp | 2.00 | 0.25 |
| 角速度内环 Kp | 0.20 | 0.30 |
| 角速度内环 Ki | 0.20 | 0.05 |
| 角速度内环 Kd / FF | 0 / 0.20 | 0 / 0.20 |
| 转向积分输出上限 | 1.00 | 0.15 |
| 目标角速度上限 | 40°/s | 10°/s |
| 目标角加速度上限 | 30°/s² | 15°/s² |
| 控制器状态超时 | 200 ms | 500 ms |

航向不是单个 PID，而是航向 P 外环加角速度 PI 内环；Kd 均为零。积分上限是积分分项限幅，不是电机总输出限幅。速度加速度限制仍为 0.25 m/s²，滤波仍为 10 Hz。

速度此次增加即时误差反馈、略降积分并限制积分积累；保持前馈，因为日志无法可靠确定实际推力—速度关系。参考 [ArduPilot 官方速度调参说明](https://ardupilot.org/rover/docs/rover-tuning-throttle-and-speed.html)，前馈需要真实稳定航行速度支撑，PI 调整仍需复测；本项目具体实现以源码为准。

## 航向：整段复核支持继续使用慢转向参数

训练包含 16:38–16:42、16:45:05–16:49:09、16:49:09–16:53:17 多段可观测转向，1,617 个有效拟合比较点；留出 16:57:11–16:59:11 共 191 点。最佳有效转向时间常数约 4 秒，留出角速度 RMSE 3.15°/s、R² 0.953。该时间常数综合船体惯性、驱动与传感器反馈，不能当成磁力计独立延迟。

将日志中最优 8 个模型、6 种正负转角、100/200/300 ms 控制周期、0/200 ms 传感器附加延迟组合，直接调用实际 boat_attitude/boat_pid C 源码进行 288 个场景测试。测试时限 120 秒，收敛判据为航向误差不超过 5°且角速度不超过 2°/s，并持续到测试末尾。原始参数 180 个场景在测试时限内未收敛；当前参数 0 个。当前最差过冲约 10.70°、末 30 秒最大误差约 2.91°。

典型 179°阶跃：原始峰值角速度 34.93°/s、过冲 40.51°、未收敛；当前峰值 6.51°/s、过冲 8.77°、约 60.6 秒进入并保持误差带。代价是大角度转向更慢。模型仿真结果不是实船保证，尤其没有证明运行周期能长期保持低于 500 ms。

## 速度：跨航段模型验证失败，采用有限幅度试验值

速度拟合用 16:39–16:45:05 共 947 帧训练、16:49:09–17:00:11 共 1,326 帧留出，按 GPS speed/COG 构建世界坐标速度并考虑航向变化。最佳模型训练误差虽较小，但全留出 R² 仅 0.200；单独留出导航 R²={extra['heldout_navigation']['r2']:.3f}、手动 R²={extra['heldout_manual']['r2']:.3f}，不具有可信跨航段预测能力。不能拿拟合增益直接修改速度前馈，亦不能宣称已辨识真实船体速度模型。

以 256 个明确假设的模型筛选保守 PI：时间常数 0.5/2/4/8 秒，归一化推力增益 1.1/2/3.6/4.5 m/s，驱动延迟 0/0.5 秒，漂移 ±0.03 m/s，推力死区 0/10%，周期 100/200 ms，测量延迟 200 ms 并叠加噪声。这些范围是工程假设，不是日志识别出的置信区间。候选筛选分数为平均累计绝对误差 + 0.2×最差累计误差 + 10×最差超速，权重体现本轮优先压制超速的选择。

最终再次调用实际 C 源码验证，得到：

| 256 个假设场景指标 | 调整前速度参数 | 本次速度参数 |
|---|---:|---:|
| 平均累计绝对速度误差（m） | 6.359 | 6.271 |
| 最差累计绝对速度误差（m） | 10.948 | 9.649 |
| 最差超速（m/s） | 0.646 | 0.583 |
| 最差低速保持平均误差（m/s） | 0.165 | 0.183 |
| 平均输出累计变化（百分点） | 204.23 | 221.41 |

平均累计误差仅改善约 1.4%，最差累计误差约改善 11.9%，最差超速约改善 9.7%；低速保持误差和输出变化反而增加。这是一项明确取舍，不能说全部性能改善。超速统计剔除目标变化后前三秒，但仍可能包含船体减速惯性；数值不是最高航速。

测试目标依次 0、0.37、0.60、0.20、0 m/s。Python 筛选模型与实际 C 控制器在各 700 个包含启停的样本上，量化输出差为零。模型只验证独立环路，没有验证整套模式切换、GPS 漂移和船体耦合。

## 应用范围和稳泊剩余问题

上述参数属于共享自动控制器，影响调用对应速度/航向环的自动模式。当前定速巡航直接给油门，不调用这里的速度 PI；手动直接输出也不经过这些 PI。

稳泊 2 m 到点判据、出圈后的目标航向重新计算、圈内保留航向的策略均未修改。GPS 漂移越过边界时仍可能触发约 180°目标翻转；PID 放缓可以减少追赶时摆动，但不能消除目标本身跳变造成的原地转向。因此此次调参不能等同于修复全部稳泊策略问题。

建议实船复测先确认遥测显示 output_*，记录目标速度、实际前向速度、目标/实际角速度、积分输出、控制 dt 与 GNSS/航向更新时间；分别验证低速直行和小角度转向，再复测 2 m 边界。速度保持误差若变差，应结合稳定直线数据重新标定前馈，再评估 PI，而非继续凭摆动航段加大积分。

## 构建与复现

Keil 构建日志显示 0 Error(s)、0 Warning(s)。固件：`{hexfile}`。SHA256：`{metadata[str(hexfile)]['sha256']}`。尚未烧录。

- `full_scan.py`：全日志审计与规范化。
- `fit_full.py`：速度及航向训练/留出检验。
- `speed_bench.py`：速度候选假设模型筛选。
- `verify_joint_c.py`：实际 C 控制器联合复核。
- `make_report.py`：细分速度留出验证、交互图与构建记录。
- `before_source/` 保存本次速度修改前源码（已含上一轮航向调整）；原始航向基线保存在前一天诊断目录的 `baseline_source/`。

交互对比图见 `速度与航向PID对比.html`；审计、模型、控制器结果与构建哈希均单独保存为 JSON。图中速度曲线是明确假设模型下的新旧控制对比，不是采集到的新参数实船曲线。
'''
(ROOT / '全日志PID复核说明.md').write_text(report, encoding='utf-8')
print(json.dumps({'speed_validation': extra, 'report': str(ROOT / '全日志PID复核说明.md'), 'hex_sha256': metadata[str(hexfile)]['sha256']}, ensure_ascii=False, indent=2))
