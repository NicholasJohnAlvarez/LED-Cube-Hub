// =============================================================================
// hardware.h - Hardware abstraction layer for LED Cube Hub
// =============================================================================
// Contains all pin definitions, constants, structures, and hardware function
// declarations for the LED Cube Hub project.
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

// =============================================================================
// Version Information
// =============================================================================
#define FIRMWARE_VERSION "2.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// =============================================================================
// Pin Definitions (XIAO ESP32-C3)
// =============================================================================
#define PIN_LED_DATA    D3   // WS2812 data (GPIO4)
#define PIN_ONEWIRE     D10  // DS2431 1-Wire bus (GPIO21)
#define PIN_I2C_SDA     D4   // LIS3DH I2C SDA (GPIO6)
#define PIN_I2C_SCL     D5   // LIS3DH I2C SCL (GPIO7)
#define PIN_LIS3DH_INT  D1   // LIS3DH INT1 pin (GPIO2) - WAKEUP CAPABLE

// =============================================================================
// Configuration Constants
// =============================================================================
#define MAX_CUBES       8
#define MAX_TOTAL_LEDS  300
#define DS2431_FAMILY   0x2D

// Timing
#define ONEWIRE_POLL_MS     1000
#define ANIMATION_MS        33
#define ACCEL_UPDATE_MS     50
#define ORIENTATION_CHECK_MS 100

// Sleep Configuration
#define FLIP_DETECT_WINDOW_MS  2000  // 2 second window for double flip
#define SLEEP_FADE_MS          1000  // 1 second fade to black before sleep

// LIS3DH I2C Address
#define LIS3DH_ADDRESS    0x18

// =============================================================================
// Data Structures
// =============================================================================

// Cube Configuration Structure (stored in DS2431 EEPROM)
struct CubeConfig {
    uint8_t  cubeType;
    uint16_t ledCount;
    uint8_t  colorOrder;
    uint8_t  brightness;
    uint8_t  reserved[27];
};

// Cube Instance (runtime tracking)
struct Cube {
    uint64_t romId;
    CubeConfig config;
    uint16_t ledStart;
    uint16_t ledCount;
    bool active;
};

// =============================================================================
// Global Hardware Objects (extern declarations)
// =============================================================================
extern OneWire oneWire;
extern CRGB leds[];
extern Adafruit_LIS3DH lis3dh;

// =============================================================================
// Global State Variables (extern declarations)
// =============================================================================
extern Cube cubes[];
extern int cubeCount;
extern int totalLeds;

extern uint32_t lastPoll;
extern uint32_t lastAnim;
extern uint32_t lastAccel;
extern uint32_t lastOrientationCheck;
extern uint8_t animFrame;
extern uint8_t currentAnimation;
extern bool animationRunning;
extern bool accelMode;
extern bool lis3dhFound;
extern bool ledsEnabled;

extern volatile bool doubleTapDetected;

extern uint8_t accelR;
extern uint8_t accelG;
extern uint8_t accelB;

// Sleep/orientation tracking
extern bool isUpsideDown;
extern uint32_t firstFlipTime;
extern int flipCount;
extern bool sleepRequested;

// =============================================================================
// Hardware Function Declarations
// =============================================================================

// I2C Register Functions
void writeReg(uint8_t reg, uint8_t val);
uint8_t readReg(uint8_t reg);

// Interrupt Handler
void IRAM_ATTR onDoubleTap();

// Utility Functions
int freeRam();

// LIS3DH Functions
bool initLIS3DH();
void handleDoubleTap();
void updateAccelerometer();
void printAccelData();

// Sleep Functions
void enterDeepSleep();
void checkOrientation();

// DS2431 Functions
uint64_t addressToId(uint8_t* addr);
void idToAddress(uint64_t id, uint8_t* addr);
bool isDS2431(uint8_t* addr);
bool ds2431ReadPage(uint8_t* addr, uint8_t page, uint8_t* buffer);
bool ds2431Write8(uint8_t* addr, uint8_t offset, uint8_t* data);
bool ds2431WritePage(uint8_t* addr, uint8_t page, uint8_t* data);

// Cube Management Functions
int findCube(uint64_t romId);
bool addCube(uint64_t romId, CubeConfig* config);
void removeCube(uint64_t romId);
void scanOneWireBus();

// Animation Functions
void runAnimation();

// Hardware Initialization
void initializeHardware();

#endif // HARDWARE_H
