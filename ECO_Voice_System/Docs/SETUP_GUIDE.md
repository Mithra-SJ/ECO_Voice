# ECO Voice System - Step-by-Step Setup Guide

## Phase 1: Hardware Assembly (1-2 hours)

### Step 1: Prepare Components
- [ ] Verify all components from the parts list
- [ ] Check ESP32-S3 board with USB cable
- [ ] Organize breadboard and jumper wires

### Step 2: Power Supply Setup
```
IMPORTANT: Power considerations
- ESP32-S3: 5V via USB or VIN pin
- Relays: Require 5V supply
- Sensors: Most use 3.3V (check datasheets)
- Use external 5V power supply if USB power is insufficient for relays + motors
```

### Step 3: Connect INMP441 Microphone
```
ESP32-S3          INMP441
GPIO 41    -->    SCK
GPIO 42    -->    WS
GPIO 2     -->    SD
3.3V       -->    VDD
GND        -->    GND
```

### Step 4: Connect PIR Motion Sensor
```
ESP32-S3          HC-SR501
GPIO 4     -->    OUT
5V         -->    VCC
GND        -->    GND

Adjust sensitivity potentiometer for desired range (3-7 meters)
```

### Step 5: Connect LDR Module
```
ESP32-S3          LDR Module
GPIO 1     -->    AO (Analog Out)
3.3V       -->    VCC
GND        -->    GND

Note: GPIO 1 is ADC1_CH0, suitable for analog reading
```

### Step 6: Connect INA219 Current Sensor
```
ESP32-S3          INA219
GPIO 8     -->    SDA
GPIO 9     -->    SCL
3.3V       -->    VCC
GND        -->    GND

Power connections:
VIN+ --> Positive of 12V power supply
VIN- --> Positive terminal of load (motor/fan)
Load negative --> Negative of 12V power supply
```

### Step 7: Connect Relay Module
```
ESP32-S3          2-Channel Relay
GPIO 10    -->    IN1 (Light)
GPIO 11    -->    IN2 (Fan)
5V         -->    VCC
GND        -->    GND

AC Load Connections:
Common (COM) --> Live wire from mains
NO (Normally Open) --> Light/Fan

WARNING: Use proper isolation for AC mains!
For testing, use 12V DC loads instead.
```

### Step 8: Connect Status LEDs
```
ESP32-S3          LED Circuit
GPIO 12    -->    Green LED Anode --> 220Ω --> GND
GPIO 13    -->    Red LED Anode --> 220Ω --> GND
```

### Step 9: Verify Connections
- [ ] Double-check all VCC connections (3.3V vs 5V)
- [ ] Verify GND connections are common
- [ ] Check for short circuits
- [ ] Ensure no loose wires

## Phase 2: Software Installation (30-60 minutes)

### Option A: Arduino IDE Setup (Easier for beginners)

#### Step 1: Install Arduino IDE
```bash
# Download from: https://www.arduino.cc/en/software
# Install Arduino IDE 2.x
```

#### Step 2: Add ESP32 Board Support
1. Open Arduino IDE
2. Go to **File → Preferences**
3. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search "ESP32"
6. Install "esp32 by Espressif Systems" (version 3.0.0 or later)

#### Step 3: Install Required Libraries
1. Go to **Sketch → Include Library → Manage Libraries**
2. Install the following:
   - `Adafruit INA219` (by Adafruit)
   - `Adafruit BusIO` (dependency)

#### Step 4: Configure Board
1. **Tools → Board → esp32 → ESP32S3 Dev Module**
2. **Tools → Port → Select your COM port**
3. Configure settings:
   ```
   Upload Speed: 921600
   USB Mode: Hardware CDC and JTAG
   USB CDC On Boot: Enabled
   USB Firmware MSC On Boot: Disabled
   USB DFU On Boot: Disabled
   Upload Mode: UART0 / Hardware CDC
   CPU Frequency: 240MHz
   Flash Mode: QIO 80MHz
   Flash Size: 8MB
   Partition Scheme: Default 4MB with spiffs
   Core Debug Level: None
   PSRAM: OPI PSRAM
   Arduino Runs On: Core 1
   Events Run On: Core 1
   ```

### Option B: ESP-IDF Setup (Advanced, for ESP-SR support)

#### Step 1: Install ESP-IDF
```bash
# Install prerequisites (Ubuntu/Debian)
sudo apt-get install git wget flex bison gperf python3 python3-pip \
    python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
    dfu-util libusb-1.0-0

# Clone ESP-IDF
cd ~/
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1.2  # Use stable version

# Install ESP-IDF tools
./install.sh esp32s3

# Set up environment variables
. ./export.sh
```

#### Step 2: Install ESP-SR (Speech Recognition)
```bash
cd ~/
git clone --recursive https://github.com/espressif/esp-sr.git
cd esp-sr
```

## Phase 3: Code Deployment (15-30 minutes)

### Step 1: Open the Project
- Navigate to `ECO_Voice_System` folder
- Open `eco_voice_main.ino` in Arduino IDE

### Step 2: Configure Settings
Edit `config.h`:
```cpp
// Change secret code
#define SECRET_CODE "your secret phrase"

// Adjust thresholds if needed
#define BRIGHTNESS_THRESHOLD 600
#define CURRENT_THRESHOLD 2.5
```

### Step 3: Compile and Upload
1. Click **Verify** (✓) to compile
2. Fix any errors (check library installation)
3. Click **Upload** (→) to flash ESP32-S3
4. Wait for "Hard resetting via RTS pin..."

### Step 4: Open Serial Monitor
1. **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. You should see:
   ```
   ECO Voice System Starting...
   Initializing Sensors...
   All sensors initialized successfully
   Initializing Appliance Control...
   Appliance Control Ready
   Audio Handler Initialized
   Voice Recognition Ready
   System Ready. Say 'ECO' to wake up!
   ```

## Phase 4: Testing (30 minutes)

### Test 1: System Boot
- [ ] Power on ESP32-S3
- [ ] Red LED should turn ON (locked state)
- [ ] Serial monitor shows initialization messages

### Test 2: Wake Word Detection (Serial Demo Mode)
- [ ] Type `eco` in Serial Monitor
- [ ] System responds: "Listening for secret code"
- [ ] Green LED blinks 3 times

### Test 3: Authentication
- [ ] Type your secret code (default: `open sesame`)
- [ ] System responds: "Unlocked. Ready for commands"
- [ ] Green LED turns ON solid

### Test 4: Command Testing
Test each command:
- [ ] Type `light on` → Light relay activates
- [ ] Type `light off` → Light relay deactivates
- [ ] Type `fan on` → Fan relay activates
- [ ] Type `fan off` → Fan relay deactivates
- [ ] Type `status` → System reports sensor readings

### Test 5: Sensor Integration

#### PIR Motion Test:
1. Turn off lights in room
2. Try `light on` without motion
3. System should ask for confirmation
4. Wave hand in front of PIR
5. Try `light on` again

#### LDR Brightness Test:
1. Ensure room is well-lit
2. Type `light on`
3. System should detect brightness and ask confirmation

#### Current Sensor Test:
1. Connect motor/fan load
2. Check current reading in status
3. Simulate high current (add resistive load)
4. Try `fan on` - should ask for confirmation

### Test 6: Lock Function
- [ ] Type `lock`
- [ ] System responds: "System locked"
- [ ] Red LED turns ON
- [ ] System returns to idle state

## Phase 5: Voice Recognition Integration (Advanced)

### For Production Voice Control:

#### Option 1: Using ESP-SR (Recommended)
See `ESP_SR_INTEGRATION.md` for detailed guide

#### Option 2: Using DFPlayer Mini for Audio Feedback
1. Connect DFPlayer Mini:
   ```
   ESP32 TX → DFPlayer RX
   ESP32 RX → DFPlayer TX
   ```
2. Prepare SD card with audio files
3. Update `audio_handler.cpp`

## Troubleshooting

### Issue: ESP32 not detected
- Check USB cable (must be data cable, not charge-only)
- Install CH340/CP2102 drivers
- Try different USB port

### Issue: Compilation errors
- Verify all libraries installed
- Check ESP32 board support version
- Update Arduino IDE to latest version

### Issue: I2S microphone not working
- Verify pin connections
- Check if INMP441 requires 3.3V or 5V
- Test with I2S example sketch first

### Issue: INA219 not detected
- Check I2C connections (SDA/SCL)
- Run I2C scanner sketch
- Verify INA219 address (default 0x40)

### Issue: Relays not switching
- Check relay module voltage (5V required)
- Verify IN1/IN2 connections
- Test relay module separately
- Check if relay is active HIGH or LOW

### Issue: Sensors giving wrong readings
- Calibrate LDR threshold in bright/dark conditions
- Adjust PIR sensitivity potentiometer
- Verify INA219 calibration for your load range

## Next Steps

1. **Calibrate Sensors**: Adjust thresholds in `config.h`
2. **Record Audio**: Create feedback messages for DFPlayer
3. **Train Voice Model**: Use ESP-SR to train custom wake word
4. **Enclosure**: Design 3D-printed case for final assembly
5. **Power Optimization**: Implement deep sleep modes
6. **OTA Updates**: Enable wireless firmware updates

## Safety Warnings

⚠️ **HIGH VOLTAGE WARNING**
- AC mains connections should only be done by qualified electricians
- Use proper isolation and enclosures
- Test with low-voltage DC loads first

⚠️ **CURRENT SAFETY**
- Ensure relay ratings match your load
- Add fuses for overcurrent protection
- Monitor INA219 readings for anomalies

⚠️ **THERMAL MANAGEMENT**
- ESP32-S3 can get hot under load
- Ensure adequate ventilation
- Add heatsink if running continuously
