# 传感器采集

完整目录与采集设计见项目外层的 `STM32F405RGT6_FreeRTOS重构方案.md` 第 3 节。

GNSS：USART2 PA2/PA3，9600 8N1，DMA1 Stream5 Channel4。航向：USART3 PB10/PB11，9600 8N1，DMA1 Stream1 Channel4。BMS：USART1，9600 8N1，中断接收。三个设备各有独立任务与单个在途查询。

RX 在 TX 前准备，TX 完成后计 100 ms 应答窗口；4 ms 静默闭合事务。GNSS/航向各 128 字节循环 DMA，事务上限 64 字节。正常 IDLE 不停止 DMA；错误终止完成后才重新启动。GNSS 不接受回显或缺失帧头；航向仅接受完整查询回显后跟完整 23 字节应答。

状态码保留，GNSS 诊断版本为 7。err 位：1 PE、2 NE、4 FE、8 ORE、16 缓冲溢出、32 DMA 故障。只有有效数据刷新成功时间，NO_FIX 立即撤销定位有效性。失败保留旧数值但不刷新时间戳。

主机模拟不能证明 RS485 波形、设备应答延迟、真实 DMA/NVIC 时序。上板先验证原始完整 21/23 字节帧和 CRC，再验证闭环输出。历史 `docs/gnss` 中的轮询说明保留为历史资料。
