# 架构与维护入口

完整方案、目录树、各文件夹职责、迁移表、任务表、传感器流程以及 PID/前馈/限幅/低通流程见 [总方案](../../../STM32F405RGT6_FreeRTOS重构方案.md)。

源码阅读顺序、中文注释范围和当前实现细节见 [源码注释导读](源码注释导读.md)。

App/Tasks 只放启动和循环调度。App/Control 独占 BoatApp 状态；App/State 管理跨任务快照和命令队列。Platform 统一 HAL UART 回调，每个 RX 只有一个管理者。App/Sensors、App/Protocol、App/Actuators 各自实现协议；算法和参数统一迁入 App/Control/Boat。具体文件职责及迁移取舍见 [控制模块说明](../../App/Control/README.md)。

Keil 只编译选定 M4F 端口，无 CMSIS-RTOS 包装，无 heap 文件；中断优先级 6 可调用 FromISR，内核 SysTick/PendSV/SVC 由 FreeRTOS 端口提供，TIM6 提供 HAL 时间。

不要把 tests/fixtures/legacy 加入固件工程。它保存的是重构前的测试基准，可能包含旧轮询路径、旧目录和参数注释；当前行为以编译中的 App 为准。
