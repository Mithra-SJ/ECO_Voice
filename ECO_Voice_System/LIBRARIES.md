# Required Libraries and Dependencies

## Arduino IDE Libraries

### Essential Libraries (Required)
Install these through Arduino IDE Library Manager:

1. **Adafruit INA219** (v1.2.0 or later)
   - Purpose: Current and voltage sensing
   - Author: Adafruit
   - Install: Sketch → Include Library → Manage Libraries → Search "Adafruit INA219"

2. **Adafruit BusIO** (v1.14.0 or later)
   - Purpose: I2C/SPI abstraction layer (dependency for INA219)
   - Author: Adafruit
   - Install: Auto-installed with Adafruit INA219

3. **DHT sensor library** (v1.4.4 or later)
   - Purpose: Temperature and humidity readings from DHT11 (fan control gate)
   - Author: Adafruit
   - Install: Sketch → Include Library → Manage Libraries → Search "DHT sensor library"
   - Note: Also installs **Adafruit Unified Sensor** (v1.1.9+) as a dependency — accept when prompted

### Built-in Libraries (No installation needed)
These are included with ESP32 Arduino core:

- **Wire.h** - I2C communication
- **driver/i2s.h** - I2S audio interface
- **Arduino.h** - Core Arduino functions

## ESP-IDF Components (For Production Voice Recognition)

### ESP-SR (Speech Recognition)
For full voice recognition functionality:

```bash
cd ~/esp
git clone --recursive https://github.com/espressif/esp-sr.git
```

**Components:**
- **WakeNet** - Wake word detection engine
- **MultiNet** - Command word recognition
- **AFE** - Audio Front End (noise suppression, AEC)

**Version**: Latest stable (tested with v1.4.0)

### ESP-IDF Framework
Required for ESP-SR integration:

```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1.2  # Recommended stable version
./install.sh esp32s3
. ./export.sh
```

## Optional Libraries (For Enhanced Features)

### Audio Playback

#### Option 1: DFPlayer Mini (MP3 Playback)
```
Library: DFRobotDFPlayerMini
Install: Library Manager → Search "DFPlayer"
Purpose: Play pre-recorded voice messages
```

#### Option 2: ESP32-audioI2S
```
Library: ESP32-audioI2S
Install: Library Manager → Search "ESP32 audio"
Purpose: I2S speaker output with WAV/MP3 support
```

### Additional Sensors

#### RTC (Real-Time Clock)

#### RTC (Real-Time Clock)
```
Library: RTClib (Adafruit)
Purpose: Scheduling and time-based automation
```

## Library Installation Methods

### Method 1: Arduino IDE Library Manager (Easiest)

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for library name
4. Click "Install"
5. Wait for installation to complete

### Method 2: Manual Installation (ZIP)

1. Download library ZIP from GitHub
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library**
3. Select downloaded ZIP file
4. Restart Arduino IDE

### Method 3: Git Clone (Advanced)

```bash
cd ~/Arduino/libraries/
git clone https://github.com/adafruit/Adafruit_INA219.git
git clone https://github.com/adafruit/Adafruit_BusIO.git
```

## Dependency Tree

```
ECO Voice System
│
├── Arduino Core (ESP32)
│   ├── Wire (I2C)
│   ├── driver/i2s (Audio)
│   └── Arduino.h (Core)
│
├── Sensor Libraries
│   ├── Adafruit_INA219
│   │   └── Adafruit_BusIO (dependency)
│   └── [Wire.h] (built-in)
│
└── Voice Recognition (Production)
    └── ESP-SR
        ├── WakeNet
        ├── MultiNet
        └── AFE

Optional Additions:
├── DFRobotDFPlayerMini (Audio playback)
├── ESP32-audioI2S (I2S speaker)
└── RTClib (Time-based features)
```

## Version Compatibility

### Tested Versions

| Component | Minimum Version | Recommended | Latest Tested |
|-----------|----------------|-------------|---------------|
| Arduino IDE | 1.8.19 | 2.2.1 | 2.3.0 |
| ESP32 Board Support | 2.0.11 | 3.0.0 | 3.0.1 |
| Adafruit INA219 | 1.0.0 | 1.2.0 | 1.2.2 |
| Adafruit BusIO | 1.10.0 | 1.14.0 | 1.15.0 |
| ESP-IDF | 5.0.0 | 5.1.2 | 5.2.0 |
| ESP-SR | 1.3.0 | 1.4.0 | 1.4.1 |

### Compatibility Notes

- **ESP32 Board Support v3.x**: Recommended for ESP32-S3
- **ESP-IDF v5.1+**: Required for latest ESP-SR features
- **Arduino 2.x**: Better IntelliSense and debugging
- **Adafruit libraries**: Always use matching BusIO version

## Installation Verification

### Test Sketch for Libraries

Create a new sketch and try compiling this:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <driver/i2s.h>

Adafruit_INA219 ina219;

void setup() {
    Serial.begin(115200);

    // Test I2C
    Wire.begin();

    // Test INA219
    if (!ina219.begin()) {
        Serial.println("INA219 library loaded but sensor not found");
    } else {
        Serial.println("All libraries loaded successfully!");
    }
}

void loop() {
    delay(1000);
}
```

**Expected Output:**
```
All libraries loaded successfully!
```
or
```
INA219 library loaded but sensor not found
```
(Second message is OK if sensor not connected)

### Common Installation Issues

#### Issue: "library not found"
**Solution:**
1. Restart Arduino IDE
2. Check library installed in correct folder: `~/Arduino/libraries/`
3. Re-install via Library Manager

#### Issue: "multiple libraries found"
**Solution:**
1. Remove duplicate libraries from:
   - `~/Arduino/libraries/`
   - ESP32 core libraries folder
2. Keep only one version (latest)

#### Issue: "version conflict"
**Solution:**
1. Update ESP32 board support to latest
2. Update all Adafruit libraries
3. Tools → Board → Boards Manager → Update ESP32

## PlatformIO Configuration (Alternative to Arduino IDE)

For users preferring PlatformIO, create `platformio.ini`:

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Serial Monitor
monitor_speed = 115200

; Build flags
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1

; Libraries
lib_deps =
    adafruit/Adafruit INA219@^1.2.0
    adafruit/Adafruit BusIO@^1.14.0

; Upload settings
upload_speed = 921600
```

Install with:
```bash
pio lib install
pio run --target upload
```

## ESP-SR Setup (Detailed)

### Prerequisites
- ESP-IDF v5.1.2 installed
- Python 3.8+
- Git

### Installation Steps

```bash
# 1. Set up environment
cd ~/esp
. ~/esp/esp-idf/export.sh

# 2. Clone ESP-SR
git clone --recursive https://github.com/espressif/esp-sr.git
cd esp-sr

# 3. Install Python dependencies
pip install -r requirements.txt

# 4. Build example to verify
cd examples/wake_word_detection
idf.py set-target esp32s3
idf.py build

# Success if builds without errors
```

### ESP-SR Components in menuconfig

```bash
idf.py menuconfig
```

Navigate to:
```
Component config →
  ESP Speech Recognition →
    [*] Enable WakeNet
    [*] Enable MultiNet
    Wake Word Model: WakeNet9
    Command Recognition Model: MultiNet (English)
```

## License Information

### Open Source Libraries
- **Adafruit INA219**: BSD License
- **Adafruit BusIO**: MIT License
- **ESP-SR**: Apache 2.0 License
- **ESP-IDF**: Apache 2.0 License
- **Arduino Core ESP32**: LGPL 2.1

All libraries are free for commercial and non-commercial use.

## Updating Libraries

### Arduino IDE Method
1. **Sketch → Include Library → Manage Libraries**
2. Click "Update" next to library name
3. Restart IDE

### PlatformIO Method
```bash
pio lib update
```

### ESP-IDF Method
```bash
cd ~/esp/esp-idf
git pull
git submodule update --init --recursive
./install.sh esp32s3
```

## Troubleshooting Library Issues

### Compilation Errors

**Error:** `fatal error: Adafruit_INA219.h: No such file or directory`
**Fix:** Install Adafruit INA219 library via Library Manager

**Error:** `multiple definition of setup/loop`
**Fix:** Only one .ino file should be in the sketch folder

**Error:** `I2C/Wire.h not found`
**Fix:** Select ESP32S3 board in Tools → Board

### Runtime Issues

**Error:** INA219 `begin()` returns false
**Fix:**
1. Check I2C wiring (SDA=GPIO1, SCL=GPIO2)
2. Run I2C scanner to detect device
3. Check INA219 address (default 0x40)

**Error:** I2S `i2s_driver_install()` fails
**Fix:**
1. Check I2S pins correct (SCK=GPIO5, WS=GPIO4, SD=GPIO6)
2. Verify microphone power (3.3V)
3. Increase DMA buffer count

## Additional Resources

### Library Documentation
- Adafruit INA219: https://github.com/adafruit/Adafruit_INA219
- ESP-SR: https://github.com/espressif/esp-sr
- ESP32 Arduino Core: https://docs.espressif.com/projects/arduino-esp32/

### Example Code
- INA219 Examples: File → Examples → Adafruit INA219
- I2S Examples: File → Examples → ESP32 → I2S
- ESP-SR Examples: `~/esp/esp-sr/examples/`

### Community Support
- Arduino Forum: https://forum.arduino.cc/
- ESP32 Forum: https://www.esp32.com/
- Adafruit Forums: https://forums.adafruit.com/

---

**Last Updated:** 2026-02-18
**Tested With:** Arduino IDE 2.3.0, ESP32 Board Support 3.0.1
