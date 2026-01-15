// =============================================================================
// hardware.cpp - Hardware implementation for LED Cube Hub
// =============================================================================

#include "hardware.h"

// =============================================================================
// Global Hardware Objects (definitions)
// =============================================================================
OneWire oneWire(PIN_ONEWIRE);
CRGB leds[MAX_TOTAL_LEDS];
Adafruit_LIS3DH lis3dh = Adafruit_LIS3DH();

// =============================================================================
// Global State Variables (definitions)
// =============================================================================
Cube cubes[MAX_CUBES];
int cubeCount = 0;
int totalLeds = 0;

uint32_t lastPoll = 0;
uint32_t lastAnim = 0;
uint32_t lastAccel = 0;
uint32_t lastOrientationCheck = 0;
uint8_t animFrame = 0;
uint8_t currentAnimation = 0;
bool animationRunning = true;
bool accelMode = false;
bool lis3dhFound = false;
bool ledsEnabled = true;

volatile bool doubleTapDetected = false;

uint8_t accelR = 0;
uint8_t accelG = 0;
uint8_t accelB = 0;

// Sleep/orientation tracking
bool isUpsideDown = false;
uint32_t firstFlipTime = 0;
int flipCount = 0;
bool sleepRequested = false;

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
void IRAM_ATTR onDoubleTap() {
    doubleTapDetected = true;
}

// =============================================================================
// Free RAM Function
// =============================================================================
int freeRam() {
    return ESP.getFreeHeap();
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
    
    // Configure LIS3DH
    lis3dh.setRange(LIS3DH_RANGE_2_G);
    lis3dh.setDataRate(LIS3DH_DATARATE_100_HZ);
    
    // Configure double-tap detection for LED toggle and wake
    writeReg(0x21, 0x04);  // CTRL_REG2: HP filter enabled for click
    writeReg(0x22, 0x80);  // CTRL_REG3: I1_CLICK enabled
    writeReg(0x24, 0x08);  // CTRL_REG5: LIR_INT1 = 1 (latch interrupt)
    writeReg(0x25, 0x00);  // CTRL_REG6: INT1 active high
    writeReg(0x38, 0x20);  // CLICK_CFG: ZD enabled (double-tap on Z)
    
    // Click timing parameters
    writeReg(0x3A, 0x18);  // CLICK_THS: ~0.38G threshold
    writeReg(0x3B, 0x20);  // TIME_LIMIT: 320ms
    writeReg(0x3C, 0x10);  // TIME_LATENCY: 160ms
    writeReg(0x3D, 0x70);  // TIME_WINDOW: 1120ms
    
    // Configure 6D orientation detection (for sleep trigger)
    writeReg(0x30, 0x7F);  // INT1_CFG: Enable all axes, OR combination, 6D enabled
    writeReg(0x32, 0x20);  // INT1_THS: ~500mg threshold
    writeReg(0x33, 0x02);  // INT1_DURATION: 20ms at 100Hz
    
    // Clear any pending interrupts
    readReg(0x39);  // Read CLICK_SRC
    readReg(0x31);  // Read INT1_SRC
    
    // Setup hardware interrupt
    pinMode(PIN_LIS3DH_INT, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_LIS3DH_INT), onDoubleTap, RISING);
    
    Serial.println(F("  Double-tap detection enabled"));
    Serial.println(F("  Tap Z-axis to toggle LEDs"));
    Serial.println(F("  Flip upside down twice within 2s to sleep"));
    
    // Debug register readback
    Serial.print(F("  CTRL_REG3: 0x")); Serial.println(readReg(0x22), HEX);
    Serial.print(F("  CTRL_REG5: 0x")); Serial.println(readReg(0x24), HEX);
    Serial.print(F("  CLICK_CFG: 0x")); Serial.println(readReg(0x38), HEX);
    Serial.print(F("  INT1_CFG: 0x")); Serial.println(readReg(0x30), HEX);
    
    Serial.print(F("  Range: "));
    switch (lis3dh.getRange()) {
        case LIS3DH_RANGE_2_G:  Serial.println(F("2G")); break;
        case LIS3DH_RANGE_4_G:  Serial.println(F("4G")); break;
        case LIS3DH_RANGE_8_G:  Serial.println(F("8G")); break;
        case LIS3DH_RANGE_16_G: Serial.println(F("16G")); break;
    }
    
    return true;
}

void handleDoubleTap() {
    if (!doubleTapDetected) return;
    doubleTapDetected = false;
    
    // Read CLICK_SRC to get tap info and clear the interrupt
    uint8_t clickSrc = readReg(0x39);
    
    // Debug output
    Serial.print(F("INT fired! CLICK_SRC: 0x"));
    Serial.println(clickSrc, HEX);
    
    // Check if it was a double-tap (bit 5 = DClick)
    if (clickSrc & 0x20) {
        ledsEnabled = !ledsEnabled;
        
        Serial.print(F("Double-tap detected! LEDs: "));
        Serial.println(ledsEnabled ? F("ON") : F("OFF"));
        
        if (!ledsEnabled) {
            fill_solid(leds, MAX_TOTAL_LEDS, CRGB::Black);
            FastLED.show();
        }
    } else if (clickSrc & 0x10) {
        // Single tap detected (bit 4)
        Serial.println(F("Single tap detected (need double-tap)"));
    }
}

void updateAccelerometer() {
    if (!lis3dhFound) return;
    
    lis3dh.read();
    
    int16_t x = lis3dh.x;
    int16_t y = lis3dh.y;
    int16_t z = lis3dh.z;
    
    accelR = constrain(abs(x) / 64, 0, 255);
    accelG = constrain(abs(y) / 64, 0, 255);
    accelB = constrain(abs(z) / 64, 0, 255);
}

void printAccelData() {
    if (!lis3dhFound) {
        Serial.println(F("LIS3DH not available"));
        return;
    }
    
    lis3dh.read();
    
    Serial.println(F("\n=== Accelerometer Data ==="));
    Serial.print(F("Raw X: ")); Serial.print(lis3dh.x);
    Serial.print(F("  Y: ")); Serial.print(lis3dh.y);
    Serial.print(F("  Z: ")); Serial.println(lis3dh.z);
    
    sensors_event_t event;
    lis3dh.getEvent(&event);
    
    Serial.print(F("Accel (m/s²) X: ")); Serial.print(event.acceleration.x, 2);
    Serial.print(F("  Y: ")); Serial.print(event.acceleration.y, 2);
    Serial.print(F("  Z: ")); Serial.println(event.acceleration.z, 2);
    
    Serial.print(F("LED Color -> R: ")); Serial.print(accelR);
    Serial.print(F("  G: ")); Serial.print(accelG);
    Serial.print(F("  B: ")); Serial.println(accelB);
    
    // Orientation info
    Serial.print(F("Upside down: "));
    Serial.println(isUpsideDown ? F("YES") : F("NO"));
    Serial.print(F("Flip count: "));
    Serial.println(flipCount);
}

// =============================================================================
// Sleep Functions
// =============================================================================

void enterDeepSleep() {
    Serial.println(F("\n=== Entering Deep Sleep ==="));
    Serial.println(F("Double-tap to wake up"));
    Serial.flush();
    
    // Fade LEDs to black
    for (int brightness = 100; brightness >= 0; brightness -= 5) {
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(SLEEP_FADE_MS / 20);
    }
    
    // Turn off all LEDs
    fill_solid(leds, MAX_TOTAL_LEDS, CRGB::Black);
    FastLED.show();
    
    // Reconfigure LIS3DH for wake-on-tap only
    writeReg(0x30, 0x00);  // INT1_CFG = 0 (disable 6D)
    writeReg(0x22, 0x80);  // CTRL_REG3: I1_CLICK enabled
    writeReg(0x38, 0x20);  // CLICK_CFG: ZD enabled (double-tap on Z)
    
    // Clear any pending interrupts
    readReg(0x39);  // Read CLICK_SRC
    readReg(0x31);  // Read INT1_SRC
    
    // Configure ESP32-C3 GPIO wakeup
    esp_deep_sleep_enable_gpio_wakeup(BIT(D1), ESP_GPIO_WAKEUP_GPIO_HIGH);
    
    // Small delay to ensure everything is settled
    delay(100);
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

void checkOrientation() {
    if (!lis3dhFound) return;
    
    // Read accelerometer
    lis3dh.read();
    int16_t z = lis3dh.z;
    
    // Check if cube is upside down (Z-axis negative, around -16384 at 2G)
    bool currentlyUpsideDown = (z < -8000);
    
    // Detect transition from right-side-up to upside-down
    if (currentlyUpsideDown && !isUpsideDown) {
        uint32_t now = millis();
        
        if (flipCount == 0) {
            // First flip detected
            flipCount = 1;
            firstFlipTime = now;
            Serial.println(F("First flip detected (upside down)"));
        }
        else if (flipCount == 1 && (now - firstFlipTime) < FLIP_DETECT_WINDOW_MS) {
            // Second flip within time window
            flipCount = 2;
            Serial.println(F("Second flip detected - initiating sleep!"));
            sleepRequested = true;
        }
        else if ((now - firstFlipTime) >= FLIP_DETECT_WINDOW_MS) {
            // Time window expired, restart count
            flipCount = 1;
            firstFlipTime = now;
            Serial.println(F("First flip detected (timer reset)"));
        }
    }
    
    // Reset flip counter if time window expires
    if (flipCount > 0 && (millis() - firstFlipTime) >= FLIP_DETECT_WINDOW_MS) {
        if (flipCount < 2) {
            Serial.println(F("Flip timeout - counter reset"));
        }
        flipCount = 0;
    }
    
    isUpsideDown = currentlyUpsideDown;
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

bool ds2431ReadPage(uint8_t* addr, uint8_t page, uint8_t* buffer) {
    if (!oneWire.reset()) return false;
    
    oneWire.select(addr);
    oneWire.write(0xF0);
    oneWire.write(page * 32);
    oneWire.write(0x00);
    
    for (int i = 0; i < 32; i++) {
        buffer[i] = oneWire.read();
    }
    return true;
}

bool ds2431Write8(uint8_t* addr, uint8_t offset, uint8_t* data) {
    if (!oneWire.reset()) return false;
    oneWire.select(addr);
    oneWire.write(0x0F);
    oneWire.write(offset);
    oneWire.write(0x00);
    for (int i = 0; i < 8; i++) {
        oneWire.write(data[i]);
    }
    
    if (!oneWire.reset()) return false;
    oneWire.select(addr);
    oneWire.write(0xAA);
    uint8_t ta1 = oneWire.read();
    uint8_t ta2 = oneWire.read();
    uint8_t es = oneWire.read();
    
    for (int i = 0; i < 8; i++) {
        if (oneWire.read() != data[i]) return false;
    }
    
    if (!oneWire.reset()) return false;
    oneWire.select(addr);
    oneWire.write(0x55);
    oneWire.write(ta1);
    oneWire.write(ta2);
    oneWire.write(es);
    
    delay(15);
    return (oneWire.read() == 0xAA);
}

bool ds2431WritePage(uint8_t* addr, uint8_t page, uint8_t* data) {
    uint8_t offset = page * 32;
    for (int chunk = 0; chunk < 4; chunk++) {
        if (!ds2431Write8(addr, offset + (chunk * 8), data + (chunk * 8))) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// Cube Management
// =============================================================================

int findCube(uint64_t romId) {
    for (int i = 0; i < cubeCount; i++) {
        if (cubes[i].romId == romId) return i;
    }
    return -1;
}

bool addCube(uint64_t romId, CubeConfig* config) {
    if (cubeCount >= MAX_CUBES) return false;
    if (totalLeds + config->ledCount > MAX_TOTAL_LEDS) return false;
    
    Cube* cube = &cubes[cubeCount];
    cube->romId = romId;
    memcpy(&cube->config, config, sizeof(CubeConfig));
    cube->ledStart = totalLeds;
    cube->ledCount = config->ledCount;
    cube->active = true;
    
    totalLeds += config->ledCount;
    cubeCount++;
    
    Serial.print(F("Added cube: LEDs "));
    Serial.print(cube->ledStart);
    Serial.print(F("-"));
    Serial.println(cube->ledStart + cube->ledCount - 1);
    
    for (int i = cube->ledStart; i < cube->ledStart + cube->ledCount; i++) {
        leds[i] = CRGB::Green;
    }
    FastLED.show();
    delay(200);
    for (int i = cube->ledStart; i < cube->ledStart + cube->ledCount; i++) {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    
    return true;
}

void removeCube(uint64_t romId) {
    int idx = findCube(romId);
    if (idx < 0) return;
    
    Serial.print(F("Removed cube at index "));
    Serial.println(idx);
    
    for (int i = cubes[idx].ledStart; i < cubes[idx].ledStart + cubes[idx].ledCount; i++) {
        leds[i] = CRGB::Black;
    }
    
    cubes[idx].active = false;
}

// =============================================================================
// 1-Wire Bus Scanning
// =============================================================================

void scanOneWireBus() {
    uint8_t addr[8];
    uint64_t foundIds[MAX_CUBES];
    int foundCount = 0;
    
    oneWire.reset_search();
    while (oneWire.search(addr) && foundCount < MAX_CUBES) {
        if (isDS2431(addr)) {
            foundIds[foundCount++] = addressToId(addr);
        }
    }
    
    for (int i = 0; i < foundCount; i++) {
        if (findCube(foundIds[i]) < 0) {
            Serial.print(F("New device: "));
            Serial.println((unsigned long)(foundIds[i] & 0xFFFFFFFF), HEX);
            
            uint8_t configAddr[8];
            idToAddress(foundIds[i], configAddr);
            
            CubeConfig config;
            if (ds2431ReadPage(configAddr, 0, (uint8_t*)&config)) {
                if (config.ledCount > 0 && config.ledCount <= 100) {
                    addCube(foundIds[i], &config);
                } else {
                    Serial.println(F("  Invalid config - needs programming"));
                }
            } else {
                Serial.println(F("  Read failed"));
            }
        }
    }
    
    for (int i = 0; i < cubeCount; i++) {
        if (!cubes[i].active) continue;
        
        bool stillPresent = false;
        for (int j = 0; j < foundCount; j++) {
            if (cubes[i].romId == foundIds[j]) {
                stillPresent = true;
                break;
            }
        }
        
        if (!stillPresent) {
            removeCube(cubes[i].romId);
        }
    }
}

// =============================================================================
// Animations
// =============================================================================

void runAnimation() {
    if (!animationRunning || !ledsEnabled) return;
    
    if (accelMode) {
        CRGB color = CRGB(accelR, accelG, accelB);
        fill_solid(leds, totalLeds, color);
        animFrame++;
        return;
    }
    
    switch (currentAnimation) {
        case 0:
            for (int i = 0; i < totalLeds; i++) {
                leds[i] = CHSV((animFrame + i * 10) % 256, 255, 200);
            }
            break;
            
        case 1:
            {
                uint8_t brightness = beatsin8(30, 50, 255);
                for (int i = 0; i < totalLeds; i++) {
                    leds[i] = CHSV(160, 255, brightness);
                }
            }
            break;
            
        case 2:
            fadeToBlackBy(leds, totalLeds, 100);
            if (totalLeds > 0) {
                leds[animFrame % totalLeds] = CRGB::Red;
            }
            break;
            
        case 3:
            fadeToBlackBy(leds, totalLeds, 50);
            if (random8() < 80 && totalLeds > 0) {
                leds[random16(totalLeds)] = CRGB::White;
            }
            break;
            
        case 4:
            fill_solid(leds, totalLeds, CRGB::White);
            break;
    }
    
    animFrame++;
}

// =============================================================================
// Hardware Initialization
// =============================================================================

void initializeHardware() {
    // Initialize LIS3DH
    lis3dhFound = initLIS3DH();
    
    // Initialize FastLED with simple configuration
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, MAX_TOTAL_LEDS);
    FastLED.setBrightness(100);
    FastLED.clear();
    FastLED.show();
    delay(100);  // Give FastLED time to stabilize
}
