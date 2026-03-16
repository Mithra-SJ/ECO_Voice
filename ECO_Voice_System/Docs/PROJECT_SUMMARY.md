# ECO Voice System - Complete Project Summary

## Project Title
**ECO Voice - Edge-based, Context-aware, Offline Voice Recognition for Home Automation**

## Overview
A fully offline, edge-based home automation system using ESP32-S3's AI acceleration capabilities for voice recognition and control. The system features wake word detection, secret code authentication, and intelligent sensor-based automation with voice feedback.

## Project Objectives

### Primary Goals
1. ✅ **Offline Operation**: Complete functionality without internet connectivity
2. ✅ **Voice Control**: Wake word and natural language command recognition
3. ✅ **Security**: Secret code authentication before granting access
4. ✅ **Context Awareness**: Sensor-based intelligent decision making
5. ✅ **User Feedback**: Audio responses for all interactions
6. ✅ **Safety**: Load monitoring and user confirmation for edge cases

### Key Features
- Wake word detection: "ECO"
- Secret code protection with timeout
- Voice commands: Light on/off, Fan on/off, Status, Lock
- PIR motion sensing with user confirmation
- Light level detection for smart lighting
- Current monitoring for load protection
- Visual status indicators (LED)
- Audio feedback for all actions

## Hardware Architecture

### Core Components
| Component | Purpose | Interface | Power |
|-----------|---------|-----------|-------|
| ESP32-S3 | Main controller & AI processing | - | 5V USB |
| INMP441 | Voice input via I2S microphone | I2S | 3.3V |
| HC-SR501 | Motion detection | Digital GPIO | 5V |
| LDR Module | Ambient light sensing | ADC | 3.3V |
| INA219 | Current/voltage monitoring | I2C | 3.3V |
| 2-Ch Relay | Appliance switching (10A) | Digital GPIO | 5V |
| LEDs (R/G) | System status indication | Digital GPIO | 3.3V |

### Pin Assignment Summary
```
Audio Input:  GPIO 41 (I2S_SCK), GPIO 42 (I2S_WS), GPIO 2 (I2S_SD)
Sensors:      GPIO 4 (PIR), GPIO 1 (LDR/ADC)
I2C Bus:      GPIO 8 (SDA), GPIO 9 (SCL)
Relays:       GPIO 10 (Light), GPIO 11 (Fan)
Status LEDs:  GPIO 12 (Green), GPIO 13 (Red)
```

### Power Requirements
- ESP32-S3: ~500mA @ 5V
- Sensors: ~100mA @ 3.3V/5V
- Relays: ~40mA @ 5V (coil)
- Total: 2A @ 5V recommended minimum

## Software Architecture

### System States
```
1. IDLE              → Listening for wake word "ECO"
2. WAITING_CODE      → 30-second authentication window
3. UNLOCKED          → Ready for commands
4. PROCESSING_CMD    → Executing user command
```

### State Transitions
```
IDLE ──[wake word]──> WAITING_CODE ──[correct code]──> UNLOCKED
  ▲                         │                             │
  │                         │                             │
  └────[timeout]────────────┘                             │
  │                                                        │
  └──────────────────────[lock command]────────────────────┘
```

### Software Modules

#### 1. Main State Machine (`eco_voice_main.ino`)
- Manages system states and transitions
- Coordinates all subsystems
- Implements timeout logic
- Handles user authentication

#### 2. Voice Recognition (`voice_recognition.h/cpp`)
- Wake word detection ("ECO")
- Command recognition
- Secret code verification
- Yes/No confirmation
- **Demo Mode**: Serial input for testing
- **Production**: ESP-SR integration

#### 3. Audio Handler (`audio_handler.h/cpp`)
- Text-to-speech output
- Pre-recorded message playback
- Tone generation for feedback
- **Current**: Serial output + beeps
- **Future**: DFPlayer Mini or I2S speaker

#### 4. Sensor Handler (`sensor_handler.h/cpp`)
- PIR motion detection with persistence
- LDR analog reading with smoothing
- INA219 current/voltage monitoring
- Automatic sensor updates

#### 5. Appliance Control (`appliance_control.h/cpp`)
- Relay switching logic
- Status LED management
- State tracking
- Safety interlocks

#### 6. Configuration (`config.h`)
- Pin definitions
- Sensor thresholds
- Timeout values
- Secret code storage

## Operational Flow

### Typical Usage Scenario

```
1. User says "ECO"
   ↓
2. System: "Listening for secret code"
   Green LED blinks
   ↓
3. User says "open sesame"
   ↓
4. System: "Unlocked. Ready for commands"
   Green LED solid ON
   ↓
5. User says "light on"
   ↓
6. System checks motion sensor
   ├─ No motion → "No motion detected. Continue?"
   │              User: "yes" → Proceed
   └─ Motion OK → Check brightness
                  ├─ Bright → "It's bright. Continue?"
                  │           User: "yes" → Turn on
                  └─ Dark → Turn on light immediately
   ↓
7. System: "Light turned on"
   Relay activates
```

### Sensor-Based Intelligence

#### Motion Detection Logic
```
IF command is "light on" OR "fan on":
    IF no motion detected:
        ASK user "No motion detected. Continue?"
        IF user says "yes":
            PROCEED
        ELSE:
            CANCEL
    ELSE:
        PROCEED
```

#### Brightness Check Logic
```
IF command is "light on":
    IF brightness > threshold:
        ASK user "It's bright. Continue?"
        IF user says "yes":
            PROCEED
        ELSE:
            CANCEL
    ELSE:
        PROCEED
```

#### Current Monitoring Logic
```
IF command is "fan on":
    IF current > threshold:
        WARN user "High current. Continue?"
        IF user says "yes":
            PROCEED (with caution)
        ELSE:
            CANCEL for safety
    ELSE:
        PROCEED
```

## Development Phases

### Phase 1: Demo Mode ✅ (Current Implementation)
- [x] Hardware assembly with basic sensors
- [x] Arduino IDE setup
- [x] Core state machine
- [x] Serial input for commands (testing)
- [x] Relay control
- [x] LED status indicators
- [x] Sensor reading and logic

### Phase 2: Voice Recognition 🔄 (Next Step)
- [ ] ESP-IDF migration
- [ ] ESP-SR library integration
- [ ] Wake word training for "ECO"
- [ ] Command phrase training
- [ ] MultiNet model deployment
- [ ] Accuracy testing and optimization

### Phase 3: Audio Feedback 📋 (Planned)
- [ ] DFPlayer Mini integration OR
- [ ] I2S speaker + amplifier
- [ ] Pre-record all response messages
- [ ] Audio file organization
- [ ] Playback timing optimization

### Phase 4: Optimization 📋 (Future)
- [ ] Power consumption optimization
- [ ] Deep sleep modes
- [ ] Over-the-air (OTA) updates
- [ ] Cloud logging (optional)
- [ ] Mobile app integration (optional)
- [ ] Custom enclosure design

## File Structure

```
ECO_Voice_System/
│
├── Documentation
│   ├── README.md                    # Project overview
│   ├── QUICK_START.md              # 30-min getting started
│   ├── SETUP_GUIDE.md              # Detailed setup steps
│   ├── ESP_SR_INTEGRATION.md       # Voice recognition guide
│   ├── HARDWARE_DIAGRAM.txt        # Circuit diagrams
│   └── PROJECT_SUMMARY.md          # This file
│
├── Source Code
│   ├── eco_voice_main.ino          # Main program (1,000 lines)
│   ├── config.h                    # Configuration & pins
│   ├── audio_handler.h/cpp         # Audio output module
│   ├── voice_recognition.h/cpp     # Voice input module
│   ├── sensor_handler.h/cpp        # Sensor management
│   └── appliance_control.h/cpp     # Relay & LED control
│
└── Future Additions
    ├── esp-sr/                     # ESP-SR library
    ├── audio_files/                # Pre-recorded messages
    ├── models/                     # Trained ML models
    └── enclosure/                  # 3D print STL files
```

## Technical Specifications

### Voice Recognition
- **Engine**: ESP-SR (Espressif Speech Recognition)
- **Wake Word**: Custom trained "ECO" model
- **Command Set**: 8 phrases (Light on/off, Fan on/off, Status, Lock, Yes, No)
- **Sample Rate**: 16kHz
- **Bit Depth**: 16-bit
- **Channels**: Mono
- **Detection Mode**: 90% confidence threshold
- **Language**: English (customizable)

### Sensor Specifications
- **PIR Range**: 3-7 meters (adjustable)
- **PIR Angle**: 110° cone
- **LDR Range**: 0-4095 (12-bit ADC)
- **Current Range**: ±3.2A (INA219)
- **Voltage Range**: 0-26V (INA219)
- **Sampling Rate**: 10Hz (100ms update)

### Performance Metrics
- **Wake Word Latency**: <500ms
- **Command Recognition**: <1s
- **Response Time**: <200ms
- **False Wake Rate**: <1/hour (target)
- **Command Accuracy**: >90% (target)
- **Power Consumption**: ~2.5W active, ~0.5W idle

## Implementation Details

### Memory Usage
```
ESP32-S3 Resources:
- Flash: ~1.5MB (program + models)
- SRAM: ~200KB (runtime)
- PSRAM: ~4MB (audio buffers + ML models)
- Stack: 8KB per task (FreeRTOS)
```

### Task Allocation (FreeRTOS)
```
Task Name           Core    Priority    Stack
─────────────────────────────────────────────
wake_detect_task    0       5           8KB
sensor_update_task  1       3           4KB
led_blink_task      1       2           2KB
audio_output_task   0       4           4KB
```

### I2S Audio Configuration
```c
Sample Rate:      16000 Hz
Bits Per Sample:  32-bit (I2S) → 16-bit (processing)
Channel:          Mono (LEFT)
DMA Buffers:      4 x 1024 bytes
Mode:             Master RX
Clock:            BCLK = 1.024 MHz, LRCK = 16 kHz
```

### I2C Configuration
```c
Clock Speed:      100 kHz (standard mode)
Address:          0x40 (INA219)
SDA Pull-up:      4.7kΩ
SCL Pull-up:      4.7kΩ
```

## Testing Checklist

### Hardware Tests
- [ ] Power supply voltages (5V, 3.3V)
- [ ] I2S microphone audio capture
- [ ] PIR motion detection range
- [ ] LDR brightness readings (dark/bright)
- [ ] INA219 current accuracy (0-3A)
- [ ] Relay switching (audible click)
- [ ] LED status indicators
- [ ] No short circuits or overheating

### Software Tests
- [ ] Wake word detection
- [ ] Secret code authentication
- [ ] Command recognition
- [ ] Sensor reading accuracy
- [ ] Relay control
- [ ] State machine transitions
- [ ] Timeout handling
- [ ] Yes/No confirmations

### Integration Tests
- [ ] Motion-based confirmation flow
- [ ] Brightness-based confirmation
- [ ] Current-based safety checks
- [ ] Multi-command sequences
- [ ] Lock/unlock cycles
- [ ] Error recovery
- [ ] Continuous operation (24h test)

## Safety Considerations

### Electrical Safety
- ⚠️ **AC Voltage**: Only for qualified personnel
- Use GFCI/RCD protection for AC loads
- Add fuses: 5A for 12V DC, appropriately rated for AC
- Ensure relay isolation
- Proper wire gauge for current (18 AWG for 10A)

### Software Safety
- Current monitoring prevents overload
- User confirmation for edge cases
- Timeout prevents stuck states
- Watchdog timer (future addition)
- Fail-safe: power loss = all OFF

### Thermal Management
- ESP32-S3 max temp: 85°C
- Add heatsink if continuous operation
- Ventilation holes in enclosure
- Monitor via internal temp sensor

## Customization Options

### User Configurable (`config.h`)
```cpp
#define SECRET_CODE "your phrase here"
#define BRIGHTNESS_THRESHOLD 600
#define CURRENT_THRESHOLD 2.5
#define AUTH_TIMEOUT_MS 30000
#define WAKE_WORD "eco"  // Requires model retraining
```

### Hardware Variants
- **Minimal**: ESP32-S3 + LEDs only
- **Standard**: + PIR + LDR + Relay
- **Full**: + INA219 + Microphone + Speaker
- **Advanced**: + Multiple microphones for beamforming

### Software Modes
- **Demo Mode**: Serial input (current)
- **Voice Mode**: ESP-SR recognition (production)
- **Hybrid Mode**: Both Serial and Voice
- **Debug Mode**: Verbose logging enabled

## Cost Estimate

| Component | Quantity | Unit Price | Total |
|-----------|----------|------------|-------|
| ESP32-S3 Dev Board | 1 | $10 | $10 |
| INMP441 Microphone | 1 | $3 | $3 |
| PIR Sensor (HC-SR501) | 1 | $2 | $2 |
| LDR Module | 1 | $1 | $1 |
| INA219 Current Sensor | 1 | $4 | $4 |
| 2-Channel Relay | 1 | $3 | $3 |
| LEDs + Resistors | - | $1 | $1 |
| Breadboard & Wires | 1 | $5 | $5 |
| USB Cable | 1 | $3 | $3 |
| Misc (screws, enclosure) | - | $5 | $5 |
| **TOTAL** | | | **$37** |

*Prices are approximate and may vary by region/supplier*

## Learning Outcomes

By completing this project, you will learn:
1. ✅ ESP32-S3 programming (Arduino & ESP-IDF)
2. ✅ I2S audio interfacing
3. ✅ I2C sensor communication
4. ✅ Relay control and electrical switching
5. ✅ State machine design
6. ✅ Real-time audio processing
7. ✅ Machine learning model deployment (ESP-SR)
8. ✅ Sensor data fusion
9. ✅ Power management
10. ✅ IoT system design patterns

## Future Enhancements

### Short Term
- [ ] Add buzzer for audio tones
- [ ] Implement watchdog timer
- [ ] Add SD card logging
- [ ] Create mobile app for configuration

### Medium Term
- [ ] Multi-room support (multiple ESP32s)
- [ ] MQTT for inter-device communication
- [ ] Energy monitoring dashboard
- [ ] Scheduling and automation rules

### Long Term
- [ ] AI-based usage prediction
- [ ] Integration with smart home platforms
- [ ] Voice recognition for multiple users
- [ ] Gesture control addition

## Troubleshooting Matrix

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| No serial output | Wrong baud rate | Set to 115200 |
| Compilation error | Missing libraries | Install Adafruit INA219 |
| Upload fails | Wrong board selected | ESP32S3 Dev Module |
| LEDs don't work | Wrong polarity | Long leg to GPIO |
| PIR always triggered | Sensitivity too high | Adjust potentiometer |
| INA219 not found | I2C wiring | Check SDA/SCL pins |
| Relay not clicking | Insufficient voltage | Use 5V for VCC |
| High current draw | Short circuit | Check all connections |

## References & Resources

### Documentation
- ESP32-S3 Technical Reference: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
- ESP-SR Guide: https://github.com/espressif/esp-sr
- Arduino ESP32: https://docs.espressif.com/projects/arduino-esp32/

### Datasheets
- INMP441: https://invensense.tdk.com/products/digital/inmp441/
- INA219: https://www.ti.com/lit/ds/symlink/ina219.pdf
- HC-SR501: https://www.epitran.it/ebayDrive/datasheet/44.pdf

### Tools
- Arduino IDE: https://www.arduino.cc/en/software
- ESP-IDF: https://docs.espressif.com/projects/esp-idf/
- Audacity: https://www.audacityteam.org/ (audio editing)

## Credits & License

**Project Author**: Kabilan
**Platform**: ESP32-S3
**Framework**: Arduino / ESP-IDF
**Speech Recognition**: ESP-SR (Espressif)

**License**: Open source (MIT License)
Feel free to modify, distribute, and use for educational purposes.

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────┐
│              ECO VOICE QUICK REFERENCE                  │
├─────────────────────────────────────────────────────────┤
│ WAKE WORD:      "ECO"                                   │
│ SECRET CODE:    "open sesame" (configurable)            │
│                                                         │
│ COMMANDS:                                               │
│  • "light on" / "light off"                            │
│  • "fan on" / "fan off"                                │
│  • "status"                                            │
│  • "lock"                                              │
│                                                         │
│ LED STATUS:                                            │
│  • Red ON      → System locked                         │
│  • Green ON    → System unlocked                       │
│  • Green BLINK → Listening                             │
│                                                         │
│ SERIAL COMMANDS (Demo Mode):                           │
│  eco, [code], light on, fan on, status, lock           │
│  yes/no for confirmations                              │
│                                                         │
│ PIN REFERENCE:                                         │
│  GPIO 1,2,41,42: Audio (I2S)                          │
│  GPIO 4: PIR                                           │
│  GPIO 8,9: I2C (INA219)                               │
│  GPIO 10,11: Relays                                    │
│  GPIO 12,13: LEDs                                      │
└─────────────────────────────────────────────────────────┘
```

**Built with ❤️ for offline home automation**
**Powered by ESP32-S3 AI Acceleration**
