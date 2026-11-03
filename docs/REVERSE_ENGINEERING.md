# Technical Reverse Engineering Report: Attack Shark X11

**Target Hardware**: Attack Shark X11 Gaming Mouse (PixArt PAW3311 Sensor / Beken BK2535/BK52832 Controller)  
**Original Target Software**: `ATTACK SHARK X11 SOFT.exe` (Official Windows Version)  
**Analysis Environment**: Linux (Distrobox Ubuntu 26.04 LTS on Bazzite / Fedora Atomic host)  
**Author**: `attack-shark-x11-linux` Project

---

## 1. Objective of the Analysis

The official software provided by the manufacturer for the **Attack Shark X11** gaming mouse is available exclusively as a Win32 application. The initial attempt to implement a native Linux driver based on amateur reverse engineering (`hook.cpp`) had serious problems:
1. **Profiles and DPI not applied**: The mouse systematically rejected the commands, leaving the sensitivity unchanged even after pressing "Apply".
2. **Incorrect battery reading**: The interface reported inconsistent values (e.g., a fixed 10%, or 100% with no noticeable variation) and did not recognize the charging/discharging state.
3. **No synchronization of the physical DPI button**: When pressing the hardware button on the mouse to cycle through sensitivity profiles, the Linux software was unaware of it and did not update the visual state of the GUI.
4. **No handling of mixed Wired / Wireless 2.4G mode**: When the mouse is connected via USB cable for charging, it changes its USB Product ID and disables the 2.4G radio, blocking communication with the dongle.

To definitively resolve these issues, an in-depth static and dynamic reverse engineering analysis was conducted on the official Windows package provided by Attack Shark.

---

## 2. Methodology and Tools Used

### 2.1 Extraction of the Installation Package
The official executable `ATTACK SHARK X11 SOFT.exe` is a compressed archive generated with **Inno Setup**. It was extracted using the `innoextract` utility:
```bash
innoextract -d /tmp/attack_shark_official "ATTACK SHARK X11 SOFT.exe"
```
The extraction revealed the complete application tree:
* `AttackShark.exe`: Main Win32 binary (developed in C++ with the DirectUI / DuiLib graphical framework).
* `hiddriver_*.dll`: Suite of low-level HID interface drivers (`hiddriver_1.dll` ... `hiddriver_18.dll`).
* `res/MS/`: Graphical definitions and XML functional constraints divided by model (`MS_1` .. `MS_18`).
* `res/MS/MS_3/`: Directory specifically associated with the Attack Shark X11.

### 2.2 Binary Analysis Tools
* `objdump -M intel -D`: 32-bit x86 disassembly of code sections (`.text`) and import/export tables (`.rdata`, `.edata`, `.idata`).
* `readelf` / `hexdump`: PE header analysis, RVA (Relative Virtual Address) computation, and mapping of file offsets.
* Python binary introspection script to locate hardware constants, HID packet signatures, and Windows API calls (`HidD_SetFeature`, `HidD_GetFeature`, `ReadFile`, `WriteFile`, `SetTimer`, `CProgressUI::SetValue`).
* Live tracing of USB HID reports via `ioctl(HIDIOCSFEATURE)` and non-blocking reads on `/dev/hidraw*` nodes.

---

## 3. Device Identification and USB HID Architecture

### 3.1 USB Identifiers (VID / PID)
From the analysis of `AttackShark.exe` and the USB bus feedback via `sysfs`:
* **Vendor ID**: `0x1D57` (Beken Corporation).
* **Product ID Wireless Mode (2.4G Dongle)**: `0xFA60` (`Beken 2.4G Wireless Device`).
* **Product ID Wired Mode (USB Cable / Charging)**: `0xFA61` (`Beken USB Gaming Mouse`).

> [!NOTE]
> The old code incorrectly looked for `0xfa55` for wired mode. In reality, the Attack Shark X11 exposes the ID **`0xfa61`** when the USB cable is plugged in.

### 3.2 USB Interface Mapping
The USB receiver and the wired mouse expose 4 HID interfaces:
* `Interface 0` (`input0`): Standard mouse cursor (X/Y report, left/right/middle/side button clicks, wheel).
* `Interface 1` (`input1`): Consumer/multimedia keys and keyboard shortcuts.
* **`Interface 2` (`input2`, config)**: **Proprietary vendor interface**. It is the only endpoint enabled to receive configuration Feature Reports and to transmit asynchronous telemetry and hardware notification reports.
* `Interface 3` (`input3`): Diagnostic/firmware update interface.

The driver must therefore open exclusively the `/dev/hidraw*` node associated with **`bInterfaceNumber == 2`**.

---

## 4. Reverse Engineering of the DPI Protocol (Report `0x04`)

### 4.1 The Cause of Packet Rejection (`03 10 50 01 04`)
In the legacy `hook.cpp` code, any DPI change was ignored by the cursor.
By monitoring the hardware responses, the mouse replied with:
```
03 10 50 01 04
```
In the Beken protocol:
* `buf[0] == 0x03`: Report ID 3 (ACK/Response).
* `buf[1] == 0x10`: Vendor Command.
* `buf[2] == 0x50`: Packet type = Acknowledgement.
* `buf[3] == 0x01`: **STATUS = ERROR / REJECTED**.
* `buf[4] == 0x04`: Report ID to which the ACK refers (DPI Report `0x04`).

From the analysis of the `AttackShark.exe` disassembly (DPI packet builder routine), **three structural errors** emerged in the packet sent by the original code:

### 4.2 Actual Structure of the DPI Packet (`Report 0x04`, 64 Bytes)

| Byte Offset | Value / Function | Description |
|---|---|---|
| `0` | `0x04` | Feature Report ID (DPI Configuration) |
| `1` | `0x09` or `0x10` | Sub-command / Vendor prefix |
| `2` | `0x01` | Sensor configuration mode |
| `3` | `0x05` | Total number of configured DPI stages (default: 5) |
| `4..5` | `0x00, 0x00` | Reserved / Alignment |
| `6..7` | Bitmask | Flags for extended DPI (> 12,000 DPI) |
| **`8..13`** | **Encoded DPI values** | **Encoding bytes for the 5 DPI stages (`stage 0..4`)** |
| `14..15` | `0x00, 0x00` | Padding |
| `16..21` | High-Stage Flags | Specific flags for the 10,100–12,000 and 20,100–22,000 DPI ranges |
| `22..23` | `0x00, 0x00` | Padding |
| **`24`** | **Active Stage (`1..5`)** | **1-based index of the currently selected DPI stage** |
| `25..49` | `0x00` ... | Advanced sensor parameters (LOD, ripple control, angle snapping) |
| **`50..51`** | **Big-Endian Checksum (16 bit)** | **16-bit sum computed over bytes `3..49` inclusive** |
| `52..63` | `0x00` ... | Padding to 64 bytes |

#### Errors corrected compared to the previous code:
1. **Wrong position of the DPI bytes**: The old code placed the active stage at byte `8` and the DPI values at bytes `9..13`. The mouse firmware, however, requires the DPI values starting at **offset `8`** and the active stage at **offset `24`**.
2. **Non-existent linear formula**: The PixArt PAW3311 sensor does not use a mathematical formula `(DPI / 50) + 1`. It uses a non-linear hardware calibration table (`kDpiStepTable`), where for example:
   * 400 DPI = `0x09`
   * 800 DPI = `0x12`
   * 1600 DPI = `0x25`
   * 10000 DPI = `0xEB`
3. **16-bit Big-Endian checksum**: The firmware validates packet integrity by computing the 16-bit arithmetic sum of bytes `3` through `49`. If the sum in bytes `50` and `51` does not exactly match `(sum >> 8) & 0xFF` and `sum & 0xFF`, the command is discarded with status `0x01`.

By applying the correct structure and the dynamic checksum, the mouse responds immediately:
```
03 10 50 00 04
```
where `0x00` indicates **SUCCESS / COMMAND ACCEPTED**. The sensor sensitivity changes instantly.

---

## 5. Reverse Engineering of Asynchronous Hardware Notifications (Interrupt In)

By opening the `/dev/hidraw*` file descriptor in non-blocking mode with a loop based on `QSocketNotifier` or `poll()`, the mouse autonomously transmits asynchronous packets without the need for active polling.

### 5.1 Physical DPI Button Press Notification (`0x1010`)
When the user presses the physical DPI button located behind the wheel or on the bottom of the mouse, the device transmits a packet on the interrupt channel with the following signature:
```
[0x03, 0x10, 0x10, <stage_1_based>, 0x00]
```
In the `AttackShark.exe` binary, the main dispatcher routine at VMA `0x00496710` handles this event:
```x86asm
496719: cmp    al, 0x10                ; buf[1] == 0x10 (Vendor Prefix)
496721: and    eax, 0xff00             ; Mask the second command byte
496727: cmp    eax, 0x1000             ; buf[2] == 0x10 (DPI switch event)
49672c: jne    0x496782
49672e: mov    esi, DWORD PTR [ebp+0xc] ; esi = received stage (1-based: 1..8)
496731: lea    eax, [esi-1]
496734: cmp    eax, 7                  ; Maximum limit of 8 stages
496737: ja     0x496859
496743: dec    esi                     ; esi = stage - 1 (conversion to 0-based)
496744: push   0
496746: push   1                       ; SetSelected(true)
49674e: mov    ecx, [edi+esi*4+0x2e4]  ; Pointer to the CControlUI of the stage radio button
496755: mov    eax, [ecx]
496757: call   [eax+0x1c0]             ; Invokes OptionUI::Selected(true)
496763: push   [eax+esi*4+0x8c4]       ; Numeric DPI value corresponding to the stage
49676a: push   2
49676c: push   0x50b                   ; Internal WM_APP message: DPI toast notification
496771: push   [edi+0x168]
496777: call   PostMessageW            ; Updates the user interface
```

#### Replication in the Linux driver:
In [`AttackSharkDevice::parseIncomingReport`](file:///var/home/simone/GitHub/attack-shark-x11-linux/device/attackshark_device.cpp#L332-L345), when the bytes `0x03, 0x10, 0x10, stage` arrive:
1. The 0-based index `stageIndex = stage1Based - 1` is computed.
2. The Qt signal `activeDpiStageChanged(stageIndex)` is emitted.
3. In [`atsx11::onHardwareDpiStageChanged`](file:///var/home/simone/GitHub/attack-shark-x11-linux/atsx11.cpp#L195-L215), the radio button for the corresponding stage is checked via software and a notification is shown in the status bar with the actual DPI value.

---

### 5.2 Battery Telemetry and Charging Logic (`0x1040`)
The battery status report transmitted by the Beken chip has the following structure:
```
[0x03, 0x10, 0x40, <state>, <level>]
```
Analyzing the `AttackShark.exe` disassembly at functions `0x00496800` (report handler) and `0x00486210` (percentage interface update):

```x86asm
; State analysis (state = buf[3])
49681b: cmp    ebx, 0x2                ; ebx == 0x02 ? (State: Charging)
49681e: jne    0x49685f
496820: call   KillTimer(0x223)        ; Restart animation timer
496832: push   0xa                     ; Force display to 100% during animation
496836: call   0x486210
49687c: call   SetTimer(0x223, 30ms)   ; Start a 30 ms timer to make the green bar scroll

; Normal level analysis while discharging (level = buf[4])
49690d: cmp    esi, 0xa                ; esi = level (0x00..0x0a)
496910: ja     0x496920                ; If > 10 discard erroneous packet
496912: push   esi                     ; Pass the level to the rendering function
49691b: call   0x00486210

; Level -> percentage conversion routine (0x00486210):
48623f: mov    eax, [ebp+0x8]          ; eax = level (1..10)
486242: mov    ecx, [edi+0x39c]        ; ecx = Pointer to CProgressUI
486248: lea    esi, [eax+eax*4]        ; esi = level * 5
48624b: add    esi, esi                ; esi = (level * 5) * 2 = level * 10
48624d: push   esi                     ; push percentage (e.g., 10 * 10 = 100)
48624e: call   CProgressUI::SetValue   ; Sets the progress bar value
48627b: push   0x4f3090                ; L"%d%%" format string
486281: call   CDuiString::Format      ; Generates the text "%d%"
```

#### Resolution of the "Shows 100% and does not drop" behavior:
1. **10% hardware granularity**: The internal ADC converter of the Beken wireless controller does not send continuous 1% readings. It divides the discharge range of the Li-Po battery (4.2V $\rightarrow$ 3.5V) into **10 discrete intervals of 10% each** (`0x01` = 10%, `0x02` = 20%, ..., `0x0a` = 100%).
2. **Live confirmation**: By querying the real hardware connected to the machine:
   ```
   Data received from /dev/hidraw7: 03 10 40 01 0a
   ```
   * `buf[3] == 0x01`: Discharging (the mouse is not connected to the cable or the dock).
   * `buf[4] == 0x0a`: Level 10 $\rightarrow$ $10 \times 10\% = 100\%$.
   The battery was indeed fully charged at 100%. It stays at that value until prolonged use reduces the cell voltage below the 90% threshold, at which point the packet will contain `0x09` (90%).

---

## 6. Dual-Mode Management and Cable/Wireless Hot-Swap

Analysis of the USB behavior reveals a peculiar characteristic of the Attack Shark X11:
* When the mouse is connected via USB Type-C cable for use or charging, the mouse firmware **turns off the 2.4G radio transceiver module**.
* The wireless dongle remains connected to the PC (`1d57:fa60`), but the mouse stops communicating with it and activates the wired controller with the identifier **`1d57:fa61`**.

### Hotplugging Architecture in the Linux Driver:
In [`AttackSharkDevice::checkHotplugTransition`](file:///var/home/simone/GitHub/attack-shark-x11-linux/device/attackshark_device.cpp#L291-L310):
1. A periodic timer (`kHotplugCheckIntervalMs = 2000`) scans `/sys/class/hidraw`.
2. If the wired device `1d57:fa61` is detected, priority is immediately given to the cable, closing the dongle descriptor and attaching to the wired one.
3. The charging state is forced to `isCharging = true` and the Qt interface updates the indicator to `Battery: %1% (Charging via USB ⚡)`.
4. When the USB cable is unplugged, the driver instantly reopens the 2.4G receiver interface (`1d57:fa60`) with complete transparency.

---

## 7. Auto-Apply On-the-Fly

In the original Windows software, for some sections it was necessary to explicitly press the "Apply" button (`ms_3_btn_apply_dpi`), introducing usage friction. In the refactored Linux driver:
* All spinboxes and numeric selectors are connected to a 350 ms timer (`m_autoApplyTimer`). As soon as the user stops typing or scrolling, the complete configuration is compiled and sent to the device.
* Selecting the DPI stage radio buttons or loading a preset profile in *Settings* triggers **immediate** application with 0 ms latency.
* Full bidirectional synchronization is preserved: any change via the UI updates the mouse, and any press of the physical button on the mouse updates the UI.

---

---

## 8. RGB Lighting and LED Protocol (Report 0x05)

### 8.1 Diagnosis of the "LED stuck on breathing that changes colors" Bug
The anomalous behavior reported by the user (LED remaining permanently on rainbow *breathing*, ignoring disabling or mode changes) was traced to three distinct causes:
1. **Wrong off opcode**: In the old `kBaseColorModes` table, index 0 (*Disabled*) sent opcode `0x10` (Static) with RGB `0, 0, 0`, instead of the official value `0x00` (*Off*). The hardware interpreted `0x10` as a steady light.
2. **Static mode completely missing**: In the original Windows software (`res/lan/EN.xml` and `ms_3_lightpage.xml`), the 5 base modes for the `MS_3` profile (Attack Shark X11) are:
   * Mode 0 (`0x00`): **LED Off / Disabled**
   * Mode 1 (`0x10`): **Static / Steady Light** (custom RGB color)
   * Mode 2 (`0x20`): **Breathing** (custom RGB color)
   * Mode 3 (`0x30`): **Neon** (continuous spectrum cycle / rainbow)
   * Mode 4 (`0x40`): **Color Breathing** (continuous multicolor breathing)
   In the previous code, mode 1 was mapped to *Breathing*, skipping the *Static* mode, and the default option saved in the configuration files was *Neon* (`0x30`), which produces exactly a continuous cycle of pulsing colors.
3. **8-bit checksum truncation**: The Report `0x05` checksum in the original firmware (`0x496e93..0x496f8d` in `AttackShark.exe`) is a 16-bit Big-Endian sum distributed over bytes 11 and 12 (`sum >> 8` and `sum & 0xFF`). The old driver wrote only the low byte to `out[12]`, leaving `out[11]` unchanged. Consequently, for any configuration with a sum $\ge 256$, the packet was rejected by the mouse.

### 8.2 Disassembled Structure of the Report Builder Routine
```asm
; AttackShark.exe @ 0x496eeb - 0x496f8d (Report 0x05 LED & Sleep construction)
496eeb: mov WORD PTR [ebp-0x68], 0x0f05  ; [0] = 0x05, [1] = 0x0F
496ef1: mov BYTE PTR [ebp-0x66], 0x01    ; [2] = 0x01
496ef8: mov BYTE PTR [ebp-0x65], dl      ; [3] = mode << 4 (0x00, 0x10, 0x20, 0x30, 0x40)
496efb: mov BYTE PTR [ebp-0x64], dh      ; [4] = (bucket << 4) | (speed & 0x0F)
496f19: mov BYTE PTR [ebp-0x63], cl      ; [5] = (base & 0xF0) | (brightness & 0x0F)
496f25: mov BYTE PTR [ebp-0x62], ch      ; [6] = Red
496f28: mov BYTE PTR [ebp-0x61], bl      ; [7] = Green
496f3a: mov BYTE PTR [ebp-0x60], dl      ; [8] = Blue
496f37: mov BYTE PTR [ebp-0x5f], al      ; [9] = sleepTimeMin * 2
496f4c: mov BYTE PTR [ebp-0x5e], cl      ; [10] = ((keyResponseTime - 4) / 2) + 2
; 16-bit checksum calculation over bytes 3..10
496f7c: mov BYTE PTR [ebp-0x5c], cl      ; [12] = LSB checksum
496f7f: shr eax, 0x8
496f84: mov BYTE PTR [ebp-0x5d], al      ; [11] = MSB checksum
496f8a: push 0x0f                        ; Total length = 15 bytes
496f8c: push eax
496f8d: call 0x4969f0                    ; Sends Feature Report via ioctl
```

### 8.3 Live Validation on `/dev/hidraw7`
By querying the Attack Shark X11 with the 15-byte packets generated by the driver:
* **Off Mode (`0x00`)**: Immediate LED turn-off and ACK reception `03 10 50 00 05`.
* **Static Mode (`0x10`)**: Solid fixed color (tested with Red, Green, Blue, White and confirmed by the user).
* **Breathing Mode (`0x20`)**: Smooth breathing in the fixed color set by the user.
* **Neon Mode (`0x30`) & Color Breathing (`0x40`)**: Cyclic full-spectrum transitions with ACK `00`.
* **Static DPI Mode (`0x50`) & Breathing DPI (`0x60`)**: The lighting directly reflects the color of the active DPI stage (Red for S1, Green for S2, Blue for S3, Yellow for S4, Cyan for S5), both as a steady light and in pulsing breathing.

### 8.4 Stage Color Mapping in Report 0x04 (Bytes 25..48)
From the analysis of the routine at `0x00497350` in `AttackShark.exe`:
In the optical sensor configuration packet (Report `0x04`, 56 bytes), bytes 25..48 contain 8 RGB triplets that define the hardware colors associated with each DPI level:
* **Bytes 25..27**: Stage 1 = Red (`0xFF, 0x00, 0x00`)
* **Bytes 28..30**: Stage 2 = Green (`0x00, 0xFF, 0x00`)
* **Bytes 31..33**: Stage 3 = Blue (`0x00, 0x00, 0xFF`)
* **Bytes 34..36**: Stage 4 = Yellow (`0xFF, 0xFF, 0x00`)
* **Bytes 37..39**: Stage 5 = Cyan (`0x00, 0xFF, 0xFF`)
* **Bytes 40..48**: Stages 6..8 = Default padding (Purple, Orange, White)
* **Byte 49**: Lift-Off Distance (LOD = `0x02` for 2 mm)

### 8.5 Automatic Lighting Restoration on Hardware DPI Change
When the user presses the physical button to change the DPI level during use, the mouse firmware sends the notification report `0x03 0x10 0x10 <stage> 0x00` and autonomously activates the LED in the fixed color of the new level to provide visual feedback.
However, if the user had set a different custom mode (e.g., *Breathing*, *Neon* or *Disabled*), the mouse would remain stuck on that fixed color.
The Linux driver resolves this behavior:
1. When the hardware notification arrives, it updates the stage in the UI.
2. If the active mode is not *Static DPI*, it starts a 1000 ms timer (`m_dpiLedRestoreTimer`).
3. The user clearly sees the color of the new level for 1 second, after which the driver reapplies Report `0x05`, smoothly and transparently restoring the desired mode.

---

## 9. Summary of Results Obtained

| Feature | Original State (`hook.cpp`) | Post-Reverse Engineering State |
|---|---|---|
| **DPI Application** | Failed (`03 10 50 01 04`, hardware error) | Working (`03 10 50 00 04`, positive ACK, PAW3311 configured) |
| **Physical DPI Button** | Not detected | Real-time synchronization (`0x1010`) with automatic LED restoration |
| **Battery Reading** | Incorrect (false 10% / unknown format) | Correct (tenths scale, charge/discharge detection `0x1040`) |
| **USB Wired Mode** | Not recognized (looked for PID `fa55`) | Recognized with priority (`fa61`) and automatic charging state |
| **RGB LED Control** | Stuck on continuous breathing / broken off | 7 complete modes (Off, Static, Breathing, Neon, Color Breathing, Static DPI, Breathing DPI), RGB color picker and dedicated stage colors |
| **Parameter Application** | Required clicking the Apply button | Instant and debounced Auto-Apply |
| **Software Architecture** | Monolithic procedural file | Modular C++17 / Qt6 with strong types, testable and documented |
