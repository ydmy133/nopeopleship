# Strict GNSS receive regression (v6 polling)

Run from the project root in an x64 Visual Studio Developer Command Prompt:

```bat
cl /nologo /utf-8 /std:c11 /W4 /Itests\gnss_receive\mocks /ICore\Inc /Fe:tests\gnss_receive\gnss_receive_test.exe /Fo:tests\gnss_receive\ tests\gnss_receive\test_gnss.c Core\Src\cJSON.c
tests\gnss_receive\gnss_receive_test.exe tests\gnss_receive\fixtures\wit_gps_register_snapshots.bin
```

The harness includes the actual driver and JSON formatter. It injects bytes
with the first byte pending at TX return and subsequent bytes across tick calls.
F4 SR-to-DR reads clear simulated flags; USART2 IRQ ownership is checked. It checks strict frame acceptance,
missing headers, a 13-byte FE reply, incorrect headers, CRC failure, echo-prefixed
frames, each UART error on otherwise CRC-correct data, bounded overflow, silence,
TX failure, Modbus exceptions, retained samples, the user's exact 15:09 serial-tool response, read-only transactions and JSON
bounds. No baud scan, sensor writes or response-delay recovery are simulated.

The fixture contains 550 equivalent 21-byte GPS replies generated from verified
87-byte WIT responses in the user's 2026-09-30 14-15-33-001 wplay capture. Extracted
registers 0x49..0x50 are unchanged; the smaller-frame CRC is generated for tests.
These are register replay fixtures, not original wire packets. Production code
never reconstructs missing bytes or turns a partial response into a valid frame.

Host tests do not model the analog RS485 circuit, NVIC timing, wire bit timing,
sensor settings or hardware persistence. Keil build and physical waveform
verification remain required. See docs/gnss/README.md and
RS485_TURNAROUND_ASSESSMENT.md for the actual root-cause investigation.
