# LED Cube Hub

ESP32-C3 based modular LED cube system with hot-swappable magnetic connectors and intelligent cube detection.

![Firmware Version](https://img.shields.io/badge/firmware-v2.1-blue)
![Platform](https://img.shields.io/badge/platform-PlatformIO-orange)
![Board](https://img.shields.io/badge/board-ESP32--C3-green)

## Overview

The LED Cube Hub is a modular lighting system that allows you to build custom LED cube configurations by magnetically connecting individual cube modules. Each cube contains WS2812B addressable LEDs and a DS2431 1-Wire EEPROM chip that stores configuration data, enabling automatic detection and configuration when cubes are connected or removed.

### Key Features

- 🧲 **Hot-Swappable Cubes** - Magnetic connectors allow instant plug-and-play cube configuration
- 🔍 **Automatic Detection** - DS2431 1-Wire EEPROM in each cube for auto-identification
- 🎨 **Dynamic LED Control** - WS2812B addressable LEDs with multiple animation modes
- 📱 **Gesture Control** - LIS3DH accelerometer for tap detection and orientation sensing
- 💤 **Smart Sleep Mode** - Double-flip gesture to enter deep sleep, double-tap to wake
- ⚡ **Low Power** - Deep sleep mode for battery operation (~0.043mA)
- 🔧 **Serial Interface** - Full command-line control for debugging and configuration

## Hardware

### 3D Models & Assembly
**[View Complete Assembly on OnShape →](https://cad.onshape.com/documents/34c5c7f2d0c6fb02d6313538/w/ba31acd58973aaa831a2c1d2/e/0c686cc24784cde7dad5088e)**

### Hub PCB Components
- **Microcontroller:** Seeed XIAO ESP32-C3 (RISC-V, 160MHz, 400KB SRAM)
- **Accelerometer:** LIS3DH 3-axis (I2C, interrupt-driven)
- **LEDs:** WS2812B addressable RGB LEDs (up to 300 total)
- **1-Wire Bus:** DS2431 EEPROM communication (GPIO21, 1.2k-4.7k pull-up)
- **Magnetic Connectors:** Power and data for cube connections

### Extension Cube PCB
- **Memory:** DS2431 1024-bit EEPROM with unique 64-bit ROM ID
- **LEDs:** WS2812B strips (configurable per cube)
- **Connectors:** Magnetic interconnects for daisy-chaining

### Pin Configuration
```
D3  (GPIO4)  - WS2812 LED Data
D10 (GPIO21) - DS2431 1-Wire Bus
D4  (GPIO6)  - LIS3DH I2C SDA
D5  (GPIO7)  - LIS3DH I2C SCL
D1  (GPIO2)  - LIS3DH INT1 (Wake-capable)
```

## Firmware Features

### Animations
- **Rainbow Wave** - Smooth HSV color cycling
- **Breathe** - Pulsing brightness effect
- **Chase** - Single LED runner with fade trail
- **Sparkle** - Random white LED bursts
- **Solid White** - Full brightness white
- **Accelerometer Mode** - XYZ axes mapped to RGB color

### Gestures & Controls
- **Double-Tap** - Toggle LEDs on/off
- **Double Flip** - Flip cube upside-down twice within 2 seconds to enter sleep mode
- **Wake-Up Tap** - Double-tap while sleeping to wake device

### Serial Commands
```
help      - Show all commands
status    - System status and cube info
scan      - Rescan 1-Wire bus for cubes
list      - List all detected DS2431 devices
next      - Cycle to next animation
on/off    - Enable/disable LEDs
accel     - Toggle accelerometer color mode
xyz       - Print accelerometer data
sleep     - Enter deep sleep immediately
prog      - Program cube EEPROM
read      - Read cube configuration
```

## Software Architecture

The firmware is organized into three main files for clean separation of concerns:

```
src/
├── main.cpp          - Setup, loop, serial command interface (356 lines)
├── hardware.cpp      - Hardware implementations (634 lines)
└── include/
    └── hardware.h    - Declarations and configuration (135 lines)
```

### Architecture Benefits
- ✅ **Modular Design** - Hardware abstraction separated from application logic
- ✅ **Fast Compilation** - Only changed files recompile
- ✅ **Reusable Code** - Hardware layer portable to other projects
- ✅ **Easy Testing** - Individual modules can be tested independently
- ✅ **Maintainable** - Clear separation makes debugging straightforward

## Getting Started

### Prerequisites
- [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) (VS Code extension)
- USB-C cable for programming
- Seeed XIAO ESP32-C3 board

### Installation

1. **Clone the repository**
```bash
git clone https://github.com/NicholasJohnAlvarez/LED-Cube-Hub.git
cd LED-Cube-Hub
```

2. **Open in VS Code**
```bash
code .
```

3. **Build and Upload**
- Click the PlatformIO icon (alien head) in VS Code sidebar
- Select "Upload and Monitor" under seeed_xiao_esp32c3

4. **Connect to Serial Monitor**
- Baud rate: 115200
- Type `help` to see available commands

### Configuration

Edit `include/hardware.h` to customize:
```cpp
#define MAX_CUBES       8      // Maximum number of cubes
#define MAX_TOTAL_LEDS  300    // Total LED count
#define ANIMATION_MS    33     // Animation frame rate (30fps)
```

## Programming Cubes

Each cube must be programmed with its configuration before first use:

```
prog <index> <type> <led_count>

Example:
prog 0 1 25    // Program first cube as type 1 with 25 LEDs
```

**Cube Types:**
- `1` - Corner cube
- `2` - Edge cube  
- `3` - Center cube
- `4` - Hub cube

## Power Consumption

| Mode | Current Draw |
|------|-------------|
| Active (LEDs on) | ~60mA per LED @ full white |
| Active (ESP32) | 40-80mA |
| Deep Sleep | 0.043mA |
| Serial Active | ~2mA |

**Battery Life Example:**  
With 100 LEDs @ 50% brightness and 1000mAh battery: ~3-4 hours continuous operation

## Troubleshooting

### LEDs not working
- Check WS2812 data pin connection (D3/GPIO4)
- Verify power supply can handle LED current
- Ensure FastLED library version 3.6.0 is installed

### Cubes not detected
- Check 1-Wire pull-up resistor (1.2k-4.7k to VCC)
- Verify DS2431 ROM ID is valid
- Run `scan` command to force bus rescan

### Double-tap not responding
- Check INT1 connection (D1/GPIO2)
- Verify I2C connections (SDA/SCL)
- Run `tap` command to check CLICK_SRC register

### Sleep/wake issues
- Ensure LIS3DH INT1 is on GPIO2 (wake-capable pin)
- Check that interrupt is configured as active-high
- Test with `sleep` command first

## Technical Details

### FastLED Compatibility
This project uses **FastLED 3.6.0** specifically for ESP32-C3 RISC-V compatibility. Newer versions (3.7+, 3.10+) have known issues with RMT interrupt handling on ESP32-C3 that cause boot loops.

### 1-Wire Protocol
- Uses standard Dallas/Maxim 1-Wire protocol
- CRC-8 validation on all ROM ID reads
- Page-based EEPROM writes (32 bytes per page)
- Hot-swap detection via periodic bus scanning

### Sleep Implementation
- ESP32-C3 GPIO wake on D0-D3 pins only
- Uses `esp_deep_sleep_enable_gpio_wakeup()` for compatibility
- LIS3DH interrupt latching ensures wake signal persists
- All state cleared on wake (cubes re-scanned)

## Development

### Building from Source
```bash
# Install dependencies
pio lib install

# Clean build
pio run -t clean

# Build
pio run

# Upload
pio run -t upload

# Monitor serial
pio device monitor
```

### Adding New Animations
1. Add animation code in `hardware.cpp` under `runAnimation()`
2. Increment animation count in `main.cpp` where `currentAnimation` cycles
3. Rebuild and upload

### Creating Custom Gestures
1. Configure LIS3DH registers in `initLIS3DH()` 
2. Add detection logic in `handleDoubleTap()` or `checkOrientation()`
3. Implement action in main loop

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is open source. Feel free to use, modify, and distribute as needed.

## Acknowledgments

- **FastLED Library** - High-performance LED control
- **Adafruit** - LIS3DH driver library
- **OneWire Library** - DS2431 communication
- **PlatformIO** - Modern embedded development platform

## Project Status

✅ Hardware design complete  
✅ PCB designs finalized  
✅ Firmware v2.1 stable  
🔄 Custom PCB manufacturing planned  
🔄 Battery management improvements  
📋 Future: WiFi control and web interface

---

**Made with ❤️ by Nicholas John Alvarez**

*For questions or collaboration: [GitHub Profile](https://github.com/NicholasJohnAlvarez)*
