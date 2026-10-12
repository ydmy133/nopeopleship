# FreeRTOS 重构验证记录

日期：2026-10-10。验证对象为本工程当前源文件及 `MDK-ARM/Objects/STM32F405RGT6.axf`。未烧录，未进行台架或实船验证。

## 已通过

| 检查 | 结果与证据 |
|---|---|
| Keil 完整 rebuild | ARMClang 6.24；0 Error(s)、0 Warning(s)。日志在 MDK-ARM/freertos_build_final.log。 |
| 内核与端口 | FreeRTOS V11.2.0、GCC/ARM_CM4F 已编译链接；SVC/PendSV/SysTick 的实际符号来自 port.o。 |
| 工程引用 | 71 个编译源文件，路径存在且无重复；不包含测试基准或 heap 实现。 |
| 算法保留 | 原 User/boat 的全部 19 个 `.c/.h` 文件已迁入 App/Control/Boat 并补中文注释；去除注释/普通空白后的 C token 与冻结原件一致，包含 boat_config；冻结原件 SHA256 保持不变。 |
| 注释修改范围 | App、Platform 和工程 Core 中 86 个 `.c/.h` 文件与补注释前 token 基准一致；保留字符串、运算符、宏和预处理行边界；不修改厂商 system_stm32f4xx.c。 |
| 控制行为 | 12 场景各 1000 步，与记录的真实旧控制函数输出逐字相同；另确认到点回手动/零输出和实际航线延伸发生。 |
| 传感器/传输 | 实际模块经事件替身验证碎片、重复事件、环回、事务溢出、严格 CRC、异常帧、缺头、NO_FIX、超时、TX 终止、UART/DMA 错误及延迟 abort 恢复。 |
| 历史数据 | 550 组 GNSS 寄存器回放，完整 CRC 和航迹角换算一致。这些是寄存器回放帧，不是原始线缆包。 |
| JSON/共享状态 | 分包、连包、转义字符串、必需字段、坐标精度、arena 重用、嵌套上限、大数/非有限值、队列满、计时回绕和截断处理通过。 |
| 实际驱动任务 | 发送期间请求停止，下一帧优先发零；TX 失败不改变上一次完成值/时间。 |
| 实际命令任务 | 缺失 JSON 后半包，500 ms 后丢弃；随后完整命令可恢复。 |
| 遥测数据边界 | 最新解析 rx_* 与控制状态分开，队列溢出也保留接收证据；实际输出只在 TX 完成后更新。 |

主机输出见 [模块联合测试](host_tests.log)、[旧控制对照](control_baseline.log)。机器可读构建及布局结果见 [build_summary.json](build_summary.json)。

## 固件大小与 SRAM

最终构建：Code 54,656 B，RO-data 2,048 B，RW-data 44 B，ZI-data 57,428 B。静态 RAM 总量 57,472 B，约 56.13 KiB；其中包含任务栈、启动栈及库预留 heap。当前配置普通 SRAM 为 128 KiB，静态占用不代表运行栈峰值已经合格。

链接地址：ports（含 DMA 缓冲）`0x20006430`，dma_gnss `0x200048a8`，dma_heading `0x20004908`，cJSON arena `0x200001f0`。全部位于普通 SRAM；未使用 CCM。链接 map 在 `MDK-ARM/Objects/STM32F405RGT6.map`。

## 上板验收：尚未执行

1. 拆开推进输出或使用台架，确认上电零帧、八任务运行、IWDG 和故障复位。
2. 让两个使用浮点运算的任务持续运行并检查结果，确认实际 FPU 上下文切换；主机测试与编译不能替代。
3. USART2/USART3 均为 9600 8N1；检查完整 21/23 字节帧及 CRC，尤其确认此前缺头现象是否消失。保持禁止波特率配置写入。
4. 拔插 GNSS/航向、连续噪声、CRC 错、UART ORE、超时和延迟应答；确认错误原始字节保留、旧数据不刷新、闭环停止且模式/目标保留，恢复后继续。
5. 上位机连续命令、分包、连包及队列溢出；检查 rx_*、有效模式、output_*、输出状态与时间。停止请求不得被后来移动目标跳过。
6. 阻塞控制任务超过 300 ms，测量驱动任务停推帧；确认其他任务停滞导致停止喂 IWDG。正常传感器查询失败不得被误判为死任务。
7. 使用 rtos_diag 检查稳定运行时 CPU 空闲≥30%、各任务栈余量≥25%；测量 100 ms 算法周期最大偏差≤10 ms，确认 DMA HT/TC 服务未被长时间屏蔽。
8. 台架通过后实船逐项验证五功能、到点停止、锚点和航向锁定、持续航线延伸、断连/低电量返航及原取消规则。保持现有 PID 参数，不在迁移验收中混入调参。

复核命令：`python tools/verify_comments.py`、`python tests/run_host_tests.py`、`python tests/control/compare_legacy.py`、`powershell -ExecutionPolicy Bypass -File tools/build_keil.ps1`、`python tools/verify_layout.py`。注释基准为 `comment_baseline.json`，不自动重写。当前文件原始字节因中文注释改变，不能再声称当前源文件 SHA256 与迁移前相同。
