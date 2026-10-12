# 测试数据

wit_gps_register_snapshots.bin 是 550 组历史寄存器回放帧，不是原始线缆抓包；其说明保留在 legacy/GNSS_POLLING_TEST.md。legacy 保存重构前源码与算法 SHA256；只用作测试证据，不参与固件。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
