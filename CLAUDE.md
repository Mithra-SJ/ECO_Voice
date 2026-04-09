# ECO Voice — Project Guide for Claude

---

## Project Overview

ECO Voice is an IoT-based home automation system built on the ESP32-S3.
The project has two versions maintained on separate git branches:

| Branch | Version | Description |
|--------|---------|-------------|
| `offline_version` | v1 — Offline Voice | Fully offline, wake word + MultiNet speech recognition controls appliances |
| `online_version` | v2 — Online Remote | Internet-connected, web dashboard controls appliances from anywhere |

---

## Hardware Components

| Component | Model | Purpose |
|-----------|-------|---------|
| Microcontroller | ESP32-S3 (N16R8 — 16MB Flash, 8MB PSRAM) | Main controller |
| Microphone | INMP441 (I2S) | Voice recognition (offline version only) |
| Motion Sensor | PIR HC-SR501 | Detects presence in room |
| Light Sensor | LDR module | Measures ambient brightness (ADC) |
| Temperature/Humidity | DHT11 | Reads temperature (°C) and humidity (%RH) |
| Current Sensor | INA219 (I2C, ±3.2A) | Monitors load current, voltage, power |
| Relay Module | 2-channel, 5V, 10A opto-isolated | Switches light and fan |
| Fan (demo) | 3V–6V DC motor (0.3–0.6A) | Demonstrates fan control |
| Light (demo) | White LED | Demonstrates light control |
| Status LEDs | Red LED + Green LED | System status indication |
| Audio (offline only) | DFPlayer Mini + SD card + Speaker | Plays pre-recorded audio responses |

### Pin Assignments
| Signal | GPIO |
|--------|------|
| I2S SCK | 5 |
| I2S WS | 4 |
| I2S SD | 6 |
| PIR | 7 |
| LDR (ADC) | 8 |
| DHT11 | 9 |
| INA219 SDA | 1 |
| INA219 SCL | 2 |
| Relay Light | 17 |
| Relay Fan | 18 |
| LED Green | 14 |
| LED Red | 13 |
| DFPlayer TX | 16 |
| DFPlayer RX | 15 |
| DFPlayer BUSY | 10 |

---

## Version 1 — Offline Voice (branch: `offline_version`)

### How It Works
- Fully offline, no internet required
- Wake word: **"Hi ESP"** (detected by ESP-SR MultiNet7)
- After wake: system says "listening for secret code" and listens for 30 seconds
- User speaks the secret code phrase (default: "one four five zero")
- If correct: system unlocks and listens for commands
- If wrong: system says "wrong code", user can retry within the same 30-second window
- After 30 seconds with no valid code: system locks again
- Commands: `turn light on`, `turn light off`, `turn fan on`, `turn fan off`, `show status`, `lock system`
- All actions have audio replies via DFPlayer Mini

### Speech Commands (MultiNet7 registered phrases)
| ID | Phrase to say | Action |
|----|--------------|--------|
| 1 | "hi esp" | Wake word |
| 2 | "hello there" | Alternative wake |
| 3 | "turn light on" | Turn light relay ON |
| 4 | "turn light off" | Turn light relay OFF |
| 5 | "turn fan on" | Turn fan relay ON |
| 6 | "turn fan off" | Turn fan relay OFF |
| 7 | "show status" | Read out sensor values |
| 8 | "lock system" | Lock the system |
| 9 | "yes please" | Confirm a prompted action |
| 10 | "no thanks" | Reject a prompted action |
| 11 | "one four five zero" | Secret code (default) |

### Sensor Guards (Offline)
| Sensor | Behaviour |
|--------|----------|
| PIR | If no motion detected and user gives a command → ask "no motion detected, do you still want to continue?" |
| LDR | If brightness > threshold and user says "turn light on" → ask "it's already bright, do you still want to switch on?" |
| DHT11 | If temp < 22°C or humidity < 40% and user says "turn fan on" → warn and ask for confirmation |
| INA219 | If voltage < 4.5V or fluctuation > 0.3V → inform user via audio |

### Known Issue
- MultiNet recognition rate is inconsistent in practice. Single-syllable words ("yes", "no", "lock") were replaced with longer phrases to improve accuracy. Still unreliable in noisy environments. This was the primary reason for switching to the online version.

---

## Version 2 — Online Remote Control (branch: `online_version`)

### Architecture Reference
See `architecture.md` in this branch for the full system design.

### How It Works
- ESP32-S3 connects to home WiFi on boot
- Syncs continuously with **Firebase Realtime Database**
- Any authorized user opens the **web app** (browser, any device, anywhere in the world)
- Logs in with email + password (Firebase Authentication)
- Dashboard shows live sensor readings and appliance states
- User clicks toggles → command written to Firebase → ESP32 receives it within ~1 second → relay toggles → status confirmed back to Firebase → dashboard updates

### Tech Stack
| Layer | Technology |
|-------|-----------|
| ESP32 firmware | C++ / ESP-IDF + Firebase-ESP-Client library |
| Cloud database | Firebase Realtime Database |
| Authentication | Firebase Authentication (email/password) |
| Web app | React + Firebase JS SDK |
| Hosting | Firebase Hosting (free, HTTPS) |

### Firebase Database Schema
```
/eco_voice
  /commands
    light: false        ← web app writes, ESP32 reads
    fan:   false
  /sensors
    temperature: 26.0   ← ESP32 writes, web app reads
    humidity:    60.0
    motion:      true
    light_level: 400
    current:     0.5
    voltage:     5.0
    power:       2.5
  /status
    light_on:  false    ← ESP32 writes (confirmed state)
    fan_on:    false
    last_seen: 1712345678
```

### Web App Features
- Login page with email/password
- Dashboard:
  - Device online/offline indicator (based on `last_seen`)
  - Light ON/OFF toggle with current state
  - Fan ON/OFF toggle with current state
  - Live sensor cards (temperature, humidity, motion, brightness, current, voltage, power)
  - Alert banner if appliance has been ON above a time threshold
  - Sensor-based warnings before executing commands (same logic as offline version)

### Sensor Guards (Online)
| Sensor | Behaviour |
|--------|----------|
| PIR | Web app shows warning if no motion detected before confirming command |
| LDR | Web app warns if brightness > threshold when turning light ON |
| DHT11 | Web app warns if temp/humidity below threshold when turning fan ON |
| INA219 | Dashboard shows alert if voltage is low or fluctuating |

### Security
- Firebase Auth rejects all unauthenticated requests
- Firebase Security Rules: only authenticated users can read/write `/eco_voice`
- WiFi SSID/password and Firebase credentials stored in `secrets.h` (not committed to git)

### Hardware Changes vs Offline Version
- INMP441 microphone: not used
- DFPlayer Mini + Speaker: not used
- ESP32 WiFi radio: active
- All sensors and relays: same as offline version

### Build Phases
1. Firebase project setup + security rules + database schema
2. ESP32 firmware: WiFi + Firebase connect + sensor push loop
3. ESP32 firmware: command stream listener + relay control
4. React web app: login page + Firebase auth
5. React web app: dashboard + real-time sensor display
6. React web app: appliance toggles + sensor-based warnings + alerts
7. Firebase Hosting deploy + end-to-end test

---

## Repository Structure

```
ECO_Voice/
├── main/
│   ├── main.cpp                 # App entry point and task setup
│   ├── voice_recognition.cpp/h  # ESP-SR MultiNet7 pipeline (offline only)
│   ├── audio_handler.cpp/h      # DFPlayer Mini driver (offline only)
│   ├── sensor_handler.cpp/h     # DHT11, PIR, LDR, INA219 reads
│   ├── appliance_control.cpp/h  # Relay and LED control
│   ├── config.h                 # Pin definitions and thresholds
│   ├── secrets.h                # WiFi credentials, Firebase token, secret code (not in git)
│   └── dht11.c/h                # DHT11 low-level driver
├── architecture.md              # Online version system design (this branch)
├── CLAUDE.md                    # This file
├── platformio.ini               # PlatformIO build config
└── partitions.csv               # Flash partition table
```

---

## Development Notes

- Platform: PlatformIO + ESP-IDF 5.5.0
- Board: `esp32-s3-devkitc-1-n16r8` (16MB QIO flash, 8MB PSRAM)
- Flash command: `pio run --target upload`
- Monitor command: `pio device monitor --port COMX --baud 115200`
- The `dependencies.lock` file tracks ESP-SR and other component versions — do not modify manually
- Always pull before pushing; the team pushes from multiple machines
