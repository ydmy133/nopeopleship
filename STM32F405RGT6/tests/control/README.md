# 控制基准测试

compare_legacy.py 编译真实旧控制函数和当前 ControlManager，使用两个独立可执行文件隔离 BoatApp 状态。12 场景各 1000 步输出必须逐字一致。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
