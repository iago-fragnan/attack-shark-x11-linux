# Attack Shark X11 Linux Technical Documentation

This directory collects the technical documentation and the results of the reverse engineering activity conducted on the **Attack Shark X11** gaming mouse and its official Windows software.

---

## Document Index

1. **[REVERSE_ENGINEERING.md](file:///var/home/simone/GitHub/attack-shark-x11-linux/docs/REVERSE_ENGINEERING.md)**:
   * Complete report on the analysis of the official binaries (`AttackShark.exe` and `hiddriver_3.dll`).
   * Detailed explanation with assembly code snippets (x86 disassembly) of the command dispatcher.
   * Diagnosis and resolution of historical bugs (ACK error `03 10 50 01 04`, PAW3311 sensor tables, 16-bit checksum).
   * Explanation of the battery ADC with discrete 10% intervals and hardware synchronization of the DPI button.
   * Management of the dual-mode architecture (wired `1d57:fa61` with radio module off vs wireless `1d57:fa60`).

2. **[PROTOCOL.md](file:///var/home/simone/GitHub/attack-shark-x11-linux/docs/PROTOCOL.md)**:
   * Quick reference for developers on the low-level USB HID protocol.
   * Exact layout of the 64-byte Feature Report packets (`Report 0x04` DPI, `Report 0x05` RGB, `Report 0x06` Polling Rate).
   * Format of the asynchronous Interrupt Input Reports (ACK, physical DPI button, battery telemetry).
   * Checksum algorithms and hardware conversion tables.
