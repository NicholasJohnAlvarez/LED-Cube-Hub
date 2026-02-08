# LED Cube Hub Firmware v2.3

ESP32-C3 based modular LED cube system with hot-swappable magnetic cubes and intelligent DS2431-based detection.

## Features

- 🧲 **Hot-swappable cubes** with magnetic pogo pin connectors
- 🔄 **Truly modular** - program cubes once, use on any branch
- 🎨 **WS2812B LED control** with multiple animations
- 📱 **Gesture controls** via LIS3DH accelerometer
- 💤 **Deep sleep mode** for battery operation
- 🔋 **Factory-calibrated battery monitoring**
- 🔧 **Serial command interface** for programming and control

## Hardware

- **MCU:** Seeed XIAO ESP32-C3 (RISC-V, 160MHz)
- **Accelerometer:** LIS3DH (I2C, tap/orientation detection)
- **Cube Memory:** DS2431 1-Wire EEPROM (unique 64-bit ROM ID per cube)
- **LEDs:** WS2812B addressable RGB (up to 300 total)
- **Battery:** LiPo with voltage divider monitoring

### Pin Configuration (v3 Hardware)

```
LED Data:
  D3  (GPIO4)  - Branch 1 (on-board)
  D6  (GPIO21) - Branch 2 (off-board)
  D10 (GPIO10) - Branch 3 (off-board) - white-wired from D9 to avoid strapping pin

1-Wire Bus:
  D8  (GPIO8)  - Branch 1 (on-board)
  D9  (GPIO9)  - Branch 2 (off-board) - white-wired from D10, strapping pin OK for 1-Wire
  D2  (GPIO3)  - Branch 3 (off-board)

LIS3DH:
  D4  (GPIO6)  - I2C SDA
  D5  (GPIO7)  - I2C SCL
  D1  (GPIO3)  - INT1 (tap detection, wake-capable)

Battery:
  A0  - Battery voltage sense (2:1 divider, factory calibrated)
```

**Note:** v3 hardware swaps D9/D10 to avoid GPIO9 strapping pin issues with WS2812 timing.

## Quick Start

### Installation

1. Install [PlatformIO](https://platformio.org/install/ide?install=vscode)
2. Clone repository
3. Open in VS Code
4. Upload to board (PlatformIO: Upload)
5. Open Serial Monitor (115200 baud)

### Programming Your First Cube

```bash
# 1. Connect unprogrammed cube to any branch
list              # See all 1-Wire devices

# 2. Program it (branch, device index, type, LED count)
prog 1 0 3 1     # Branch 1, device 0, Hub type, 1 LED
prog 1 1 1 2     # Branch 1, device 1, Edge type, 2 LEDs

# 3. Check status
status           # Shows all detected cubes

# 4. Move cube to another branch - it works automatically!
```

### Cube Types

| Type | Name   | Typical Use      |
|------|--------|------------------|
| 1    | Edge   | Edge cubes       |
| 2    | Center | Center cubes     |
| 3    | Hub    | Hub (on-board)   |

## Commands

```
Essential:
  help              - Show all commands
  list              - List 1-Wire devices on each branch
  prog <br> <idx> <type> <leds>  - Program cube
  status            - System status and cube configuration
  
Control:
  on/off            - Toggle LEDs
  next              - Next animation
  sleep             - Enter deep sleep
  
Debug:
  scan              - Rescan all branches
  bat               - Battery voltage
  tap               - LIS3DH tap status
  xyz               - Accelerometer data
```

## Gestures

- **Single-tap** - Next animation
- **Double-tap** - Toggle LEDs on/off
- **Triple-flip** - Enter deep sleep (flip upside-down 3 times within 1.5s)
- **Double-tap (sleeping)** - Wake from deep sleep

## Animations

1. Rainbow Wave
2. Breathe
3. Chase
4. Sparkle
5. Solid White
6. Accelerometer (XYZ → RGB)

## Configuration

Edit `include/hardware.h`:

```cpp
#define MAX_CUBES_PER_BRANCH  8
#define MAX_TOTAL_LEDS        300
#define ANIMATION_MS          33     // 30fps
#define CLICK_THS             0x20   // Tap sensitivity (0x18-0x30 range)
```

## Building

```bash
# Clean build
pio run -t clean

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
```

## Important Notes

### FastLED Version
**Must use FastLED 3.6.0** for ESP32-C3 compatibility. Newer versions have RMT interrupt issues.

### Battery Measurement
Uses `analogReadMilliVolts()` with factory calibration and 16-sample averaging per Seeed Studio recommendations. Much more accurate than raw ADC reads.

### Deep Sleep Wake
- Only GPIO 0-5 can wake ESP32-C3 from deep sleep
- LIS3DH INT1 on GPIO3 (D1) works perfectly
- ~100ms delay before `esp_deep_sleep_start()` is critical
- Do NOT use `Serial.flush()` before sleep on battery

### Strapping Pins (v3 Hardware)
- GPIO9 is a strapping pin - bad for WS2812 timing
- GPIO9 is fine for 1-Wire (slow, tolerant protocol)
- Solution: Swap D9↔D10 between LED and 1-Wire functions

## Troubleshooting

**LEDs not working**
- Check FastLED version (must be 3.6.0)
- Verify pin connections match hardware.h

**Cubes not detected**
- Run `list` to see 1-Wire devices
- Check pull-up resistor on 1-Wire bus (2kΩ recommended)
- Verify magnetic connector contact

**Battery wake not working**
- Ensure 100ms delay before `esp_deep_sleep_start()`
- Remove `Serial.flush()` if present
- Verify LIS3DH registers are configured (see `enterDeepSleep()`)

**False sleep triggers when disconnecting cubes**
- Increase flip threshold in `checkOrientation()`: `-13000` → `-14000` (current default is `-13000`)

## Version History

### v2.3 (Current)
- Modular cube programming (program once, use anywhere)
- Branch/index addressing for programming
- Factory-calibrated battery measurement
- v3 hardware support (GPIO9/10 swap)
- Deep sleep wake fixes

### v2.2
- Gesture controls (tap, flip)
- Low battery warnings
- Sleep mode

### v2.1
- Initial release

## Project Structure

```
LED-Cube-Hub/
├── platformio.ini
├── include/
│   └── hardware.h        # Hardware declarations
└── src/
    ├── hardware.cpp      # Hardware implementation
    └── main.cpp          # Main program & commands
```

## License

Open source - feel free to use and modify.

## Author

Nicholas John Alvarez  
[GitHub](https://github.com/NicholasJohnAlvarez)

---

**3D Models:** [OnShape CAD](https://cad.onshape.com/documents/34c5c7f2d0c6fb02d6313538/w/ba31acd58973aaa831a2c1d2/e/0c686cc24784cde7dad5088e)
