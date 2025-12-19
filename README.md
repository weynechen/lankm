# LanKM - LAN Keyboard & Mouse Controller (Hardware-based)

LanKM (LAN Keyboard & Mouse) is a lightweight LAN keyboard and mouse sharing system implemented with a hardware-based solution for cross-platform control.

**Key Advantages:**
- ✅ **No software installation required** on Windows side
- ✅ Bypasses all Windows input interception
- ✅ Ultra-low latency (< 3ms)
- ✅ 100% compatibility with all applications
- ✅ **Verified technology stack**: ESP-IDF + TinyUSB

## System Architecture

```
[Physical Keyboard/Mouse] → [Linux Server] → [UART] → [ESP32-S3] → [USB] → [Windows PC]
```

### Component Description

1. **Linux Server**: Captures physical input devices and sends commands to ESP32 via UART
2. **ESP32-S3**: Receives UART commands and injects input via USB HID
3. **Windows PC**: Receives USB HID input directly, no software required

## Hardware Requirements

| Hardware | Description | Purpose |
|----------|-------------|---------|
| ESP32-S3 Development Board | With native USB OTG | HID device emulation |
| Linux Device | Raspberry Pi/PC/Laptop | Input capture |
| USB Cable | Micro USB / Type-C | Connect to Windows |
| UART Cable | 3 wires (TX/RX/GND) | Linux ↔ ESP32 |

### ESP32-S3 Pin Connections
```
USB  ─────────────────────→ Windows PC (plug directly)
GPIO44 (UART0_RX) ←────── Linux UART TX
GPIO43 (UART0_TX) ─────── Linux UART RX
GND                 ────── Linux GND
```

**Note:** UART0 is default for download, needs remapping to GPIO43/44

## Build Requirements

### Linux Server
```bash
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

### ESP32-S3 (ESP-IDF + TinyUSB)
- ESP-IDF v5.x
- TinyUSB (Espressif official integration)
- ESP32-S3 Dev Module

## Compilation

### Linux Server
```bash
mkdir build && cd build
cmake ..
make
```

### ESP32-S3
```bash
# Set up ESP-IDF environment
source $IDF_PATH/export.sh

cd src/device

# Configure project
idf.py menuconfig

# Build
idf.py build

# Flash (replace with actual port)
idf.py -p /dev/ttyACM0 flash

# Monitor output
idf.py -p /dev/ttyACM0 monitor
```

## Usage

### 1. Hardware Connection

```
ESP32-S3:
    USB  ─────────────→ Windows PC
    GPIO16 (RX) ←───── Linux UART TX
    GPIO17 (TX) ────── Linux UART RX
    GND      ────────→ Linux GND
```

### 2. Run Server

```bash
# Requires root privileges to access input devices
sudo ./build/lankm-server /dev/ttyACM0
```

### 3. Operation Instructions

- **F12**: Toggle control mode (LOCAL ↔ REMOTE)
- **REMOTE mode**: All input sent to Windows
- **LOCAL mode**: Input affects local Linux system

## Permissions Configuration

### Linux udev Rules
Create `/etc/udev/rules.d/99-lankm.rules`:
```
KERNEL=="event*", MODE="0666", GROUP="input"
KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"
KERNEL=="ttyAMA*", MODE="0666", GROUP="dialout"
```

Reload rules:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Communication Protocol

### Linux → ESP32 (UART)

**Protocol Type**: Text protocol, each line ends with `\n`

**Message Format**: `TYPE,PARAM1,PARAM2\n`

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| Mouse Move | `M,dx,dy` | Relative displacement | `M,10,5` |
| Mouse Button | `B,button,state` | Button/state | `B,1,1` (left button press) |
| Keyboard | `K,keycode,state` | Keycode/state | `K,28,1` (Enter press) |
| State Toggle | `S,state` | 1=REMOTE, 0=LOCAL | `S,1` |

**Parameter Description**:
- `dx, dy`: Mouse relative displacement (int16)
- `button`: 1=left, 2=right, 3=middle
- `state`: 1=pressed, 0=released
- `keycode`: Linux evdev keycode (e.g., 28=Enter)

### ESP32 → Windows (USB HID)

**Protocol Type**: Standard USB HID

**Keyboard Report (8 bytes)**:
```
[0] Modifier keys (Ctrl/Shift/Alt/Win)
[1] Reserved
[2-7] 6 keycodes
```

**Mouse Report (4 bytes)**:
```
[0] Button state (bit mask)
[1] X displacement (int8)
[2] Y displacement (int8)
[3] Scroll wheel (int8)
```

## Project Structure

```
lankm/
├── src/
│   ├── common/
│   │   ├── protocol.h          # Message definitions (Message struct)
│   │   └── protocol.c          # Message constructor functions
│   ├── server/                 # Linux Server (C language)
│   │   ├── main.c              # Main program + UART transmission
│   │   ├── input_capture.c     # evdev capture
│   │   ├── input_capture.h
│   │   ├── state_machine.c     # State management (LOCAL/REMOTE)
│   │   └── state_machine.h
│   └── device/                 # ESP32-S3 Firmware (ESP-IDF)
│       ├── main/
│       │   ├── CMakeLists.txt
│       │   ├── idf_component.yml
│       │   ├── lankm_esp32.c   # Main program (UART0 GPIO43/44)
│       │   ├── usb_descriptors.c # USB HID descriptors
│       │   └── uart_parser.c   # UART command parsing
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       └── README.md
├── docs/
│   ├── design/
│   │   └── design.md           # Design documentation
│   └── README.zh-CN.md         # Chinese documentation
├── CMakeLists.txt              # Linux Server build configuration
├── README.md                   # This file
└── CLAUDE.md                   # Claude Code instructions
```

## Performance Metrics

| Metric | Target | Description |
|--------|--------|-------------|
| End-to-end latency | < 3ms | Linux capture → ESP32 → Windows |
| CPU usage | < 1% | Linux Server |
| Memory usage | < 5MB | Linux Server |
| ESP32 processing | < 1ms | UART parsing + HID transmission |

## Advantages Comparison

| Feature | Software Solution | Hardware Solution | Result |
|---------|------------------|-------------------|--------|
| Windows compatibility | May be intercepted | ✅ 100% compatible | Hardware bypasses security |
| Security | Requires running code | ✅ Pure hardware | Less attack surface |
| Latency | 5-10ms | ✅ <3ms | Direct USB HID |
| Maintenance cost | High | ✅ Low | Minimal components |
| No installation required | ❌ | ✅ | Truly plug-and-play |

## Troubleshooting

1. **ESP32 not recognized**: Check USB cable and drivers
2. **UART no response**: Check wiring and permissions
3. **Input not working**: Confirm in REMOTE mode (toggle with F12)
4. **USB disconnects**: Re-initialize USB connection

## Core Innovation

Using **ESP32-S3's USB HID capability** to replace Windows software injection completely solves:
- Windows input interception issues
- Compatibility problems
- Security concerns

## License

MIT License

## Additional Documentation

- [中文文档](docs/README.zh-CN.md) - Chinese documentation
- [设计文档](docs/design/design.md) - Detailed design documentation
