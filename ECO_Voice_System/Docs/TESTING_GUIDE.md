# ECO Voice System - Comprehensive Testing Guide

## Testing Strategy

This guide provides step-by-step testing procedures to validate each component and the complete system integration.

## Phase 1: Component-Level Testing

### Test 1.1: ESP32-S3 Basic Functionality

**Objective:** Verify ESP32-S3 is working and programmable

**Hardware Required:**
- ESP32-S3 board
- USB cable

**Test Code:**
```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32-S3 Test");
    Serial.print("CPU Frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
}

void loop() {
    Serial.println("Hello from ESP32-S3!");
    delay(1000);
}
```

**Expected Result:**
```
ESP32-S3 Test
CPU Frequency: 240 MHz
Hello from ESP32-S3!
Hello from ESP32-S3!
...
```

**Pass Criteria:** ✅ Serial output appears at 115200 baud

---

### Test 1.2: LED Status Indicators

**Objective:** Verify LED connections and GPIO control

**Connections:**
```
GPIO 12 → Green LED (+) → 220Ω → GND
GPIO 13 → Red LED (+) → 220Ω → GND
```

**Test Code:**
```cpp
#define LED_GREEN 12
#define LED_RED 13

void setup() {
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
}

void loop() {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    delay(1000);

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    delay(1000);
}
```

**Expected Result:**
- Green LED on for 1 second
- Red LED on for 1 second
- Alternating pattern

**Pass Criteria:** ✅ Both LEDs blink alternately

**Troubleshooting:**
- LED doesn't light: Check polarity (long leg = anode = +)
- Dim light: Check resistor value (220Ω recommended)
- No light: Test with multimeter (should read ~3V when HIGH)

---

### Test 1.3: I2C Bus (INA219 Current Sensor)

**Objective:** Verify I2C communication and INA219 sensor

**Connections:**
```
GPIO 8  → INA219 SDA
GPIO 9  → INA219 SCL
3.3V    → INA219 VCC
GND     → INA219 GND
```

**Test Code:**
```cpp
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9); // SDA=8, SCL=9

    Serial.println("INA219 Test");

    if (!ina219.begin()) {
        Serial.println("Failed to find INA219 chip");
        while (1) { delay(10); }
    }

    Serial.println("INA219 Found!");
}

void loop() {
    float shuntVoltage = ina219.getShuntVoltage_mV();
    float busVoltage = ina219.getBusVoltage_V();
    float current = ina219.getCurrent_mA();
    float power = ina219.getPower_mW();

    Serial.print("Bus Voltage:   "); Serial.print(busVoltage); Serial.println(" V");
    Serial.print("Shunt Voltage: "); Serial.print(shuntVoltage); Serial.println(" mV");
    Serial.print("Current:       "); Serial.print(current); Serial.println(" mA");
    Serial.print("Power:         "); Serial.print(power); Serial.println(" mW");
    Serial.println("");

    delay(2000);
}
```

**Expected Result (no load):**
```
INA219 Found!
Bus Voltage:   0.00 V
Shunt Voltage: 0.00 mV
Current:       0.00 mA
Power:         0.00 mW
```

**With 12V load connected:**
```
Bus Voltage:   12.08 V
Shunt Voltage: 1.52 mV
Current:       152.00 mA
Power:         1836.00 mW
```

**Pass Criteria:** ✅ Sensor detected and reading values

**Troubleshooting:**
- "Failed to find": Check SDA/SCL connections
- Wrong values: Verify power connections (VIN+/VIN-)
- No response: Run I2C scanner to find address

**I2C Scanner Code:**
```cpp
#include <Wire.h>

void setup() {
    Wire.begin(8, 9);
    Serial.begin(115200);
    Serial.println("I2C Scanner");

    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("Device found at address 0x");
            Serial.println(addr, HEX);
        }
    }
}

void loop() {}
```

---

### Test 1.4: PIR Motion Sensor

**Objective:** Verify PIR sensor detection

**Connections:**
```
GPIO 4 → PIR OUT
5V     → PIR VCC
GND    → PIR GND
```

**Test Code:**
```cpp
#define PIR_PIN 4

void setup() {
    Serial.begin(115200);
    pinMode(PIR_PIN, INPUT);
    Serial.println("PIR Motion Sensor Test");
    delay(2000); // Allow PIR to stabilize
    Serial.println("Move in front of sensor...");
}

void loop() {
    int pirState = digitalRead(PIR_PIN);

    if (pirState == HIGH) {
        Serial.println("MOTION DETECTED!");
    } else {
        Serial.println("No motion");
    }

    delay(500);
}
```

**Expected Result:**
- "No motion" when still
- "MOTION DETECTED!" when moving near sensor

**Pass Criteria:** ✅ Detects motion within 3-7 meters

**Calibration:**
- Sensitivity potentiometer: Adjust detection range
- Time delay potentiometer: Adjust trigger duration
- Test in different lighting conditions

---

### Test 1.5: LDR Light Sensor

**Objective:** Verify LDR analog readings

**Connections:**
```
GPIO 1 (ADC) → LDR AO
3.3V         → LDR VCC
GND          → LDR GND
```

**Test Code:**
```cpp
#define LDR_PIN 1

void setup() {
    Serial.begin(115200);
    pinMode(LDR_PIN, INPUT);
    analogSetAttenuation(ADC_11db); // 0-3.3V range
    Serial.println("LDR Light Sensor Test");
}

void loop() {
    int lightLevel = analogRead(LDR_PIN);

    Serial.print("Light Level: ");
    Serial.print(lightLevel);

    if (lightLevel < 500) {
        Serial.println(" - DARK");
    } else if (lightLevel < 2000) {
        Serial.println(" - DIM");
    } else {
        Serial.println(" - BRIGHT");
    }

    delay(500);
}
```

**Expected Result:**
- Dark room: 0-500
- Normal lighting: 500-2000
- Bright light/sunlight: 2000-4095

**Pass Criteria:** ✅ Values change when covering/exposing sensor

**Calibration:**
- Note values in your environment
- Adjust BRIGHTNESS_THRESHOLD in config.h accordingly

---

### Test 1.6: Relay Module

**Objective:** Verify relay switching

**Connections:**
```
GPIO 10 → Relay IN1
GPIO 11 → Relay IN2
5V      → Relay VCC
GND     → Relay GND
```

**Safety:** Test with LED loads first, not AC mains!

**Test Code:**
```cpp
#define RELAY1 10
#define RELAY2 11

void setup() {
    Serial.begin(115200);
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);

    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    Serial.println("Relay Module Test");
}

void loop() {
    Serial.println("Relay 1 ON");
    digitalWrite(RELAY1, HIGH);
    delay(2000);

    Serial.println("Relay 1 OFF");
    digitalWrite(RELAY1, LOW);
    delay(1000);

    Serial.println("Relay 2 ON");
    digitalWrite(RELAY2, HIGH);
    delay(2000);

    Serial.println("Relay 2 OFF");
    digitalWrite(RELAY2, LOW);
    delay(1000);
}
```

**Expected Result:**
- Audible "click" from relay
- LED on relay board lights up
- Connected load turns on/off

**Pass Criteria:** ✅ Both relays switch independently

**Troubleshooting:**
- No click: Check VCC is 5V (not 3.3V)
- Always on: Try inverting logic (HIGH ↔ LOW)
- Erratic: Add 0.1µF capacitor across VCC-GND

---

### Test 1.7: I2S Microphone (INMP441)

**Objective:** Verify I2S audio capture

**Connections:**
```
GPIO 41 → INMP441 SCK
GPIO 42 → INMP441 WS
GPIO 2  → INMP441 SD
3.3V    → INMP441 VDD
GND     → INMP441 GND
GND     → INMP441 L/R
```

**Test Code:**
```cpp
#include <driver/i2s.h>

#define I2S_SCK 41
#define I2S_WS 42
#define I2S_SD 2

void setup() {
    Serial.begin(115200);

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    Serial.println("I2S Microphone Test");
    Serial.println("Speak into microphone...");
}

void loop() {
    int16_t buffer[512];
    size_t bytesRead = 0;

    i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);

    // Calculate audio energy (volume)
    long sum = 0;
    for (int i = 0; i < 512; i++) {
        sum += abs(buffer[i]);
    }
    float energy = sum / 512.0;

    Serial.print("Audio Energy: ");
    Serial.println(energy);

    delay(100);
}
```

**Expected Result:**
- Silence: Energy < 1000
- Normal speech: Energy 5000-20000
- Loud noise: Energy > 20000

**Pass Criteria:** ✅ Energy increases when speaking

**Troubleshooting:**
- Always zero: Check SCK/WS/SD pins
- Always high: Check L/R pin connected to GND
- Noisy: Add 0.1µF capacitor at VDD pin

---

## Phase 2: Integration Testing

### Test 2.1: Complete Hardware Integration

**Objective:** All components working together

**Procedure:**
1. Connect all components as per HARDWARE_DIAGRAM.txt
2. Upload full eco_voice_main.ino code
3. Open Serial Monitor at 115200 baud

**Expected Boot Sequence:**
```
ECO Voice System Starting...
Initializing Sensors...
All sensors initialized successfully
Initializing Appliance Control...
Appliance Control Ready
Audio Handler Initialized
Voice Recognition Ready
System Ready. Say 'ECO' to wake up!
Status: LOCKED (Red LED)
```

**Pass Criteria:** ✅ All components initialize without errors

---

### Test 2.2: State Machine Flow

**Test Scenario 1: Wake → Authenticate → Unlock**

```
User Input          Expected Response              LED State
──────────────────────────────────────────────────────────────
eco                 "Listening for secret code"    Green BLINK
open sesame         "Unlocked. Ready for commands" Green ON
```

**Pass Criteria:** ✅ Transitions through states correctly

**Test Scenario 2: Wrong Code**

```
User Input          Expected Response              LED State
──────────────────────────────────────────────────────────────
eco                 "Listening for secret code"    Green BLINK
wrong code          "Wrong code. Try again"        Green BLINK
open sesame         "Unlocked. Ready for commands" Green ON
```

**Pass Criteria:** ✅ Accepts retries, unlocks with correct code

**Test Scenario 3: Authentication Timeout**

```
Actions                                Expected Response
───────────────────────────────────────────────────────────
Type "eco"                             "Listening for secret code"
Wait 30+ seconds without typing        "Authentication timeout"
                                       Returns to IDLE, Red LED ON
```

**Pass Criteria:** ✅ Times out after 30 seconds

---

### Test 2.3: Command Execution

**Prerequisites:** System unlocked (Green LED ON)

**Test Each Command:**

| Command   | Expected Action | LED | Relay | Pass? |
|-----------|----------------|-----|-------|-------|
| light on  | "Light turned on" | - | CH1 ON | ☐ |
| light off | "Light turned off" | - | CH1 OFF | ☐ |
| fan on    | "Fan turned on" | - | CH2 ON | ☐ |
| fan off   | "Fan turned off" | - | CH2 OFF | ☐ |
| status    | Sensor readings | - | - | ☐ |
| lock      | "System locked" | Red ON | - | ☐ |

**Pass Criteria:** ✅ All commands execute correctly

---

### Test 2.4: Sensor-Based Logic

**Test 2.4a: Motion Detection Check**

**Setup:**
1. Ensure room is still (no motion)
2. System unlocked

**Procedure:**
```
Type: light on

Expected:
- System checks PIR sensor
- "No motion detected. Do you still want to continue?"
- Wait for user response

Type: yes
- Light turns on

Type: light off
Type: light on
Type: no
- Light does not turn on
```

**Pass Criteria:** ✅ Asks for confirmation when no motion

**Test 2.4b: Brightness Check**

**Setup:**
1. Ensure room is bright (open curtains, turn on lights)
2. System unlocked

**Procedure:**
```
Type: light on

Expected:
- System checks LDR sensor
- "It's bright already. Do you still want to switch on light?"

Type: yes
- Light turns on
```

**Pass Criteria:** ✅ Asks for confirmation in bright conditions

**Test 2.4c: Current Monitoring**

**Setup:**
1. Simulate high current (add resistive load or adjust threshold)
2. System unlocked

**Procedure:**
```
Type: fan on

Expected:
- System checks INA219
- "High current detected. Do you still want to turn on fan?"

Type: yes
- Fan turns on (with caution)
```

**Pass Criteria:** ✅ Warns and confirms for high current

---

### Test 2.5: Status Reporting

**Procedure:**
```
Type: status
```

**Expected Output (example):**
```
Motion: detected.
Light level: 1523.
Current: 0.15 amps.
Voltage: 12.08 volts.
Light is off.
Fan is off.
```

**Pass Criteria:** ✅ All sensor values reported accurately

---

## Phase 3: Stress & Edge Case Testing

### Test 3.1: Continuous Operation

**Objective:** Verify 24-hour stability

**Procedure:**
1. Upload code
2. Power on ESP32-S3
3. Let run for 24 hours
4. Check for crashes, memory leaks, or errors

**Monitor:**
- Serial output for errors
- LED status changes
- Memory usage (if tracking)

**Pass Criteria:** ✅ No crashes or unexpected behavior

---

### Test 3.2: Rapid Command Sequences

**Objective:** Test rapid state changes

**Procedure:**
```
Type quickly:
light on
light off
light on
fan on
fan off
status
lock
```

**Pass Criteria:** ✅ All commands execute, no state corruption

---

### Test 3.3: Power Cycling

**Objective:** Verify recovery after power loss

**Procedure:**
1. System unlocked, appliances on
2. Disconnect power
3. Reconnect power
4. Check state

**Expected:**
- System boots to IDLE (Red LED)
- Relays OFF (safe state)
- All sensors re-initialize

**Pass Criteria:** ✅ Clean boot, safe defaults

---

### Test 3.4: Invalid Input Handling

**Test invalid commands:**
```
Type: xyz
Expected: "Command not recognized"

Type: 12345
Expected: No response or error message

Type: (very long string)
Expected: Handles gracefully, no crash
```

**Pass Criteria:** ✅ No crashes or undefined behavior

---

## Phase 4: Performance Testing

### Test 4.1: Response Time Measurement

**Objective:** Measure system latency

**Test Points:**
1. Wake word detection → Response time
2. Command input → Relay activation
3. Sensor read → Decision time

**Procedure:**
```cpp
// Add timing code:
unsigned long startTime = millis();
processCommand(command);
unsigned long endTime = millis();
Serial.print("Execution time: ");
Serial.println(endTime - startTime);
```

**Target Performance:**
- Wake word response: <500ms
- Command execution: <200ms
- Sensor read: <100ms

**Pass Criteria:** ✅ Meets latency targets

---

### Test 4.2: Memory Usage

**Objective:** Check for memory leaks

**Code:**
```cpp
void loop() {
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    delay(5000);
}
```

**Monitor over time:**
- Initial free heap: ~300KB (varies)
- After 1 hour: Should be stable
- Decreasing = memory leak

**Pass Criteria:** ✅ Stable memory over 24 hours

---

## Test Result Summary

### Component Tests
- [ ] ESP32-S3 Basic (Test 1.1)
- [ ] LED Indicators (Test 1.2)
- [ ] INA219 I2C (Test 1.3)
- [ ] PIR Motion (Test 1.4)
- [ ] LDR Light (Test 1.5)
- [ ] Relay Module (Test 1.6)
- [ ] I2S Microphone (Test 1.7)

### Integration Tests
- [ ] Full Integration (Test 2.1)
- [ ] State Machine (Test 2.2)
- [ ] Commands (Test 2.3)
- [ ] Sensor Logic (Test 2.4)
- [ ] Status Report (Test 2.5)

### Stress Tests
- [ ] 24-hour Operation (Test 3.1)
- [ ] Rapid Commands (Test 3.2)
- [ ] Power Cycling (Test 3.3)
- [ ] Invalid Input (Test 3.4)

### Performance Tests
- [ ] Response Time (Test 4.1)
- [ ] Memory Usage (Test 4.2)

## Troubleshooting Matrix

| Symptom | Test Failed | Check |
|---------|-------------|-------|
| No serial output | 1.1 | USB cable, baud rate, drivers |
| LEDs not working | 1.2 | Polarity, resistors, GPIO |
| INA219 not found | 1.3 | I2C wiring, address, power |
| PIR not detecting | 1.4 | Power, sensitivity, delay |
| LDR always same | 1.5 | Wiring, ADC configuration |
| Relay not clicking | 1.6 | 5V power, logic level |
| Mic no signal | 1.7 | I2S pins, L/R grounding |
| States stuck | 2.2 | Timeout values, logic errors |
| Commands fail | 2.3 | String matching, buffer sizes |
| Sensors wrong | 2.4 | Calibration, thresholds |
| System crashes | 3.1 | Memory leak, buffer overflow |

## Acceptance Criteria

System is ready for deployment when:
- ✅ All component tests pass
- ✅ All integration tests pass
- ✅ At least 90% of stress tests pass
- ✅ Response time <500ms average
- ✅ No memory leaks over 24 hours
- ✅ Safe behavior on power loss
- ✅ User documentation complete

## Final Validation Checklist

### Functionality
- [ ] Wake word detection works reliably
- [ ] Authentication prevents unauthorized access
- [ ] All commands execute correctly
- [ ] Sensors provide accurate readings
- [ ] Smart logic (motion/brightness/current) works
- [ ] Audio feedback clear and timely
- [ ] Visual indicators match system state

### Safety
- [ ] Default state is safe (all OFF)
- [ ] Current monitoring prevents overload
- [ ] User confirmation for edge cases
- [ ] Relay ratings match loads
- [ ] Proper electrical isolation
- [ ] No exposed high voltage

### Reliability
- [ ] 24+ hours continuous operation
- [ ] Handles errors gracefully
- [ ] Recovers from power loss
- [ ] No memory leaks
- [ ] Stable under load

### Documentation
- [ ] All tests documented
- [ ] Known issues listed
- [ ] User manual complete
- [ ] Safety warnings present

---

**Test Report Template:**

```
ECO Voice System - Test Report
Date: _______________
Tester: _______________
Hardware Revision: _______________
Software Version: _______________

Component Tests:     ___/7 PASS
Integration Tests:   ___/5 PASS
Stress Tests:        ___/4 PASS
Performance Tests:   ___/2 PASS

TOTAL:               ___/18 PASS

Issues Found:
1. ________________________________
2. ________________________________

Recommendations:
1. ________________________________
2. ________________________________

Overall Status: PASS / FAIL / NEEDS WORK
```

---

**Testing Complete!** 🎉

If all tests pass, your ECO Voice system is ready for production use.
For voice recognition integration, proceed to ESP_SR_INTEGRATION.md.
