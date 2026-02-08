// =============================================================================
// hardware.h - Hardware abstraction layer for LED Cube Hub
// =============================================================================
// Version 2.3 - Modular cube programming and simplified interface
// =============================================================================

#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <FastLED.h>
#include <OneWire.h>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include "esp_sleep.h"
#include "driver/gpio.h"

// =============================================================================
// Version Information
// =============================================================================
#define FIRMWARE_VERSION "2.3"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// =============================================================================
// Pin Definitions (XIAO ESP32-C3)
// =============================================================================
// V3 Board White Wire Changes:
// - Branch 2 1-Wire moved: D10 (GPIO10) → D9 (GPIO9) - strapping pin OK for 1-Wire
// - Branch 3 LED moved: D9 (GPIO9) → D10 (GPIO10) - avoids strapping pin for WS2812
// - Result: All WS2812 LEDs now on non-strapping pins for clean timing
// - GPIO9 is strapping pin but safe for 1-Wire with 2kΩ pull-up

// WS2812 Data Lines
#define PIN_LED_1W      D3   // On-board LED (GPIO4)
#define PIN_LED_2W      D6   // Off-board branch 2 (GPIO21)
#define PIN_LED_3W      D10  // Off-board branch 3 (GPIO10) - white wired from D9 due to strapping pin issue

// 1-Wire Buses
#define PIN_ONEWIRE_1W  D8   // On-board DS2431 (GPIO8) - strapping pin, OK with 2kΩ pull-up
#define PIN_ONEWIRE_2W  D9   // Off-board branch 2 (GPIO9) - white wired from D10 due to strapping pin issue
#define PIN_ONEWIRE_3W  D2   // Off-board branch 3 (GPIO3)

// LIS3DH
#define PIN_I2C_SDA     D4   // I2C SDA (GPIO6)
#define PIN_I2C_SCL     D5   // I2C SCL (GPIO7)
#define PIN_LIS3DH_INT1 D1   // INT1 - tap detection (GPIO2) - WAKEUP CAPABLE
#define PIN_LIS3DH_INT2 D7   // INT2 - wake from sleep (GPIO20)

// Battery
#define PIN_VBAT_ADC    A0   // Battery voltage divider

// Legacy alias for compatibility
#define PIN_LIS3DH_INT  PIN_LIS3DH_INT1

// =============================================================================
// Configuration Constants
// =============================================================================
#define NUM_BRANCHES        3
#define MAX_CUBES_PER_BRANCH 8
#define MAX_CUBES           (NUM_BRANCHES * MAX_CUBES_PER_BRANCH)
#define MAX_LEDS_PER_BRANCH 100
#define MAX_TOTAL_LEDS      (NUM_BRANCHES * MAX_LEDS_PER_BRANCH)
#define DS2431_FAMILY       0x2D

// Cube Types (simplified - only 3 types)
#define CUBE_TYPE_EDGE      1   // Edge cube
#define CUBE_TYPE_CENTER    2   // Center cube  
#define CUBE_TYPE_HUB       3   // Hub cube (on-board)

// Timing
#define ONEWIRE_POLL_MS         1000
#define ANIMATION_MS            33
#define ACCEL_UPDATE_MS         50
#define ORIENTATION_CHECK_MS    100
#define VBAT_UPDATE_MS          5000

// Sleep Configuration
#define FLIP_DETECT_WINDOW_MS   1500  // was 2000 but would trigger too easily
#define SLEEP_FADE_MS           6000  // was 1000 but longer fade for better recognition of entering sleep mode

// LIS3DH I2C Address
#define LIS3DH_ADDRESS    0x18

// Battery voltage divider (200k/200k = 1:1, Vbat = 2 * Vadc)
// Uses analogReadMilliVolts() which provides factory-calibrated readings
// ADC reference varies ±10% per chip, but calibration data handles this
#define VBAT_DIVIDER_RATIO  2.0

// Low battery threshold (3.3V ~ 10%)
#define LOW_BATTERY_VOLTAGE 3.3

// =============================================================================
// Animation Modes
// =============================================================================
#define ANIM_ACCEL      0   // Accelerometer RGB mode (default on boot)
#define ANIM_RAINBOW    1
#define ANIM_PULSE      2
#define ANIM_CHASE      3
#define ANIM_SPARKLE    4
#define ANIM_SOLID      5
#define ANIM_COUNT      6   // Total number of animations

#define ANIM_LOW_BATTERY 99 // Special mode for low battery

// =============================================================================
// Data Structures
// =============================================================================

// Cube Configuration Structure (stored in DS2431 EEPROM)
struct CubeConfig {
    uint8_t  cubeType;        // 1=Corner, 2=Edge, 3=Center, 4=Hub
    uint16_t ledCount;        // Number of LEDs
    uint8_t  colorOrder;      // 0=GRB, 1=RGB
    uint8_t  brightness;      // Default brightness
    uint8_t  reserved[27];    // Pad to 32 bytes
};

// Cube Instance (runtime tracking)
struct Cube {
    uint64_t romId;
    CubeConfig config;
    uint8_t  branch;          // 0=1W, 1=2W, 2=3W
    uint16_t ledStart;        // Start index in branch LED buffer
    uint16_t ledCount;
    bool active;
};

// Branch Structure
struct Branch {
    OneWire* oneWire;
    CRGB* leds;
    uint8_t pinLed;
    uint8_t pinOneWire;
    int cubeCount;
    int totalLeds;
    Cube cubes[MAX_CUBES_PER_BRANCH];
    const char* name;
};

// =============================================================================
// Global Hardware Objects (extern declarations)
// =============================================================================
extern OneWire oneWire_1W;
extern OneWire oneWire_2W;
extern OneWire oneWire_3W;

extern CRGB leds_1W[];
extern CRGB leds_2W[];
extern CRGB leds_3W[];

extern Branch branches[];

extern Adafruit_LIS3DH lis3dh;

// =============================================================================
// Global State Variables (extern declarations)
// =============================================================================
extern uint32_t lastPoll;
extern uint32_t lastAnim;
extern uint32_t lastAccel;
extern uint32_t lastOrientationCheck;
extern uint32_t lastVbat;
extern uint8_t animFrame;
extern uint8_t currentAnimation;
extern bool animationRunning;
extern bool lis3dhFound;
extern bool ledsEnabled;
extern bool lowBatteryMode;

extern volatile bool tapDetected;

extern uint8_t accelR;
extern uint8_t accelG;
extern uint8_t accelB;

// Sleep/orientation tracking
extern bool isUpsideDown;
extern uint32_t firstFlipTime;
extern int flipCount;
extern bool sleepRequested;

// Battery
extern float batteryVoltage;

// =============================================================================
// Hardware Function Declarations
// =============================================================================

// I2C Register Functions
void writeReg(uint8_t reg, uint8_t val);
uint8_t readReg(uint8_t reg);

// Interrupt Handler
void IRAM_ATTR onTap();

// Utility Functions
int freeRam();
int getTotalCubeCount();
int getTotalLedCount();

// Battery Functions
void updateBatteryVoltage();
int getBatteryPercent();
void checkLowBattery();

// LIS3DH Functions
bool initLIS3DH();
void handleTap();
void updateAccelerometer();
void printAccelData();

// Sleep Functions
void enterDeepSleep();
void checkOrientation();

// DS2431 Functions
uint64_t addressToId(uint8_t* addr);
void idToAddress(uint64_t id, uint8_t* addr);
bool isDS2431(uint8_t* addr);
bool ds2431ReadPage(OneWire* ow, uint8_t* addr, uint8_t page, uint8_t* buffer);
bool ds2431Write8(OneWire* ow, uint8_t* addr, uint8_t offset, uint8_t* data);
bool ds2431WritePage(OneWire* ow, uint8_t* addr, uint8_t page, uint8_t* data);

void clearWarnedDevice(uint64_t romId);
void getUnprogrammedDevices(uint64_t* ids, int* count, int maxCount);
uint64_t findDeviceByPartialId(uint32_t partialId);
const char* getCubeTypeName(uint8_t type);

// Cube Management Functions (per branch)
int findCubeInBranch(Branch* branch, uint64_t romId);
bool addCubeToBranch(Branch* branch, uint8_t branchIdx, uint64_t romId, CubeConfig* config);
void removeCubeFromBranch(Branch* branch, uint64_t romId);
void scanBranch(Branch* branch, uint8_t branchIdx);
void scanAllBranches();

// Animation Functions
void runAnimation();
void runLowBatteryAnimation();
void clearAllLeds();
const char* getAnimationName(uint8_t anim);

// Hardware Initialization
void initializeHardware();

#endif // HARDWARE_H