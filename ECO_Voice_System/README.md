# ECO Voice - Offline Voice Recognition for Home Automation

## Project Overview
Edge-based, context-aware, offline voice recognition system for home automation using ESP32-S3.

## Hardware Components
1. **ESP32-S3** - Main controller with AI acceleration
2. **INMP441 I2S Microphone** - Voice input
3. **PIR Motion Sensor (HC-SR501)** - Motion detection
4. **LDR Module** - Ambient light sensing
5. **2-Channel Relay Module (5V, 10A)** - Light & Fan control
6. **LED (Red/Green)** - Status indication
7. **INA219 Current Sensor (±3.2A)** - Current monitoring
8. **12V DC Motor/Fan (0.3-0.6A)** - Fan load
9. **Breadboard & USB Cable**

## Pin Connections

### ESP32-S3 Pin Mapping
```
INMP441 Microphone:
- SCK  → GPIO 41
- WS   → GPIO 42
- SD   → GPIO 2
- VDD  → 3.3V
- GND  → GND

PIR Motion Sensor:
- OUT  → GPIO 4
- VCC  → 5V
- GND  → GND

LDR Module:
- AO   → GPIO 1 (ADC)
- VCC  → 3.3V
- GND  → GND

INA219 Current Sensor (I2C):
- SDA  → GPIO 8
- SCL  → GPIO 9
- VCC  → 3.3V
- GND  → GND

Relay Module (2-Channel):
- IN1  → GPIO 10 (Light)
- IN2  → GPIO 11 (Fan)
- VCC  → 5V
- GND  → GND

Status LEDs:
- Green LED → GPIO 12 (Anode) + 220Ω Resistor → GND
- Red LED   → GPIO 13 (Anode) + 220Ω Resistor → GND
```

## Software Setup

### Prerequisites
1. **ESP-IDF v5.1 or later** (Recommended) OR **Arduino IDE 2.x**
2. **ESP-SR Library** (Espressif Speech Recognition)
3. **Required Libraries:**
   - Adafruit_INA219
   - ESP-I2S for audio
   - ESP-SR for speech recognition

### Installation Steps

#### Option 1: ESP-IDF (Recommended for ESP-SR)
```bash
# Install ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh

# Clone ESP-SR
git clone --recursive https://github.com/espressif/esp-sr.git
```

#### Option 2: Arduino IDE
```
1. Install ESP32 board support:
   - Open Arduino IDE
   - File → Preferences → Additional Board Manager URLs
   - Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   - Tools → Board → Boards Manager → Search "ESP32" → Install

2. Install Libraries:
   - Sketch → Include Library → Manage Libraries
   - Search and install:
     * Adafruit INA219
     * ESP32-audioI2S
```

## System States
1. **IDLE** - Waiting for wake word "ECO"
2. **AUTHENTICATION** - Listening for secret code (30s timeout)
3. **UNLOCKED** - Ready for commands
4. **LOCKED** - Requires wake word to authenticate again

## Voice Commands
- **Wake Word:** "ECO"
- **Secret Code:** (User configurable)
- **Commands:**
  - "Light on" / "Light off"
  - "Fan on" / "Fan off"
  - "Status" - Reports sensor readings
  - "Lock" - Locks the system

## Sensor Logic
- **PIR Sensor:** No motion → Confirms action with user
- **LDR Sensor:** High brightness → Confirms light-on action
- **Current Sensor:** High load → Prevents fan-on, confirms with user

## Build & Flash
```bash
# ESP-IDF
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# Arduino IDE
Select: Board → ESP32S3 Dev Module
Upload
```

## Configuration
Edit `config.h` to set:
- Secret code phrase
- Sensor thresholds
- Timeout values
- Audio feedback preferences
