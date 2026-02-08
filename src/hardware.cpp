// =============================================================================
// hardware.cpp - Hardware abstraction layer implementation
// =============================================================================
// Version 2.3 - Modular cube programming and simplified interface
// =============================================================================

#include "hardware.h"

// =============================================================================
// Global Hardware Objects
// =============================================================================
// 1-Wire Buses
OneWire oneWire_1W(PIN_ONEWIRE_1W);
OneWire oneWire_2W(PIN_ONEWIRE_2W);
OneWire oneWire_3W(PIN_ONEWIRE_3W);

// LED Buffers
CRGB leds_1W[MAX_LEDS_PER_BRANCH];
CRGB leds_2W[MAX_LEDS_PER_BRANCH];
CRGB leds_3W[MAX_LEDS_PER_BRANCH];

// LIS3DH
Adafruit_LIS3DH lis3dh = Adafruit_LIS3DH();

// Branch structures (initialized in initializeHardware)
Branch branches[NUM_BRANCHES];

// =============================================================================
// Global State Variables
// =============================================================================
uint32_t lastPoll = 0;
uint32_t lastAnim = 0;
uint32_t lastAccel = 0;
uint32_t lastOrientationCheck = 0;
uint32_t lastVbat = 0;
uint8_t animFrame = 0;
uint8_t currentAnimation = ANIM_ACCEL;  // Start in accel mode
bool animationRunning = true;
bool lis3dhFound = false;
bool ledsEnabled = true;
bool lowBatteryMode = false;

volatile bool tapDetected = false;

uint8_t accelR = 0;
uint8_t accelG = 0;
uint8_t accelB = 0;

// Sleep/orientation tracking
bool isUpsideDown = false;
uint32_t firstFlipTime = 0;
int flipCount = 0;
bool sleepRequested = false;

// Battery
float batteryVoltage = 0.0;

// Unprogrammed device tracking
static uint64_t warnedUnprogrammedIds[MAX_CUBES] = {0};
static int warnedUnprogrammedCount = 0;

// =============================================================================
// I2C Register Helper Functions
// =============================================================================
void writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(LIS3DH_ADDRESS);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(LIS3DH_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)LIS3DH_ADDRESS, (uint8_t)1);
    return Wire.read();
}

// =============================================================================
// Interrupt Service Routine
// =============================================================================
void IRAM_ATTR onTap() {
    tapDetected = true;
}

// =============================================================================
// Utility Functions
// =============================================================================
int freeRam() {
    return ESP.getFreeHeap();
}

int getTotalCubeCount() {
    int total = 0;
    for (int b = 0; b < NUM_BRANCHES; b++) {
        for (int c = 0; c < branches[b].cubeCount; c++) {
            if (branches[b].cubes[c].active) {
                total++;
            }
        }
    }
    return total;
}

int getTotalLedCount() {
    int total = 0;
    for (int b = 0; b < NUM_BRANCHES; b++) {
        for (int c = 0; c < branches[b].cubeCount; c++) {
            if (branches[b].cubes[c].active) {
                total += branches[b].cubes[c].ledCount;
            }
        }
    }
    return total;
}

const char* getAnimationName(uint8_t anim) {
    switch (anim) {
        case ANIM_ACCEL:    return "Accel (XYZ=RGB)";
        case ANIM_RAINBOW:  return "Rainbow";
        case ANIM_PULSE:    return "Pulse";
        case ANIM_CHASE:    return "Chase";
        case ANIM_SPARKLE:  return "Sparkle";
        case ANIM_SOLID:    return "Solid White";
        default:            return "Unknown";
    }
}

const char* getCubeTypeName(uint8_t type) {
    switch (type) {
        case CUBE_TYPE_EDGE:    return "Edge";
        case CUBE_TYPE_CENTER:  return "Center";
        case CUBE_TYPE_HUB:     return "Hub";
        default:                return "Unknown";
    }
}

// =============================================================================
// Battery Functions
// =============================================================================
void updateBatteryVoltage() {
    // Per Seeed Studio recommendations for XIAO ESP32-C3:
    // - Use analogReadMilliVolts() for factory-calibrated readings
    // - Average 16 samples to remove communication spikes
    // - ADC reference varies ±10% per chip (2500mV nominal, can be 2700mV)
    // - Factory calibration data stored in chip fuses handles this variation
    
    uint32_t vbattSum = 0;
    for(int i = 0; i < 16; i++) {
        vbattSum += analogReadMilliVolts(PIN_VBAT_ADC);  // Factory-calibrated ADC reading
    }
    
    // Convert averaged mV to V, account for 1/2 voltage divider (200k/200k)
    // Formula: Vbatt = 2 * (averaged_mV) / 1000
    batteryVoltage = 2.0 * vbattSum / 16 / 1000.0;
}

int getBatteryPercent() {
    // LiPo: 4.2V = 100%, 3.0V = 0%
    float percent = (batteryVoltage - 3.0) / (4.2 - 3.0) * 100.0;
    return constrain((int)percent, 0, 100);
}

void checkLowBattery() {
    if (batteryVoltage > 0.5 && batteryVoltage < LOW_BATTERY_VOLTAGE) {
        if (!lowBatteryMode) {
            lowBatteryMode = true;
            Serial.println(F("*** LOW BATTERY WARNING ***"));
            Serial.print(F("Battery: "));
            Serial.print(batteryVoltage, 2);
            Serial.println(F("V - Please charge!"));
        }
    } else {
        if (lowBatteryMode) {
            lowBatteryMode = false;
            Serial.println(F("Battery OK - resuming normal operation"));
        }
    }
}

// =============================================================================
// LIS3DH Functions
// =============================================================================
bool initLIS3DH() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    if (!lis3dh.begin(LIS3DH_ADDRESS)) {
        Serial.println(F("LIS3DH not found!"));
        return false;
    }
    
    Serial.println(F("LIS3DH found!"));
    
    lis3dh.setRange(LIS3DH_RANGE_2_G);
    lis3dh.setDataRate(LIS3DH_DATARATE_100_HZ);
    
    // Configure tap detection (both single and double)
    writeReg(0x21, 0x04);  // CTRL_REG2: HP filter for click
    writeReg(0x22, 0x80);  // CTRL_REG3: Click interrupt to INT1
    writeReg(0x24, 0x08);  // CTRL_REG5: Latch INT1
    writeReg(0x25, 0x00);  // CTRL_REG6: INT active high
    writeReg(0x38, 0x2A);  // CLICK_CFG: Single AND double-tap on Z (ZS + ZD)
    writeReg(0x3A, 0x28);  // CLICK_THS: 0x18 ~0.38G threshold, 0x20 ~0.5G, 0x28 ~0.625G, 0X1C ~0.44G
    writeReg(0x3B, 0x20);  // TIME_LIMIT: Longer time limit
    writeReg(0x3C, 0x10);  // TIME_LATENCY: Shorter latency
    writeReg(0x3D, 0x70);  // TIME_WINDOW: Longer window
    
    // Clear any pending interrupt
    readReg(0x39);
    
    // Setup hardware interrupt on INT1
    pinMode(PIN_LIS3DH_INT1, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_LIS3DH_INT1), onTap, RISING);
    
    // Setup INT2 for wake (future use)
    pinMode(PIN_LIS3DH_INT2, INPUT);
    
    Serial.println(F("  Tap detection enabled on INT1 (D1)"));
    Serial.println(F("  Single-tap = next animation"));
    Serial.println(F("  Double-tap = toggle LEDs on/off"));
    Serial.print(F("  CLICK_CFG: 0x")); Serial.println(readReg(0x38), HEX);
    
    return true;
}

void handleTap() {
    if (!tapDetected) return;
    tapDetected = false;
    
    // Don't process taps in low battery mode
    if (lowBatteryMode) {
        Serial.println(F("Low battery - charge to resume normal operation"));
        return;
    }
    
    uint8_t clickSrc = readReg(0x39);
    
    // Check for double-tap (bit 5 = DCLICK)
    if (clickSrc & 0x20) {
        ledsEnabled = !ledsEnabled;
        
        Serial.print(F("Double-tap! LEDs: "));
        Serial.println(ledsEnabled ? F("ON") : F("OFF"));
        
        if (!ledsEnabled) {
            clearAllLeds();
            FastLED.show();
        }
    } 
    // Check for single-tap (bit 4 = SCLICK)
    else if (clickSrc & 0x10) {
        if (ledsEnabled) {
            currentAnimation = (currentAnimation + 1) % ANIM_COUNT;
            Serial.print(F("Single-tap! Animation: "));
            Serial.println(getAnimationName(currentAnimation));
        }
    }
}

void updateAccelerometer() {
    if (!lis3dhFound) return;
    
    lis3dh.read();
    
    accelR = constrain(abs(lis3dh.x) / 64, 0, 255);
    accelG = constrain(abs(lis3dh.y) / 64, 0, 255);
    accelB = constrain(abs(lis3dh.z) / 64, 0, 255);
}

void printAccelData() {
    if (!lis3dhFound) {
        Serial.println(F("LIS3DH not available"));
        return;
    }
    
    lis3dh.read();
    
    Serial.println(F("\n=== Accelerometer ==="));
    Serial.print(F("Raw X: ")); Serial.print(lis3dh.x);
    Serial.print(F("  Y: ")); Serial.print(lis3dh.y);
    Serial.print(F("  Z: ")); Serial.println(lis3dh.z);
    
    sensors_event_t event;
    lis3dh.getEvent(&event);
    
    Serial.print(F("m/s² X: ")); Serial.print(event.acceleration.x, 2);
    Serial.print(F("  Y: ")); Serial.print(event.acceleration.y, 2);
    Serial.print(F("  Z: ")); Serial.println(event.acceleration.z, 2);
    
    Serial.print(F("RGB -> R: ")); Serial.print(accelR);
    Serial.print(F("  G: ")); Serial.print(accelG);
    Serial.print(F("  B: ")); Serial.println(accelB);
}

// =============================================================================
// Sleep Functions
// =============================================================================
void checkOrientation() {
    if (!lis3dhFound) return;
    
    lis3dh.read();
    
    // Check if upside down (Z strongly negative)
    bool currentlyUpsideDown = (lis3dh.z < -13000);  // was -12000 too sensitive
    
    if (currentlyUpsideDown && !isUpsideDown) {
        // Just flipped upside down
        uint32_t now = millis();
        
        if (flipCount == 0 || (now - firstFlipTime) > FLIP_DETECT_WINDOW_MS) {
            // Start new flip sequence
            firstFlipTime = now;
            flipCount = 1;
            Serial.println(F("Flip detected (1/3)"));
        } else if (flipCount == 1 && (now - firstFlipTime) <= FLIP_DETECT_WINDOW_MS) {
            // Second flip within window
            flipCount = 2;
            Serial.println(F("Flip detected (2/3)"));
        } else if (flipCount == 2 && (now - firstFlipTime) <= FLIP_DETECT_WINDOW_MS) {
            // Third flip within window
            flipCount = 3;
            Serial.println(F("Flip detected (3/3) - SLEEP REQUESTED"));
            sleepRequested = true;
        }
    }
    
    isUpsideDown = currentlyUpsideDown;
    
    // Reset flip count if window expired
    if (flipCount > 0 && (millis() - firstFlipTime) > FLIP_DETECT_WINDOW_MS) {
        flipCount = 0;
    }
}


void enterDeepSleep() {
    Serial.println(F("\n*** Entering deep sleep ***"));
    
    // Red blink warning animation (3 blinks)
    Serial.println(F("Sleep warning - red blinks..."));
    for (int blink = 0; blink < 3; blink++) {
        for (int i = 0; i < NUM_BRANCHES; i++) {
            fill_solid(branches[i].leds, branches[i].totalLeds, CRGB::Red);
        }
        FastLED.setBrightness(100);
        FastLED.show();
        delay(500);
        
        clearAllLeds();
        FastLED.show();
        delay(500);
    }
    
    Serial.println(F("Double-tap to wake up\n"));
    
    // Fade out LEDs
    for (int brightness = 255; brightness >= 0; brightness -= 5) {
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(SLEEP_FADE_MS / 51);
    }
    
    clearAllLeds();
    FastLED.show();
    
    // Configure LIS3DH INT1 for wake on double-tap
    writeReg(0x22, 0x80);  // CTRL_REG3: Click to INT1
    writeReg(0x38, 0x20); // CLICK_CFG: Double-tap on Z (ZD)
    // writeReg(0x24, 0x08);  // CTRL_REG5: Latch INT1
    readReg(0x39);         // Read CLICKSRC Clear any pending click interrupt
    readReg(0x31);      // Read INT1_SRC Clear any pending INT1 interrupt
    //delay(100);
    //Serial.flush();
    
    // ESP32-C3 uses gpio wakeup (D1 = GPIO3)
    esp_deep_sleep_enable_gpio_wakeup(BIT(GPIO_NUM_3), ESP_GPIO_WAKEUP_GPIO_HIGH);
    delay(100); // Allow time for serial to flush and settle
    esp_deep_sleep_start();
    // Never returns
}

// =============================================================================
// DS2431 Functions
// =============================================================================
uint64_t addressToId(uint8_t* addr) {
    uint64_t id = 0;
    for (int i = 0; i < 8; i++) {
        id |= ((uint64_t)addr[i]) << (i * 8);
    }
    return id;
}

void idToAddress(uint64_t id, uint8_t* addr) {
    for (int i = 0; i < 8; i++) {
        addr[i] = (id >> (i * 8)) & 0xFF;
    }
}

bool isDS2431(uint8_t* addr) {
    if (addr[0] != DS2431_FAMILY) return false;
    return (OneWire::crc8(addr, 7) == addr[7]);
}

bool ds2431ReadPage(OneWire* ow, uint8_t* addr, uint8_t page, uint8_t* buffer) {
    if (!ow->reset()) return false;
    
    ow->select(addr);
    ow->write(0xF0);
    ow->write(page * 32);
    ow->write(0x00);
    
    for (int i = 0; i < 32; i++) {
        buffer[i] = ow->read();
    }
    return true;
}

bool ds2431Write8(OneWire* ow, uint8_t* addr, uint8_t offset, uint8_t* data) {
    if (!ow->reset()) return false;
    ow->select(addr);
    ow->write(0x0F);
    ow->write(offset);
    ow->write(0x00);
    for (int i = 0; i < 8; i++) {
        ow->write(data[i]);
    }
    
    if (!ow->reset()) return false;
    ow->select(addr);
    ow->write(0xAA);
    uint8_t ta1 = ow->read();
    uint8_t ta2 = ow->read();
    uint8_t es = ow->read();
    
    for (int i = 0; i < 8; i++) {
        if (ow->read() != data[i]) return false;
    }
    
    if (!ow->reset()) return false;
    ow->select(addr);
    ow->write(0x55);
    ow->write(ta1);
    ow->write(ta2);
    ow->write(es);
    
    delay(15);
    return (ow->read() == 0xAA);
}

bool ds2431WritePage(OneWire* ow, uint8_t* addr, uint8_t page, uint8_t* data) {
    uint8_t offset = page * 32;
    for (int chunk = 0; chunk < 4; chunk++) {
        if (!ds2431Write8(ow, addr, offset + (chunk * 8), data + (chunk * 8))) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// Cube Management Functions
// =============================================================================
int findCubeInBranch(Branch* branch, uint64_t romId) {
    for (int i = 0; i < branch->cubeCount; i++) {
        if (branch->cubes[i].romId == romId) return i;
    }
    return -1;
}

bool addCubeToBranch(Branch* branch, uint8_t branchIdx, uint64_t romId, CubeConfig* config) {
    if (branch->cubeCount >= MAX_CUBES_PER_BRANCH) return false;
    
    // Recalculate total LEDs from active cubes
    int activeLeds = 0;
    for (int c = 0; c < branch->cubeCount; c++) {
        if (branch->cubes[c].active) {
            activeLeds += branch->cubes[c].ledCount;
        }
    }
    
    if (activeLeds + config->ledCount > MAX_LEDS_PER_BRANCH) return false;
    
    Cube* cube = &branch->cubes[branch->cubeCount];
    cube->romId = romId;
    memcpy(&cube->config, config, sizeof(CubeConfig));
    cube->branch = branchIdx;
    cube->ledStart = activeLeds;  // Start after active LEDs
    cube->ledCount = config->ledCount;
    cube->active = true;
    
    branch->totalLeds = activeLeds + config->ledCount;
    branch->cubeCount++;
    
    Serial.print(F("Added: "));
    Serial.print(branch->name);
    Serial.print(F(" - "));
    Serial.print(getCubeTypeName(config->cubeType));
    Serial.print(F(", "));
    Serial.print(cube->ledCount);
    Serial.println(F(" LEDs"));
    
    // Flash green
    for (int i = cube->ledStart; i < cube->ledStart + cube->ledCount; i++) {
        branch->leds[i] = CRGB::Green;
    }
    FastLED.show();
    delay(200);
    for (int i = cube->ledStart; i < cube->ledStart + cube->ledCount; i++) {
        branch->leds[i] = CRGB::Black;
    }
    FastLED.show();
    
    return true;
}

void removeCubeFromBranch(Branch* branch, uint64_t romId) {
    int idx = findCubeInBranch(branch, romId);
    if (idx < 0) return;
    
    Cube* cube = &branch->cubes[idx];
    Serial.print(F("Removed: "));
    Serial.print(branch->name);
    Serial.print(F(" - "));
    Serial.print(getCubeTypeName(cube->config.cubeType));
    Serial.print(F(", "));
    Serial.print(cube->ledCount);
    Serial.println(F(" LEDs"));
    
    for (int i = cube->ledStart; i < cube->ledStart + cube->ledCount; i++) {
        branch->leds[i] = CRGB::Black;
    }
    
    cube->active = false;
    
    // Rebuild LED indices to eliminate gaps
    int ledOffset = 0;
    for (int c = 0; c < branch->cubeCount; c++) {
        if (branch->cubes[c].active) {
            branch->cubes[c].ledStart = ledOffset;
            ledOffset += branch->cubes[c].ledCount;
        }
    }
    branch->totalLeds = ledOffset;
}

void scanBranch(Branch* branch, uint8_t branchIdx) {
    uint8_t addr[8];
    uint64_t foundIds[MAX_CUBES_PER_BRANCH];
    int foundCount = 0;
    
    branch->oneWire->reset_search();
    while (branch->oneWire->search(addr) && foundCount < MAX_CUBES_PER_BRANCH) {
        if (isDS2431(addr)) {
            foundIds[foundCount++] = addressToId(addr);
        }
    }
    
    bool changesDetected = false;
    
    // Check for new devices
    for (int i = 0; i < foundCount; i++) {
        int existingIdx = findCubeInBranch(branch, foundIds[i]);
        
        if (existingIdx >= 0) {
            // Cube already exists in branch
            Cube* cube = &branch->cubes[existingIdx];
            
            if (!cube->active) {
                // Cube was previously removed but is back - reactivate it
                changesDetected = true;
                cube->active = true;
                
                Serial.print(F("Reactivated: "));
                Serial.print(branch->name);
                Serial.print(F(" - "));
                Serial.print(getCubeTypeName(cube->config.cubeType));
                Serial.print(F(", "));
                Serial.print(cube->ledCount);
                Serial.println(F(" LEDs"));
                
                // Rebuild LED layout
                int ledOffset = 0;
                for (int c = 0; c < branch->cubeCount; c++) {
                    if (branch->cubes[c].active) {
                        branch->cubes[c].ledStart = ledOffset;
                        ledOffset += branch->cubes[c].ledCount;
                    }
                }
                branch->totalLeds = ledOffset;
            }
            // If already active, do nothing - cube is good
        }
        else {
            changesDetected = true;
            // New cube - check if we've already warned about this device
            bool alreadyWarned = false;
            for (int w = 0; w < warnedUnprogrammedCount; w++) {
                if (warnedUnprogrammedIds[w] == foundIds[i]) {
                    alreadyWarned = true;
                    break;
                }
            }
            
            uint8_t configAddr[8];
            idToAddress(foundIds[i], configAddr);
            
            CubeConfig config;
            if (ds2431ReadPage(branch->oneWire, configAddr, 0, (uint8_t*)&config)) {
                if (config.ledCount > 0 && config.ledCount <= 100) {
                    addCubeToBranch(branch, branchIdx, foundIds[i], &config);
                    // Remove from warned list if it was there
                    for (int w = 0; w < warnedUnprogrammedCount; w++) {
                        if (warnedUnprogrammedIds[w] == foundIds[i]) {
                            warnedUnprogrammedIds[w] = warnedUnprogrammedIds[--warnedUnprogrammedCount];
                            break;
                        }
                    }
                } else if (!alreadyWarned) {
                    // Unprogrammed device - warn once
                    Serial.print(F("\n*** Unprogrammed device on "));
                    Serial.print(branch->name);
                    Serial.println(F(" ***"));
                    Serial.print(F("ID: "));
                    Serial.println((unsigned long)(foundIds[i] & 0xFFFFFFFF), HEX);
                    Serial.println(F("Use 'prog' command to program it."));
                    Serial.print(F("Example: prog "));
                    Serial.print(branchIdx + 1);
                    Serial.println(F(" 0 1 25\n"));
                    
                    // Add to warned list
                    if (warnedUnprogrammedCount < MAX_CUBES) {
                        warnedUnprogrammedIds[warnedUnprogrammedCount++] = foundIds[i];
                    }
                }
            } else if (!alreadyWarned) {
                Serial.print(F("Read failed on branch "));
                Serial.println(branch->name);
                
                if (warnedUnprogrammedCount < MAX_CUBES) {
                    warnedUnprogrammedIds[warnedUnprogrammedCount++] = foundIds[i];
                }
            }
        }
    }
    
    // Check for removed devices
    for (int i = 0; i < branch->cubeCount; i++) {
        if (!branch->cubes[i].active) continue;
        
        bool stillPresent = false;
        for (int j = 0; j < foundCount; j++) {
            if (branch->cubes[i].romId == foundIds[j]) {
                stillPresent = true;
                break;
            }
        }
        
        if (!stillPresent) {
            changesDetected = true;
            removeCubeFromBranch(branch, branch->cubes[i].romId);
            // Also remove from warned list if present
            for (int w = 0; w < warnedUnprogrammedCount; w++) {
                if (warnedUnprogrammedIds[w] == branch->cubes[i].romId) {
                    warnedUnprogrammedIds[w] = warnedUnprogrammedIds[--warnedUnprogrammedCount];
                    break;
                }
            }
        }
    }
}

void clearWarnedDevice(uint64_t romId) {
    for (int w = 0; w < warnedUnprogrammedCount; w++) {
        if (warnedUnprogrammedIds[w] == romId) {
            warnedUnprogrammedIds[w] = warnedUnprogrammedIds[--warnedUnprogrammedCount];
            break;
        }
    }
}

void getUnprogrammedDevices(uint64_t* ids, int* count, int maxCount) {
    *count = 0;
    
    for (int b = 0; b < NUM_BRANCHES && *count < maxCount; b++) {
        uint8_t addr[8];
        branches[b].oneWire->reset_search();
        
        while (branches[b].oneWire->search(addr) && *count < maxCount) {
            if (isDS2431(addr)) {
                uint64_t romId = addressToId(addr);
                
                // Check if this device is already in a branch (programmed and active)
                bool isProgrammed = false;
                for (int i = 0; i < branches[b].cubeCount; i++) {
                    if (branches[b].cubes[i].active && branches[b].cubes[i].romId == romId) {
                        isProgrammed = true;
                        break;
                    }
                }
                
                if (!isProgrammed) {
                    // Try to read config to verify it's unprogrammed
                    CubeConfig config;
                    if (ds2431ReadPage(branches[b].oneWire, addr, 0, (uint8_t*)&config)) {
                        if (config.ledCount == 0 || config.ledCount > 100) {
                            ids[(*count)++] = romId;
                        }
                    } else {
                        // Read failed = unprogrammed
                        ids[(*count)++] = romId;
                    }
                }
            }
        }
    }
}

uint64_t findDeviceByPartialId(uint32_t partialId) {
    for (int b = 0; b < NUM_BRANCHES; b++) {
        uint8_t addr[8];
        branches[b].oneWire->reset_search();
        
        while (branches[b].oneWire->search(addr)) {
            if (isDS2431(addr)) {
                uint64_t romId = addressToId(addr);
                uint32_t devicePartial = romId & 0xFFFFFFFF;  // Lower 32 bits
                
                if (devicePartial == partialId) {
                    return romId;
                }
            }
        }
    }
    return 0;  // Not found
}

void scanAllBranches() {
    for (int i = 0; i < NUM_BRANCHES; i++) {
        scanBranch(&branches[i], i);
    }
}

// =============================================================================
// Animation Functions
// =============================================================================
void clearAllLeds() {
    fill_solid(leds_1W, MAX_LEDS_PER_BRANCH, CRGB::Black);
    fill_solid(leds_2W, MAX_LEDS_PER_BRANCH, CRGB::Black);
    fill_solid(leds_3W, MAX_LEDS_PER_BRANCH, CRGB::Black);
}

void runLowBatteryAnimation() {
    // Slow red pulse
    uint8_t brightness = beatsin8(15, 20, 150);  // Slow pulse, dimmer
    CRGB color = CRGB(brightness, 0, 0);
    
    for (int i = 0; i < NUM_BRANCHES; i++) {
        fill_solid(branches[i].leds, branches[i].totalLeds, color);
    }
}

void runAnimationOnBranch(Branch* branch) {
    if (branch->totalLeds == 0) return;
    
    switch (currentAnimation) {
        case ANIM_ACCEL:
            {
                CRGB color = CRGB(accelR, accelG, accelB);
                fill_solid(branch->leds, branch->totalLeds, color);
            }
            break;
            
        case ANIM_RAINBOW:
            for (int i = 0; i < branch->totalLeds; i++) {
                branch->leds[i] = CHSV((animFrame + i * 10) % 256, 255, 200);
            }
            break;
            
        case ANIM_PULSE:
            {
                uint8_t brightness = beatsin8(30, 50, 255);
                for (int i = 0; i < branch->totalLeds; i++) {
                    branch->leds[i] = CHSV(160, 255, brightness);
                }
            }
            break;
            
        case ANIM_CHASE:
            fadeToBlackBy(branch->leds, branch->totalLeds, 100);
            if (branch->totalLeds > 0) {
                branch->leds[animFrame % branch->totalLeds] = CRGB::Red;
            }
            break;
            
        case ANIM_SPARKLE:
            fadeToBlackBy(branch->leds, branch->totalLeds, 50);
            if (random8() < 80 && branch->totalLeds > 0) {
                branch->leds[random16(branch->totalLeds)] = CRGB::White;
            }
            break;
            
        case ANIM_SOLID:
            fill_solid(branch->leds, branch->totalLeds, CRGB::White);
            break;
    }
}

void runAnimation() {
    if (!animationRunning || !ledsEnabled) return;
    
    // Low battery overrides all other animations
    if (lowBatteryMode) {
        runLowBatteryAnimation();
        animFrame++;
        return;
    }
    
    for (int i = 0; i < NUM_BRANCHES; i++) {
        runAnimationOnBranch(&branches[i]);
    }
    
    animFrame++;
}

// =============================================================================
// Hardware Initialization
// =============================================================================
void initializeHardware() {
    // Initialize branch structures
    branches[0].oneWire = &oneWire_1W;
    branches[0].leds = leds_1W;
    branches[0].pinLed = PIN_LED_1W;
    branches[0].pinOneWire = PIN_ONEWIRE_1W;
    branches[0].cubeCount = 0;
    branches[0].totalLeds = 0;
    branches[0].name = "1W (on-board)";
    
    branches[1].oneWire = &oneWire_2W;
    branches[1].leds = leds_2W;
    branches[1].pinLed = PIN_LED_2W;
    branches[1].pinOneWire = PIN_ONEWIRE_2W;
    branches[1].cubeCount = 0;
    branches[1].totalLeds = 0;
    branches[1].name = "2W (off-board)";
    
    branches[2].oneWire = &oneWire_3W;
    branches[2].leds = leds_3W;
    branches[2].pinLed = PIN_LED_3W;
    branches[2].pinOneWire = PIN_ONEWIRE_3W;
    branches[2].cubeCount = 0;
    branches[2].totalLeds = 0;
    branches[2].name = "3W (off-board)";
    
    // Initialize LIS3DH
    lis3dhFound = initLIS3DH();
    
    // Initialize battery ADC
    pinMode(PIN_VBAT_ADC, INPUT);
    analogReadResolution(12);
    updateBatteryVoltage();
    
    // Initialize FastLED for all branches
    FastLED.addLeds<WS2812B, PIN_LED_1W, GRB>(leds_1W, MAX_LEDS_PER_BRANCH);
    FastLED.addLeds<WS2812B, PIN_LED_2W, GRB>(leds_2W, MAX_LEDS_PER_BRANCH);
    FastLED.addLeds<WS2812B, PIN_LED_3W, GRB>(leds_3W, MAX_LEDS_PER_BRANCH);
    FastLED.setBrightness(175); // was 100, increased for better visibility
    clearAllLeds();
    FastLED.show();
    
    Serial.println(F("Hardware initialized:"));
    Serial.print(F("  Branch 1W: LED=D3, 1-Wire=D8 (on-board)\n"));
    Serial.print(F("  Branch 2W: LED=D6, 1-Wire=D10\n"));
    Serial.print(F("  Branch 3W: LED=D9, 1-Wire=D2\n"));
    Serial.print(F("  LIS3DH: SDA=D4, SCL=D5, INT1=D1, INT2=D7\n"));
    Serial.print(F("  Battery ADC: A0\n"));
    Serial.print(F("  Battery: "));
    Serial.print(batteryVoltage, 2);
    Serial.print(F("V ("));
    Serial.print(getBatteryPercent());
    Serial.println(F("%)"));
    Serial.print(F("  Starting mode: "));
    Serial.println(getAnimationName(currentAnimation));
}