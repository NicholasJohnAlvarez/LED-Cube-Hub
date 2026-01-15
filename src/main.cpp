// =============================================================================
// LED Cube Hub - Main Program
// Version 2.0 - Refactored with hardware abstraction
// =============================================================================
// This file contains the main setup(), loop(), and serial command interface.
// All hardware-specific code is in hardware.cpp/hardware.h
// =============================================================================

#include "hardware.h"

// =============================================================================
// Forward Declarations
// =============================================================================
void processSerial();
void programDevice(int deviceIdx, int cubeType, int ledCount);
void readDevice(int deviceIdx);

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println(F("\n============================="));
    Serial.println(F("     LED Cube Hub"));
    Serial.println(F("   ESP32-C3 + LIS3DH"));
    Serial.print(F("   Firmware v"));
    Serial.println(FIRMWARE_VERSION);
    Serial.println(F("=============================\n"));
    
    // Check wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println(F("*** Woke from deep sleep via double-tap! ***\n"));
    }
    
    // Initialize all hardware
    initializeHardware();
    
    Serial.println(F("LED pin: D3 (GPIO4)"));
    Serial.println(F("1-Wire pin: D10 (GPIO21)"));
    Serial.println(F("I2C SDA: D4 (GPIO6), SCL: D5 (GPIO7)"));
    Serial.println(F("LIS3DH INT1: D1 (GPIO2)"));
    Serial.print(F("Max cubes: "));
    Serial.println(MAX_CUBES);
    Serial.print(F("Max LEDs: "));
    Serial.println(MAX_TOTAL_LEDS);
    Serial.print(F("Free RAM: "));
    Serial.println(freeRam());
    
    Serial.println(F("\nScanning for cubes..."));
    scanOneWireBus();
    
    Serial.println(F("\nType 'help' for commands"));
    Serial.println(F("Gestures:"));
    Serial.println(F("  - Double-tap to toggle LEDs on/off"));
    Serial.println(F("  - Flip upside down 2x (within 2s) to sleep"));
    Serial.println(F("  - Double-tap while asleep to wake\n"));
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    uint32_t now = millis();
    
    // Check for sleep request
    if (sleepRequested) {
        enterDeepSleep();
        // Never returns from here
    }
    
    handleDoubleTap();
    
    processSerial();
    
    if (now - lastAccel >= ACCEL_UPDATE_MS) {
        lastAccel = now;
        updateAccelerometer();
    }
    
    if (now - lastOrientationCheck >= ORIENTATION_CHECK_MS) {
        lastOrientationCheck = now;
        checkOrientation();
    }
    
    if (now - lastPoll >= ONEWIRE_POLL_MS) {
        lastPoll = now;
        scanOneWireBus();
    }
    
    if (now - lastAnim >= ANIMATION_MS) {
        lastAnim = now;
        if (totalLeds > 0) {
            runAnimation();
            FastLED.show();
        }
    }
}

// =============================================================================
// Serial Command Handler
// =============================================================================

void processSerial() {
    if (!Serial.available()) return;
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "help" || cmd == "?") {
        Serial.println(F("\n=== LED Cube Hub (ESP32-C3 + LIS3DH) ==="));
        Serial.print(F("Firmware v")); Serial.println(FIRMWARE_VERSION);
        Serial.println(F("Commands:"));
        Serial.println(F("  status    - Show system status"));
        Serial.println(F("  scan      - Rescan 1-Wire bus"));
        Serial.println(F("  list      - List detected DS2431 devices"));
        Serial.println(F("  next      - Next animation"));
        Serial.println(F("  on        - Resume animation"));
        Serial.println(F("  off       - LEDs off"));
        Serial.println(F("  accel     - Toggle accelerometer mode (XYZ->RGB)"));
        Serial.println(F("  xyz       - Print current accelerometer data"));
        Serial.println(F("  tap       - Read CLICK_SRC register (debug)"));
        Serial.println(F("  sleep     - Enter deep sleep immediately"));
        Serial.println(F("  prog <idx> <type> <leds> - Program device"));
        Serial.println(F("  read <idx> - Read device config"));
        Serial.println(F("\nGestures:"));
        Serial.println(F("  Double-tap: Toggle LEDs on/off"));
        Serial.println(F("  Flip upside down 2x (within 2s): Enter sleep mode"));
        Serial.println(F("  Double-tap while sleeping: Wake up"));
    }
    else if (cmd == "status") {
        Serial.println(F("\n=== Status ==="));
        Serial.print(F("Firmware: v")); Serial.println(FIRMWARE_VERSION);
        Serial.print(F("Built: ")); Serial.print(BUILD_DATE); 
        Serial.print(F(" ")); Serial.println(BUILD_TIME);
        Serial.println(F("Board: XIAO ESP32-C3"));
        Serial.print(F("LIS3DH: "));
        Serial.println(lis3dhFound ? F("Found") : F("Not found"));
        Serial.print(F("LEDs enabled: "));
        Serial.println(ledsEnabled ? F("ON") : F("OFF"));
        Serial.print(F("Accel mode: "));
        Serial.println(accelMode ? F("ON") : F("OFF"));
        Serial.print(F("Cubes: "));
        Serial.println(cubeCount);
        Serial.print(F("Total LEDs: "));
        Serial.println(totalLeds);
        Serial.print(F("Animation: "));
        Serial.print(currentAnimation);
        Serial.println(animationRunning ? F(" (running)") : F(" (stopped)"));
        Serial.print(F("Free RAM: "));
        Serial.println(freeRam());
        Serial.print(F("Upside down: "));
        Serial.println(isUpsideDown ? F("YES") : F("NO"));
        Serial.print(F("Flip count: "));
        Serial.println(flipCount);
        Serial.print(F("INT1 pin state: "));
        Serial.println(digitalRead(PIN_LIS3DH_INT) ? F("HIGH") : F("LOW"));
        
        for (int i = 0; i < cubeCount; i++) {
            if (!cubes[i].active) continue;
            Serial.print(F("  Cube "));
            Serial.print(i);
            Serial.print(F(": Type="));
            Serial.print(cubes[i].config.cubeType);
            Serial.print(F(" LEDs="));
            Serial.print(cubes[i].ledStart);
            Serial.print(F("-"));
            Serial.println(cubes[i].ledStart + cubes[i].ledCount - 1);
        }
    }
    else if (cmd == "scan") {
        Serial.println(F("Scanning..."));
        scanOneWireBus();
        Serial.println(F("Done"));
    }
    else if (cmd == "list") {
        Serial.println(F("\nDS2431 devices on bus:"));
        uint8_t addr[8];
        int count = 0;
        oneWire.reset_search();
        while (oneWire.search(addr)) {
            if (isDS2431(addr)) {
                Serial.print(F("  ["));
                Serial.print(count);
                Serial.print(F("] "));
                for (int i = 0; i < 8; i++) {
                    if (addr[i] < 16) Serial.print('0');
                    Serial.print(addr[i], HEX);
                    if (i < 7) Serial.print(':');
                }
                Serial.println();
                count++;
            }
        }
        if (count == 0) {
            Serial.println(F("  None found"));
        }
    }
    else if (cmd == "next") {
        accelMode = false;
        currentAnimation = (currentAnimation + 1) % 5;
        animationRunning = true;
        ledsEnabled = true;
        Serial.print(F("Animation: "));
        Serial.println(currentAnimation);
    }
    else if (cmd == "on") {
        animationRunning = true;
        ledsEnabled = true;
        Serial.println(F("Animation resumed"));
    }
    else if (cmd == "off") {
        animationRunning = false;
        accelMode = false;
        ledsEnabled = false;
        fill_solid(leds, MAX_TOTAL_LEDS, CRGB::Black);
        FastLED.show();
        Serial.println(F("LEDs off"));
    }
    else if (cmd == "accel") {
        if (!lis3dhFound) {
            Serial.println(F("LIS3DH not available!"));
        } else {
            accelMode = !accelMode;
            animationRunning = true;
            ledsEnabled = true;
            Serial.print(F("Accelerometer mode: "));
            Serial.println(accelMode ? F("ON (X=R, Y=G, Z=B)") : F("OFF"));
        }
    }
    else if (cmd == "xyz") {
        printAccelData();
    }
    else if (cmd == "tap") {
        // Debug: manually read click source register
        uint8_t clickSrc = readReg(0x39);
        Serial.print(F("CLICK_SRC: 0x"));
        Serial.print(clickSrc, HEX);
        Serial.print(F(" ("));
        if (clickSrc & 0x40) Serial.print(F("IA "));
        if (clickSrc & 0x20) Serial.print(F("DCLICK "));
        if (clickSrc & 0x10) Serial.print(F("SCLICK "));
        if (clickSrc & 0x04) Serial.print(F("Z "));
        if (clickSrc & 0x02) Serial.print(F("Y "));
        if (clickSrc & 0x01) Serial.print(F("X "));
        Serial.println(F(")"));
        Serial.print(F("INT1 pin: "));
        Serial.println(digitalRead(PIN_LIS3DH_INT) ? F("HIGH") : F("LOW"));
    }
    else if (cmd == "sleep") {
        Serial.println(F("Entering sleep mode via command..."));
        sleepRequested = true;
    }
    else if (cmd.startsWith("prog ")) {
        int idx, type, ledCount;
        if (sscanf(cmd.c_str(), "prog %d %d %d", &idx, &type, &ledCount) == 3) {
            programDevice(idx, type, ledCount);
        } else {
            Serial.println(F("Usage: prog <idx> <type> <leds>"));
            Serial.println(F("Types: 1=Corner 2=Edge 3=Center 4=Hub"));
        }
    }
    else if (cmd.startsWith("read ")) {
        int idx = cmd.substring(5).toInt();
        readDevice(idx);
    }
    else if (cmd.length() > 0) {
        Serial.print(F("Unknown: "));
        Serial.println(cmd);
    }
}

void programDevice(int deviceIdx, int cubeType, int ledCount) {
    uint8_t addr[8];
    int count = 0;
    
    oneWire.reset_search();
    while (oneWire.search(addr)) {
        if (isDS2431(addr)) {
            if (count == deviceIdx) {
                CubeConfig config;
                memset(&config, 0, sizeof(config));
                config.cubeType = cubeType;
                config.ledCount = ledCount;
                config.colorOrder = 0;
                config.brightness = 128;
                
                Serial.print(F("Programming device "));
                Serial.print(deviceIdx);
                Serial.print(F(" as type "));
                Serial.print(cubeType);
                Serial.print(F(" with "));
                Serial.print(ledCount);
                Serial.println(F(" LEDs..."));
                
                if (ds2431WritePage(addr, 0, (uint8_t*)&config)) {
                    Serial.println(F("SUCCESS!"));
                    scanOneWireBus();
                } else {
                    Serial.println(F("FAILED!"));
                }
                return;
            }
            count++;
        }
    }
    Serial.println(F("Device not found"));
}

void readDevice(int deviceIdx) {
    uint8_t addr[8];
    int count = 0;
    
    oneWire.reset_search();
    while (oneWire.search(addr)) {
        if (isDS2431(addr)) {
            if (count == deviceIdx) {
                Serial.print(F("\nDevice "));
                Serial.print(deviceIdx);
                Serial.println(F(" config:"));
                
                CubeConfig config;
                if (ds2431ReadPage(addr, 0, (uint8_t*)&config)) {
                    Serial.print(F("  Type: "));
                    Serial.println(config.cubeType);
                    Serial.print(F("  LEDs: "));
                    Serial.println(config.ledCount);
                    Serial.print(F("  Color order: "));
                    Serial.println(config.colorOrder);
                    Serial.print(F("  Brightness: "));
                    Serial.println(config.brightness);
                } else {
                    Serial.println(F("  Read failed!"));
                }
                return;
            }
            count++;
        }
    }
    Serial.println(F("Device not found"));
}
