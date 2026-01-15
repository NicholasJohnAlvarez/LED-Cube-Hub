# LED Cube Hub - PlatformIO Project

ESP32-C3 based LED cube controller with hot-swappable modules, accelerometer control, and deep sleep functionality.

## Hardware

- **Board**: Seeed XIAO ESP32C3
- **LEDs**: WS2812B addressable LED strips (FastLED)
- **Memory**: DS2431 1-Wire EEPROM chips (OneWire)
- **Accelerometer**: LIS3DH (I2C)

## Pin Configuration

| Function | Pin | GPIO | Notes |
|----------|-----|------|-------|
| LED Data | D3  | GPIO4 | WS2812B data line |
| 1-Wire   | D10 | GPIO21 | DS2431 EEPROM bus |
| I2C SDA  | D4  | GPIO6 | LIS3DH accelerometer |
| I2C SCL  | D5  | GPIO7 | LIS3DH accelerometer |
| INT1     | D1  | GPIO2 | Wake pin (double-tap) |

## Features

- ✅ Hot-swappable LED cube modules via 1-Wire EEPROM
- ✅ Multiple animations with FastLED
- ✅ Accelerometer control (XYZ → RGB mapping)
- ✅ Double-tap to toggle LEDs on/off
- ✅ Double flip (upside down 2x) to enter deep sleep
- ✅ Wake from sleep via double-tap
- ✅ Serial command interface

## PlatformIO Commands

### Build
```bash
pio run
```

### Upload
```bash
pio run --target upload
```

### Monitor Serial
```bash
pio device monitor
```

### Build + Upload + Monitor (All-in-one)
```bash
pio run --target upload && pio device monitor
```

### Clean Build
```bash
pio run --target clean
```

## Serial Commands

Type these in the serial monitor (115200 baud):

- `help` or `?` - Show all commands
- `status` - System status and diagnostics
- `scan` - Rescan 1-Wire bus for cubes
- `list` - List detected DS2431 devices
- `next` - Cycle to next animation
- `on` - Resume animation
- `off` - Turn LEDs off
- `accel` - Toggle accelerometer mode (XYZ→RGB)
- `xyz` - Print current accelerometer data
- `tap` - Read CLICK_SRC register (debug)
- `sleep` - Enter deep sleep immediately
- `prog <idx> <type> <leds>` - Program DS2431 device
- `read <idx>` - Read device configuration

## Gestures

- **Double-tap**: Toggle LEDs on/off
- **Flip upside down 2x** (within 2 seconds): Enter sleep mode
- **Double-tap while sleeping**: Wake up

## Library Versions

- FastLED: ^3.7.8
- OneWire: ^2.3.8
- Adafruit LIS3DH: ^1.2.7
- Adafruit Unified Sensor: ^1.1.14

## Firmware Version

Version 2.0 - Deep sleep and wake functionality

## Troubleshooting

### Upload Issues
1. Press RESET button once
2. If that fails, hold BOOT button and connect USB
3. Release BOOT after connection

### Serial Port Not Found
- Check Device Manager for COM port
- Try different USB cable (must support data)
- Driver may be needed: CP2102 or CH340

## Build Flags

- `ARDUINO_USB_CDC_ON_BOOT=1` - Enable USB CDC on boot
- `BOARD_HAS_PSRAM` - Enable PSRAM support

## Development

Built with:
- PlatformIO
- Arduino Framework
- ESP32 Arduino Core
