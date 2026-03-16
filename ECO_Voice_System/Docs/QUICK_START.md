# ECO Voice System - Quick Start Guide

## Getting Started in 30 Minutes

This guide gets you up and running quickly with a demo version using Serial input instead of voice.
For full voice recognition, see `ESP_SR_INTEGRATION.md`.

## What You'll Build

An offline home automation system with:
- ✅ Wake word authentication ("ECO")
- ✅ Secret code protection
- ✅ Voice commands (Light/Fan control)
- ✅ Smart sensor integration (Motion, Light, Current)
- ✅ Visual status indicators

## Prerequisites

### Hardware (Minimum for Testing)
- ESP32-S3 Development Board
- USB Cable (USB-C or Micro-USB depending on board)
- Computer with Arduino IDE

### Optional Components (for full functionality)
- INMP441 I2S Microphone
- PIR Motion Sensor (HC-SR501)
- LDR Module
- INA219 Current Sensor
- 2-Channel Relay Module
- 2x LEDs (Red & Green) with 220Ω resistors
- Breadboard and jumper wires

## Step 1: Software Setup (10 minutes)

### Install Arduino IDE

1. Download Arduino IDE 2.x from https://www.arduino.cc/en/software
2. Install and open Arduino IDE

### Add ESP32 Support

1. Open **File → Preferences**
2. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Click **OK**
4. Open **Tools → Board → Boards Manager**
5. Search for "ESP32"
6. Install "**esp32 by Espressif Systems**" (version 3.0.0+)

### Install Libraries

1. Open **Sketch → Include Library → Manage Libraries**
2. Search and install:
   - **Adafruit INA219**
   - **Adafruit BusIO** (dependency)

## Step 2: Hardware Setup (10 minutes)

### Minimal Setup (LEDs Only - Good for Testing)

```
ESP32-S3          Component
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
GPIO 12    ──>    Green LED (+) ──> 220Ω ──> GND
GPIO 13    ──>    Red LED (+) ──> 220Ω ──> GND
```

**LED Polarity:**
- Long leg (anode) → GPIO pin
- Short leg (cathode) → Resistor → GND

### With Sensors (Optional)

If you have additional components, connect them as shown in `HARDWARE_DIAGRAM.txt`.
The code will work even if sensors are not connected (with reduced functionality).

## Step 3: Load the Code (5 minutes)

### Download/Copy Files

1. Navigate to the `ECO_Voice_System` folder
2. Ensure all these files are in the same folder:
   ```
   ECO_Voice_System/
   ├── eco_voice_main.ino
   ├── config.h
   ├── audio_handler.h
   ├── audio_handler.cpp
   ├── voice_recognition.h
   ├── voice_recognition.cpp
   ├── sensor_handler.h
   ├── sensor_handler.cpp
   ├── appliance_control.h
   └── appliance_control.cpp
   ```

### Open Project

1. Double-click `eco_voice_main.ino` to open in Arduino IDE
2. All other files should appear as tabs

### Configure Board

1. **Tools → Board → esp32 → ESP32S3 Dev Module**
2. **Tools → Port → [Select your ESP32 port]**
   - Windows: COM3, COM4, etc.
   - Mac: /dev/cu.usbserial-*
   - Linux: /dev/ttyUSB0, /dev/ttyACM0
3. Other settings (defaults are fine):
   - Upload Speed: 921600
   - USB CDC On Boot: Enabled

## Step 4: Customize Settings (2 minutes)

Open `config.h` and modify if needed:

```cpp
// Change your secret code here
#define SECRET_CODE "open sesame"

// Adjust sensor thresholds
#define BRIGHTNESS_THRESHOLD 600
#define CURRENT_THRESHOLD 2.5
```

## Step 5: Upload & Test (5 minutes)

### Compile and Upload

1. Click **Verify** (✓ checkmark) to compile
2. Fix any errors if they appear
3. Click **Upload** (→ arrow) to flash to ESP32-S3
4. Wait for "Leaving... Hard resetting via RTS pin..."

### Open Serial Monitor

1. **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Set line ending to **Newline**

### Expected Output

```
ECO Voice System Starting...
Initializing Sensors...
All sensors initialized successfully
Initializing Appliance Control...
Appliance Control Ready
Audio Handler Initialized
Voice Recognition Ready
DEMO MODE: Using Serial input for commands
System Ready. Say 'ECO' to wake up!
Status: LOCKED (Red LED)
```

## Step 6: Test Commands

### Test Sequence

1. **Wake the System**
   ```
   Type: eco
   Response: "Listening for secret code"
   LED: Green blinks 3 times
   ```

2. **Enter Secret Code**
   ```
   Type: open sesame
   Response: "Unlocked. Ready for commands"
   LED: Green stays ON
   ```

3. **Control Light**
   ```
   Type: light on
   Response: "Light turned on"

   Type: light off
   Response: "Light turned off"
   ```

4. **Control Fan**
   ```
   Type: fan on
   Response: "Fan turned on"

   Type: fan off
   Response: "Fan turned off"
   ```

5. **Get Status**
   ```
   Type: status
   Response: Sensor readings and appliance states
   ```

6. **Lock System**
   ```
   Type: lock
   Response: "System locked"
   LED: Red turns ON
   ```

### Quick Command Reference

```
Commands available in Serial Monitor:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
eco          → Wake system
[your code]  → Unlock (after wake)
light on     → Turn on light (or: 1)
light off    → Turn off light (or: 2)
fan on       → Turn on fan (or: 3)
fan off      → Turn off fan (or: 4)
status       → Get sensor status (or: 5)
lock         → Lock system (or: 6)
yes / y      → Confirm action
no / n       → Cancel action
```

## Troubleshooting

### ESP32 Not Detected

**Problem:** Port not showing in Arduino IDE

**Solutions:**
- Install CH340 or CP2102 USB drivers
- Try a different USB cable (must be data cable, not charge-only)
- Try a different USB port
- Press and hold BOOT button while uploading

### Compilation Errors

**Problem:** "library not found" errors

**Solutions:**
- Re-install Adafruit INA219 library
- Ensure all .h and .cpp files are in the same folder
- Close and reopen Arduino IDE

### Upload Fails

**Problem:** "Failed to connect to ESP32"

**Solutions:**
- Press and hold BOOT button, click Upload, release after "Connecting..."
- Reduce upload speed: **Tools → Upload Speed → 115200**
- Close Serial Monitor before uploading

### No Serial Output

**Problem:** Serial Monitor is blank

**Solutions:**
- Check baud rate is set to 115200
- Press RESET button on ESP32-S3
- Select correct port
- Enable "USB CDC On Boot" in Tools menu

### LEDs Not Working

**Problem:** LEDs don't light up

**Solutions:**
- Check LED polarity (long leg = anode = positive)
- Verify resistor is 220Ω (not too high)
- Test LED with 3.3V directly
- Check GPIO pin connections (12 = Green, 13 = Red)

## Next Steps

### Enhance Your System

1. **Add Real Sensors**
   - Connect PIR, LDR, INA219 as per `HARDWARE_DIAGRAM.txt`
   - Sensors provide context-aware automation

2. **Add Relay Control**
   - Connect relays to GPIO 10 (Light) and GPIO 11 (Fan)
   - Control real appliances (start with 12V DC loads)

3. **Add Voice Recognition**
   - Follow `ESP_SR_INTEGRATION.md` for offline voice
   - Train custom "ECO" wake word
   - Add command recognition

4. **Add Audio Feedback**
   - Use DFPlayer Mini for pre-recorded messages
   - Or MAX98357A I2S amplifier with speaker
   - See `audio_handler.cpp` implementation notes

5. **Optimize Performance**
   - Calibrate sensor thresholds in `config.h`
   - Adjust authentication timeout
   - Fine-tune voice detection parameters

## File Structure

```
ECO_Voice_System/
│
├── eco_voice_main.ino          ← Main program (state machine)
├── config.h                    ← Pin definitions & settings
│
├── audio_handler.h/cpp         ← Voice feedback (TTS/audio)
├── voice_recognition.h/cpp     ← Wake word & command detection
├── sensor_handler.h/cpp        ← PIR, LDR, INA219 management
├── appliance_control.h/cpp     ← Relay & LED control
│
├── README.md                   ← Project overview
├── SETUP_GUIDE.md              ← Detailed setup instructions
├── ESP_SR_INTEGRATION.md       ← Voice recognition guide
├── HARDWARE_DIAGRAM.txt        ← Circuit diagrams
└── QUICK_START.md              ← This file
```

## System Architecture

```
┌─────────────────────────────────────────────┐
│           Main State Machine                │
│         (eco_voice_main.ino)                │
│                                             │
│  States:                                    │
│  1. IDLE → Listen for wake word            │
│  2. WAITING_CODE → Authenticate             │
│  3. UNLOCKED → Ready for commands           │
│  4. PROCESSING_COMMAND → Execute action     │
└─────────────────────────────────────────────┘
              ↓         ↓         ↓
    ┌─────────┴───┐  ┌─┴──────┐  └───────┐
    ▼             ▼  ▼        ▼          ▼
┌────────┐  ┌─────────┐  ┌────────┐  ┌──────┐
│ Voice  │  │ Sensors │  │ Audio  │  │ App. │
│ Recog  │  │ Handler │  │Handler │  │ Ctrl │
└────────┘  └─────────┘  └────────┘  └──────┘
```

## Demo Mode vs. Production Mode

| Feature           | Demo Mode (Serial) | Production (Voice) |
|-------------------|--------------------|--------------------|
| Wake Word         | Type "eco"         | Say "ECO"          |
| Commands          | Type text          | Speak command      |
| Audio Feedback    | Serial print       | Speaker output     |
| Setup Complexity  | Low (no hardware)  | High (mic, SR)     |
| User Experience   | Testing only       | Full automation    |

**Current implementation runs in DEMO MODE for easy testing.**

## Support & Resources

- **Setup Issues**: See `SETUP_GUIDE.md`
- **Hardware**: See `HARDWARE_DIAGRAM.txt`
- **Voice Recognition**: See `ESP_SR_INTEGRATION.md`
- **ESP32-S3 Docs**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/
- **Arduino ESP32**: https://docs.espressif.com/projects/arduino-esp32/

## Safety Reminders

⚠️ **IMPORTANT:**
- Start with 12V DC loads, not AC mains
- Never work on live AC circuits without proper training
- Use fuses and circuit breakers
- Ensure proper ventilation for ESP32-S3
- Monitor current consumption with INA219

---

**Congratulations!** You now have a working voice-controlled home automation demo.
Explore the other documentation files to add more features and sensors.

**Built with ESP32-S3 • Powered by ESP-SR • Designed for Edge AI**
