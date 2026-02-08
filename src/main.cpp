// =============================================================================
// LED Cube Hub - Main Program
// =============================================================================
// Version 2.3 - Modular cube programming and simplified interface
// =============================================================================

#include "hardware.h"

// =============================================================================
// Forward Declarations
// =============================================================================
void processSerial();

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
    Serial.println(F("   3-Branch Architecture"));
    Serial.println(F("=============================\n"));
    
    // Check wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println(F("*** Woke from deep sleep via double-tap! ***\n"));
    }
    
    // Initialize all hardware
    initializeHardware();
    
    Serial.print(F("Free RAM: "));
    Serial.println(freeRam());
    
    Serial.println(F("\nScanning all branches for cubes..."));
    scanAllBranches();
    
    Serial.println(F("\nGestures:"));
    Serial.println(F("  Single-tap  = Next animation"));
    Serial.println(F("  Double-tap  = Toggle LEDs on/off"));
    Serial.println(F("  Double-flip = Enter sleep mode"));
    Serial.println(F("\nType 'help' for serial commands\n"));
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    uint32_t now = millis();
    
    // Check for sleep request
    if (sleepRequested) {
        enterDeepSleep();
        // Never returns
    }
    
    handleTap();
    
    processSerial();
    
    if (now - lastAccel >= ACCEL_UPDATE_MS) {
        lastAccel = now;
        updateAccelerometer();
    }
    
    if (now - lastOrientationCheck >= ORIENTATION_CHECK_MS) {
        lastOrientationCheck = now;
        checkOrientation();
    }
    
    if (now - lastVbat >= VBAT_UPDATE_MS) {
        lastVbat = now;
        updateBatteryVoltage();
        checkLowBattery();
    }
    
    if (now - lastPoll >= ONEWIRE_POLL_MS) {
        lastPoll = now;
        scanAllBranches();
    }
    
    if (now - lastAnim >= ANIMATION_MS) {
        lastAnim = now;
        if (getTotalLedCount() > 0) {
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
        Serial.println(F("\n=== LED Cube Hub (3-Branch) ==="));
        Serial.print(F("Firmware v")); Serial.println(FIRMWARE_VERSION);
        Serial.println(F("Commands:"));
        Serial.println(F("  status     - Show system status"));
        Serial.println(F("  scan       - Rescan all 1-Wire buses"));
        Serial.println(F("  list       - List all DS2431 devices"));
        Serial.println(F("  unprog     - List unprogrammed devices"));
        Serial.println(F("  next       - Next animation"));
        Serial.println(F("  on         - LEDs on"));
        Serial.println(F("  off        - LEDs off"));
        Serial.println(F("  xyz        - Print accelerometer data"));
        Serial.println(F("  tap        - Read CLICK_SRC (debug)"));
        Serial.println(F("  bat        - Read battery voltage"));
        Serial.println(F("  sleep      - Enter deep sleep"));
        Serial.println(F("  prog       - Show programming help"));
        Serial.println(F("  prog <branch> <idx> <type> <leds>"));
        Serial.println(F("  read <branch> <idx>"));
        Serial.println(F("\nGestures:"));
        Serial.println(F("  Single-tap  = Next animation"));
        Serial.println(F("  Double-tap  = Toggle LEDs on/off"));
        Serial.println(F("  Double-flip = Enter sleep mode"));
    }
    else if (cmd == "status") {
        Serial.println(F("\n=== Status ==="));
        Serial.print(F("Firmware: v")); Serial.println(FIRMWARE_VERSION);
        Serial.print(F("Built: ")); Serial.print(BUILD_DATE);
        Serial.print(F(" ")); Serial.println(BUILD_TIME);
        Serial.println(F("Board: XIAO ESP32-C3"));
        Serial.print(F("LIS3DH: "));
        Serial.println(lis3dhFound ? F("Found") : F("Not found"));
        Serial.print(F("LEDs: "));
        Serial.println(ledsEnabled ? F("ON") : F("OFF"));
        Serial.print(F("Animation: "));
        Serial.println(getAnimationName(currentAnimation));
        Serial.print(F("Low battery mode: "));
        Serial.println(lowBatteryMode ? F("YES") : F("NO"));
        Serial.print(F("Battery: "));
        Serial.print(batteryVoltage, 2);
        Serial.print(F("V ("));
        Serial.print(getBatteryPercent());
        Serial.println(F("%)"));
        Serial.print(F("Free RAM: "));
        Serial.println(freeRam());
        Serial.print(F("Upside down: "));
        Serial.println(isUpsideDown ? F("YES") : F("NO"));
        Serial.print(F("INT1 pin: "));
        Serial.println(digitalRead(PIN_LIS3DH_INT1) ? F("HIGH") : F("LOW"));
        
        Serial.print(F("\nTotal: "));
        Serial.print(getTotalCubeCount());
        Serial.print(F(" cubes, "));
        Serial.print(getTotalLedCount());
        Serial.println(F(" LEDs"));
        
        for (int b = 0; b < NUM_BRANCHES; b++) {
            // Count active cubes and LEDs for this branch
            int activeCubes = 0;
            int activeLeds = 0;
            for (int c = 0; c < branches[b].cubeCount; c++) {
                if (branches[b].cubes[c].active) {
                    activeCubes++;
                    activeLeds += branches[b].cubes[c].ledCount;
                }
            }
            
            Serial.print(F("\nBranch "));
            Serial.print(b + 1);
            Serial.print(F(" ("));
            Serial.print(branches[b].name);
            Serial.print(F("): "));
            Serial.print(activeCubes);
            Serial.print(F(" cubes, "));
            Serial.print(activeLeds);
            Serial.println(F(" LEDs"));
            
            for (int c = 0; c < branches[b].cubeCount; c++) {
                if (!branches[b].cubes[c].active) continue;
                Cube* cube = &branches[b].cubes[c];
                Serial.print(F("  ["));
                Serial.print(c);
                Serial.print(F("] "));
                Serial.print(getCubeTypeName(cube->config.cubeType));
                Serial.print(F(" - "));
                Serial.print(cube->ledCount);
                Serial.print(F(" LEDs ("));
                Serial.print(cube->ledStart);
                Serial.print(F("-"));
                Serial.print(cube->ledStart + cube->ledCount - 1);
                Serial.print(F(") ID="));
                Serial.println((unsigned long)(cube->romId & 0xFFFFFFFF), HEX);
            }
        }
    }
    else if (cmd == "scan") {
        Serial.println(F("Scanning all branches..."));
        scanAllBranches();
        Serial.println(F("Done"));
    }
    else if (cmd == "list") {
        for (int b = 0; b < NUM_BRANCHES; b++) {
            Serial.print(F("\nBranch "));
            Serial.print(b + 1);
            Serial.print(F(" ("));
            Serial.print(branches[b].name);
            Serial.println(F("):"));
            
            uint8_t addr[8];
            int count = 0;
            branches[b].oneWire->reset_search();
            while (branches[b].oneWire->search(addr)) {
                if (isDS2431(addr)) {
                    uint64_t romId = addressToId(addr);
                    Serial.print(F("  ["));
                    Serial.print(count);
                    Serial.print(F("] "));
                    for (int i = 0; i < 8; i++) {
                        if (addr[i] < 16) Serial.print('0');
                        Serial.print(addr[i], HEX);
                        if (i < 7) Serial.print(':');
                    }
                    
                    // Check if this device is in the array
                    int arrayIdx = findCubeInBranch(&branches[b], romId);
                    if (arrayIdx >= 0 && branches[b].cubes[arrayIdx].active) {
                        Serial.print(F(" -> Array["));
                        Serial.print(arrayIdx);
                        Serial.print(F("]: "));
                        Serial.print(getCubeTypeName(branches[b].cubes[arrayIdx].config.cubeType));
                        Serial.print(F(", "));
                        Serial.print(branches[b].cubes[arrayIdx].ledCount);
                        Serial.print(F(" LEDs"));
                    }
                    Serial.println();
                    count++;
                }
            }
            if (count == 0) {
                Serial.println(F("  None found"));
            }
        }
    }
    else if (cmd == "unprog") {
        uint64_t unprogrammedIds[MAX_CUBES];
        int unprogCount = 0;
        
        getUnprogrammedDevices(unprogrammedIds, &unprogCount, MAX_CUBES);
        
        if (unprogCount == 0) {
            Serial.println(F("\nNo unprogrammed devices found."));
            Serial.println(F("All detected cubes are programmed!"));
        } else {
            Serial.println(F("\n=== Unprogrammed Devices ==="));
            for (int i = 0; i < unprogCount; i++) {
                Serial.print(F("["));
                Serial.print(i);
                Serial.print(F("] ID: "));
                Serial.print((unsigned long)(unprogrammedIds[i] >> 32), HEX);
                Serial.print(F(":"));
                Serial.println((unsigned long)(unprogrammedIds[i] & 0xFFFFFFFF), HEX);
            }
            Serial.println(F("\nTo program first device:"));
            Serial.println(F("  prog <type> <leds>"));
            Serial.println(F("\nTo program specific device:"));
            Serial.print(F("  prog <type> <leds> "));
            Serial.println((unsigned long)(unprogrammedIds[0] & 0xFFFFFFFF), HEX);
            Serial.println(F("\nTypes: 1=Edge, 2=Center, 3=Hub"));
        }
    }
    else if (cmd == "next") {
        if (lowBatteryMode) {
            Serial.println(F("Low battery - charge to change animations"));
        } else {
            currentAnimation = (currentAnimation + 1) % ANIM_COUNT;
            ledsEnabled = true;
            Serial.print(F("Animation: "));
            Serial.println(getAnimationName(currentAnimation));
        }
    }
    else if (cmd == "on") {
        ledsEnabled = true;
        Serial.println(F("LEDs on"));
    }
    else if (cmd == "off") {
        ledsEnabled = false;
        clearAllLeds();
        FastLED.show();
        Serial.println(F("LEDs off"));
    }
    else if (cmd == "xyz") {
        printAccelData();
    }
    else if (cmd == "tap") {
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
        Serial.println(digitalRead(PIN_LIS3DH_INT1) ? F("HIGH") : F("LOW"));
    }
    else if (cmd == "bat") {
        updateBatteryVoltage();
        Serial.print(F("Battery: "));
        Serial.print(batteryVoltage, 2);
        Serial.print(F("V ("));
        Serial.print(getBatteryPercent());
        Serial.print(F("%) "));
        if (batteryVoltage < LOW_BATTERY_VOLTAGE && batteryVoltage > 0.5) {
            Serial.println(F("- LOW!"));
        } else {
            Serial.println(F("- OK"));
        }
    }
    else if (cmd == "sleep") {
        Serial.println(F("Entering sleep mode..."));
        sleepRequested = true;
    }
    else if (cmd == "prog" || cmd == "prog help") {
        Serial.println(F("\n=== Programming Modular Cubes ==="));
        Serial.println(F("IMPORTANT: Device index is the 1-Wire bus position!"));
        Serial.println(F("Use 'list' to see bus order before programming."));
        Serial.println(F("\nSimple addressing: Just specify branch and device index!"));
        Serial.println(F("Cubes remain modular and work on ANY branch after programming."));
        Serial.println(F("\nUsage: prog <branch> <idx> <type> <leds>"));
        Serial.println(F("\nBranches:"));
        Serial.println(F("  1 = 1W (on-board)"));
        Serial.println(F("  2 = 2W (off-board)"));
        Serial.println(F("  3 = 3W (off-board)"));
        Serial.println(F("\nCube Types:"));
        Serial.println(F("  1 = Edge   - Edge cubes"));
        Serial.println(F("  2 = Center - Center cubes"));
        Serial.println(F("  3 = Hub    - Hub cube (on-board)"));
        Serial.println(F("\nLEDs: Number of WS2812 LEDs (1-100)"));
        Serial.println(F("\nWorkflow:"));
        Serial.println(F("  1. Type 'list' to see 1-Wire bus devices"));
        Serial.println(F("  2. Note device [index] you want to program"));
        Serial.println(F("  3. Program: prog <branch> <idx> <type> <leds>"));
        Serial.println(F("  4. Cube is now portable - works on any branch!"));
        Serial.println(F("\nExamples:"));
        Serial.println(F("  list              - See all devices on each branch"));
        Serial.println(F("  prog 1 0 3 1      - Program branch 1, device [0] as Hub, 1 LED"));
        Serial.println(F("  prog 1 1 1 25     - Program branch 1, device [1] as Edge, 25 LEDs"));
        Serial.println(F("  prog 2 0 2 50     - Program branch 2, device [0] as Center, 50 LEDs"));
    }
    else if (cmd.startsWith("prog ")) {
        // Format: prog <branch> <idx> <type> <leds>
        // Simple addressing, but cube remains modular!
        int branch, idx, type, ledCount;
        
        if (sscanf(cmd.c_str(), "prog %d %d %d %d", &branch, &idx, &type, &ledCount) == 4) {
            // Validate inputs
            if (branch < 1 || branch > NUM_BRANCHES) {
                Serial.println(F("Invalid branch! Use: 1-3"));
                return;
            }
            if (type < 1 || type > 3) {
                Serial.println(F("Invalid type! Use: 1=Edge, 2=Center, 3=Hub"));
                return;
            }
            if (ledCount < 1 || ledCount > 100) {
                Serial.println(F("Invalid LED count! Range: 1-100"));
                return;
            }
            
            // Find device on specified branch
            Branch* br = &branches[branch - 1];
            uint8_t addr[8];
            int count = 0;
            
            br->oneWire->reset_search();
            while (br->oneWire->search(addr)) {
                if (isDS2431(addr)) {
                    if (count == idx) {
                        // Found it! Show user which cube this is BEFORE programming
                        uint64_t romId = addressToId(addr);
                        
                        Serial.print(F("\nFound device ["));
                        Serial.print(idx);
                        Serial.print(F("] on branch "));
                        Serial.print(branch);
                        Serial.print(F(" - ROM ID: "));
                        Serial.println((unsigned long)(romId & 0xFFFFFFFF), HEX);
                        
                        // Read current config
                        CubeConfig oldConfig;
                        if (ds2431ReadPage(br->oneWire, addr, 0, (uint8_t*)&oldConfig)) {
                            Serial.print(F("Current config: "));
                            Serial.print(getCubeTypeName(oldConfig.cubeType));
                            Serial.print(F(" ("));
                            Serial.print(oldConfig.cubeType);
                            Serial.print(F("), "));
                            Serial.print(oldConfig.ledCount);
                            Serial.println(F(" LEDs"));
                        } else {
                            Serial.println(F("Current config: UNREADABLE/UNPROGRAMMED"));
                        }
                        
                        // Program this device
                        CubeConfig config;
                        memset(&config, 0, sizeof(config));
                        config.cubeType = type;
                        config.ledCount = ledCount;
                        config.colorOrder = 0;  // GRB
                        config.brightness = 128;
                        
                        Serial.print(F("\nProgramming as: "));
                        Serial.print(getCubeTypeName(type));
                        Serial.print(F(" ("));
                        Serial.print(type);
                        Serial.print(F("), "));
                        Serial.print(ledCount);
                        Serial.println(F(" LEDs..."));
                        
                        if (ds2431WritePage(br->oneWire, addr, 0, (uint8_t*)&config)) {
                            Serial.println(F("SUCCESS!"));
                            Serial.println(F("Cube is now modular - works on any branch!"));
                            clearWarnedDevice(romId);
                            
                            // Find if this cube already exists in the branch
                            int cubeIdx = findCubeInBranch(br, romId);
                            if (cubeIdx >= 0) {
                                // Cube exists - update its config in place
                                Cube* cube = &br->cubes[cubeIdx];
                                
                                Serial.print(F("Updating slot "));
                                Serial.print(cubeIdx);
                                Serial.print(F(": was "));
                                Serial.print(getCubeTypeName(cube->config.cubeType));
                                Serial.print(F(" ("));
                                Serial.print(cube->config.cubeType);
                                Serial.print(F("), "));
                                Serial.print(cube->ledCount);
                                Serial.print(F(" LEDs -> now "));
                                Serial.print(getCubeTypeName(config.cubeType));
                                Serial.print(F(" ("));
                                Serial.print(config.cubeType);
                                Serial.print(F("), "));
                                Serial.print(config.ledCount);
                                Serial.println(F(" LEDs"));
                                
                                memcpy(&cube->config, &config, sizeof(CubeConfig));
                                cube->ledCount = config.ledCount;
                                cube->active = true;  // Make sure it's active
                                
                                // Rebuild LED layout for entire branch
                                int ledOffset = 0;
                                for (int c = 0; c < br->cubeCount; c++) {
                                    if (br->cubes[c].active) {
                                        br->cubes[c].ledStart = ledOffset;
                                        ledOffset += br->cubes[c].ledCount;
                                    }
                                }
                                br->totalLeds = ledOffset;
                                
                                Serial.println(F("Updated existing cube in branch"));
                                
                                // Dump array state after update
                                Serial.println(F("Array state after update:"));
                                for (int i = 0; i < br->cubeCount; i++) {
                                    Serial.print(F("  ["));
                                    Serial.print(i);
                                    Serial.print(F("] "));
                                    if (br->cubes[i].active) {
                                        Serial.print(getCubeTypeName(br->cubes[i].config.cubeType));
                                        Serial.print(F(" ("));
                                        Serial.print(br->cubes[i].config.cubeType);
                                        Serial.print(F("), "));
                                        Serial.print(br->cubes[i].ledCount);
                                        Serial.println(F(" LEDs"));
                                    } else {
                                        Serial.println(F("INACTIVE"));
                                    }
                                }
                            } else {
                                // New cube - scan will pick it up
                                scanAllBranches();
                            }
                        } else {
                            Serial.println(F("FAILED!"));
                        }
                        return;
                    }
                    count++;
                }
            }
            
            Serial.print(F("Device ["));
            Serial.print(idx);
            Serial.print(F("] not found on branch "));
            Serial.println(branch);
        } else {
            Serial.println(F("Usage: prog <branch> <idx> <type> <leds>"));
            Serial.println(F("Type 'prog' for detailed help"));
        }
    }
    else if (cmd.startsWith("read ")) {
        // Format: read <branch> <idx>
        int branch, idx;
        
        if (sscanf(cmd.c_str(), "read %d %d", &branch, &idx) == 2) {
            if (branch < 1 || branch > NUM_BRANCHES) {
                Serial.println(F("Invalid branch! Use: 1-3"));
                return;
            }
            
            Branch* br = &branches[branch - 1];
            uint8_t addr[8];
            int count = 0;
            
            br->oneWire->reset_search();
            while (br->oneWire->search(addr)) {
                if (isDS2431(addr)) {
                    if (count == idx) {
                        Serial.print(F("\nBranch "));
                        Serial.print(branch);
                        Serial.print(F(" Device ["));
                        Serial.print(idx);
                        Serial.println(F("] config:"));
                        
                        CubeConfig config;
                        if (ds2431ReadPage(br->oneWire, addr, 0, (uint8_t*)&config)) {
                            Serial.print(F("  Type: "));
                            Serial.print(getCubeTypeName(config.cubeType));
                            Serial.print(F(" ("));
                            Serial.print(config.cubeType);
                            Serial.println(F(")"));
                            Serial.print(F("  LEDs: "));
                            Serial.println(config.ledCount);
                        } else {
                            Serial.println(F("  Read failed!"));
                        }
                        return;
                    }
                    count++;
                }
            }
            
            Serial.print(F("Device ["));
            Serial.print(idx);
            Serial.print(F("] not found on branch "));
            Serial.println(branch);
        } else {
            Serial.println(F("Usage: read <branch> <idx>"));
            Serial.println(F("Use 'list' to see devices"));
        }
    }
    else if (cmd.length() > 0) {
        Serial.print(F("Unknown: "));
        Serial.println(cmd);
    }
}