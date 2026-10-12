# 协议测试

实际检查在 tests/integration/test_firmware.c 的 protocol_tests：分包连包、转义引号、字符串括号、必需字段、经纬度精度、arena 重用、超大与非有限数值、嵌套深度及遥测截断。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
