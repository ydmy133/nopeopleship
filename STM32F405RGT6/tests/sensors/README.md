# 传感器测试

实际检查在 tests/integration/test_firmware.c 的 sensor_tests 和 replay_test：完整帧、CRC、异常、NO_FIX、原换算、航向回显规则和 550 组历史寄存器样本。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
