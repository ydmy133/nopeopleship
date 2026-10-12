# 传输测试

实际检查在 tests/integration/test_firmware.c：当前 DMA 位置重复事件、环回、事务溢出、碎片、TX 超时、UART/DMA 错误和延迟 abort 回调。测试运行真实 Platform/Src/board_uart.c 与 rx_cursor.c。

运行完整检查：`python tests/run_host_tests.py` 和 `python tests/control/compare_legacy.py`。
