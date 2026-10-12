# 联合测试

test_firmware.c 编译实际平台、传感器、协议、共享状态、保护、驱动和控制模块。运行入口为 python tests/run_host_tests.py。HAL/RTOS 替身是确定性的事件模拟，不证明真实 NVIC 或调度器时序。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
