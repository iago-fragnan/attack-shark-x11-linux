# HID Protocol Specification: Attack Shark X11

Reference document for the USB HID packets of the **Attack Shark X11** (Beken BK2535 chipset / PixArt PAW3311).

---

## 1. Device Identifiers

* **Vendor ID (VID)**: `0x1D57`
* **Product ID Wireless (PID)**: `0xFA60`
* **Product ID Wired/Charging (PID)**: `0xFA61`
* **Target Interface**: `bInterfaceNumber == 2` (Vendor Configuration Interface)
* **Transport Channel**:
  * Write: USB Feature Report via `ioctl(fd, HIDIOCSFEATURE(len), data)`
  * Notification read: Interrupt Input Report via `read(fd, buf, sizeof(buf))`

---

## 2. Feature Reports (Host $\rightarrow$ Mouse)

### 2.1 Report `0x04`: Sensor and DPI Profile Configuration (64 Bytes)

Sent to configure the DPI stages, the active stage, and the registers of the PAW3311 optical sensor.

```
Byte 00: 0x04              (Report ID)
Byte 01: 0x09 or 0x10       (Vendor command)
Byte 02: 0x01              (Sensor mode)
Byte 03: 0x05              (Total number of stages, default = 5)
Byte 04..05: 0x00, 0x00    (Reserved)
Byte 06..07: Bitmask       (Mask for DPI > 12,000: e.g., 0x01 for stage 0 if >12000)
Byte 08..13: DPI values    (5 step bytes computed via the kDpiStepTable table)
Byte 14..15: 0x00, 0x00    (Padding)
Byte 16..21: Range Flags   (Flags for the 10,100–12,000 and 20,100–22,000 DPI ranges)
Byte 22..23: 0x00, 0x00    (Padding)
Byte 24:     Active Stage  (1-based index of the active stage: 0x01..0x05)
Byte 25..48: Stage Colors  (8 RGB triplets for the colors associated with DPI stages 1..8)
             - Stage 1: 0xFF, 0x00, 0x00 (Red)
             - Stage 2: 0x00, 0xFF, 0x00 (Green)
             - Stage 3: 0x00, 0x00, 0xFF (Blue)
             - Stage 4: 0xFF, 0xFF, 0x00 (Yellow)
             - Stage 5: 0x00, 0xFF, 0xFF (Cyan)
             - Stage 6..8: 0xFF/0x00 padding
Byte 49:     LOD           (Lift-Off Distance: 0x02 = 2mm, 0x01 = 1mm)
Byte 50..51: Checksum 16   (Big-Endian 16-bit sum of bytes 3..49 inclusive)
Byte 52..63: 0x00 ...      (Zero padding up to 64 bytes)
```

#### 16-bit Big-Endian Checksum Calculation:
```c
uint16_t sum = 0;
for (int i = 3; i <= 49; ++i) {
    sum += packet[i];
}
packet[50] = (sum >> 8) & 0xFF; // MSB
packet[51] = sum & 0xFF;        // LSB
```

#### PAW3311 Sensor DPI Encoding Table (`kDpiStepTable`):
The values are not linear. Examples of common values:
* `400 DPI` $\rightarrow$ `0x09`
* `800 DPI` $\rightarrow$ `0x12`
* `1200 DPI` $\rightarrow$ `0x1B`
* `1600 DPI` $\rightarrow$ `0x25`
* `2400 DPI` $\rightarrow$ `0x38`
* `3200 DPI` $\rightarrow$ `0x4B`
* `6400 DPI` $\rightarrow$ `0x97`
* `10000 DPI` $\rightarrow$ `0xEB`

---

### 2.2 Report `0x05`: RGB Lighting, Sleep Timer, and Debounce (15 Bytes)

Sent to set the RGB LED mode, the static/breathing color, the power-saving parameters, and the key response time.

```
Byte 00: 0x05              (Report ID)
Byte 01: 0x0F              (LED Vendor command)
Byte 02: 0x01              (Zone ID)
Byte 03: LED Mode          (0x00=Off, 0x10=Static, 0x20=Breathing, 0x30=Neon, 0x40=Color Breathing, 0x50=Static DPI, 0x60=Breathing DPI)
Byte 04: Speed and Bucket  ((bucket << 4) | (speed & 0x0F), speed = 1..8)
Byte 05: Brightness/Sleep  (((0x08 + deepSleep * 0x10) & 0xF0) | (brightness & 0x0F), brightness = 1..8)
Byte 06: R Color           (0x00..0xFF)
Byte 07: G Color           (0x00..0xFF)
Byte 08: B Color           (0x00..0xFF)
Byte 09: Sleep Timer       (sleepTimeMin * 2, from 1 to 30 minutes)
Byte 10: Debounce Time     (((keyResponseTimeMs - 4) / 2) + 2, from 4 to 50 ms)
Byte 11: Checksum High     ((sum >> 8) & 0xFF, 16-bit Big-Endian sum of bytes 3..10)
Byte 12: Checksum Low      (sum & 0xFF)
Byte 13..14: 0x00, 0x00    (Padding)
```

#### Supported LED Modes:
1. `0x00` - **Disabled / Off**: LED completely off.
2. `0x10` - **Static**: Solid fixed custom RGB color (Bytes 6..8).
3. `0x20` - **Breathing**: Single custom RGB color breathing effect (Bytes 6..8).
4. `0x30` - **Neon**: Continuous color spectrum cycle / automatic rainbow.
5. `0x40` - **Color Breathing**: Cyclic multicolor breathing with gradual color transitions.
6. `0x50` - **Static DPI**: Steady light with the color associated with the active DPI level (S1=Red, S2=Green, S3=Blue, S4=Yellow, S5=Cyan).
7. `0x60` - **Breathing DPI**: Breathing effect with the color associated with the active DPI level.

#### 16-bit Big-Endian Checksum Calculation:
```c
uint16_t sum = 0;
for (int i = 3; i <= 10; ++i) {
    sum += packet[i];
}
packet[11] = (sum >> 8) & 0xFF; // MSB
packet[12] = sum & 0xFF;        // LSB
```

---

### 2.3 Report `0x06`: Polling Rate (64 Bytes)

Sent to set the USB sampling frequency.

```
Byte 00..02: 0x06, 0x09, 0x01
Byte 03..04: Frequency payload
  - 125 Hz:  0x08, 0xF7
  - 250 Hz:  0x04, 0xFB
  - 500 Hz:  0x02, 0xFD
  - 1000 Hz: 0x01, 0xFE
Byte 05..63: 0x00 ... (Zero padding)
```

---

## 3. Asynchronous Input Reports (Mouse $\rightarrow$ Host)

Received by performing an asynchronous `read()` on the `/dev/hidrawX` descriptor.

### 3.1 ACK Response to Host Commands
```
[0x03, 0x10, 0x50, <status>, <report_id>]
```
* `<status>`:
  * `0x00`: Success (command accepted and configuration applied).
  * `0x01`: Error / Invalid checksum / Value out of range.
* `<report_id>`: ID of the report to which the response refers (e.g., `0x04` for DPI).

---

### 3.2 Physical DPI Button Press Notification
Sent by the mouse every time the user clicks the hardware DPI button:
```
[0x03, 0x10, 0x10, <stage_1_based>, 0x00]
```
* `<stage_1_based>`: Index of the selected stage (from `1` to `5`).
* Software index conversion formula: `stageIndex = stage_1_based - 1` (from `0` to `4`).

---

### 3.3 Battery Telemetry and Charging State
Sent periodically by the mouse or when the electrical state changes:
```
[0x03, 0x10, 0x40, <state>, <level>]
```
* `<state>`:
  * `0x01`: **Discharging** (normal battery operation).
  * `0x02`: **Charging** (connected to the charging cable or dock).
  * `0x03`: **Fully Charged** (battery at 100% with charging complete).
* `<level>`:
  * Scale from `0x00` to `0x0a` (`0`..`10`), representing discrete 10% intervals.
  * Percentage formula: `percentage = level * 10` (e.g., `0x0a` $\rightarrow$ `100%`, `0x08` $\rightarrow$ `80%`).
