# 保护测试

实际检查在 tests/integration/test_firmware.c 的 state_safety_tests/drive_task_tests：命令 FIFO 满、停止锁存、新命令恢复、500 ms 新鲜度、300 ms 推进阈值、5200 ms 命令超时、计时回绕和倒车规则。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
