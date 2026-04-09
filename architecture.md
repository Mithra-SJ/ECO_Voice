# ECO Voice — Online Version Architecture

## Project Overview

ECO Voice (Online Version) is an internet-connected home automation system built on the ESP32-S3.
This version replaces the offline voice recognition pipeline with a web-based remote control dashboard
accessible from anywhere in the world. Any authorized user can monitor sensor readings and control
appliances in real time through a browser.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Firebase Cloud                          │
│                                                                 │
│   Firebase Auth          Firebase Realtime Database             │
│   (email/password)                                              │
│                          /eco_voice                             │
│                            /commands                            │
│                              light: false                       │
│                              fan:   false                       │
│                            /sensors                             │
│                              temperature:  26.0                 │
│                              humidity:     60.0                 │
│                              motion:       true                 │
│                              light_level:  400                  │
│                              current:      0.5                  │
│                              voltage:      5.0                  │
│                              power:        2.5                  │
│                            /status                              │
│                              light_on:   false                  │
│                              fan_on:     false                  │
│                              last_seen:  1712345678             │
└────────────────┬────────────────────────────┬───────────────────┘
                 │                            │
          reads/writes                  reads/writes
                 │                            │
    ┌────────────▼────────┐       ┌───────────▼──────────┐
    │     ESP32-S3        │       │      Web App          │
    │                     │       │   (React + Firebase)  │
    │  - DHT11 (temp/hum) │       │                       │
    │  - PIR  (motion)    │       │  - Login page         │
    │  - LDR  (light)     │       │  - Dashboard          │
    │  - INA219 (current) │       │    • Light toggle     │
    │  - Relay: Light     │       │    • Fan toggle       │
    │  - Relay: Fan       │       │    • Sensor cards     │
    │  - LED: Red/Green   │       │    • Device status    │
    │                     │       │    • Alerts           │
    └─────────────────────┘       └──────────────────────┘
```

---

## Tech Stack

| Layer | Technology | Purpose |
|-------|-----------|---------|
| Microcontroller | ESP32-S3 (C++ / ESP-IDF) | Reads sensors, controls relays, syncs with Firebase |
| IoT Library | Firebase-ESP-Client | WiFi + Firebase RTDB client for ESP32 |
| Database | Firebase Realtime Database | Real-time bidirectional sync between ESP32 and web app |
| Authentication | Firebase Authentication (email/password) | Secure login for all web app users |
| Web App | React + Firebase JS SDK | Dashboard UI served from the browser |
| Hosting | Firebase Hosting | Free static hosting with HTTPS |
| Notifications | In-app browser alerts | Alert when appliance has been on above threshold |

**Cost: Free** — Firebase Spark plan covers all of the above at this scale.

---

## ESP32 Firmware Behaviour

### Startup
1. Connect to home WiFi (credentials stored in `secrets.h`)
2. Authenticate with Firebase using a device service account token
3. Initialize all sensors (DHT11, PIR, LDR, INA219)
4. Initialize relay outputs (light, fan) — default OFF
5. Set up Firebase stream listener on `/eco_voice/commands`

### Main Loop (every 2 seconds)
1. Read all sensors
2. Push sensor data to `/eco_voice/sensors`
3. Update `/eco_voice/status/last_seen` with current epoch timestamp
4. Reflect current relay states to `/eco_voice/status`

### Command Listener (event-driven)
- Firebase stream fires when `/eco_voice/commands/light` or `/eco_voice/commands/fan` changes
- ESP32 reads the new value and toggles the corresponding relay immediately
- Updates `/eco_voice/status` to confirm the action

### Sensor-based Guards (retained from offline version)
| Sensor | Guard behaviour |
|--------|----------------|
| PIR | If no motion, web app shows a warning before confirming command |
| LDR | If brightness > threshold and light command = ON, web app warns user |
| DHT11 | If temp/humidity below threshold and fan = ON, web app warns user |
| INA219 | If voltage low or fluctuating, web app shows an alert on dashboard |

---

## Web App Pages

### Login Page
- Email + password form
- Firebase Authentication handles session
- Redirects to Dashboard on success

### Dashboard
```
┌──────────────────────────────────────────────────┐
│  ECO Voice Dashboard               [Logout]      │
├──────────────────────────────────────────────────┤
│  Device: Online  •  Last seen: 2 sec ago         │
├──────────────────────────────────────────────────┤
│  APPLIANCES                                      │
│  ┌────────────────┐   ┌────────────────┐         │
│  │  Light   [OFF] │   │  Fan     [OFF] │         │
│  │  [Turn ON]     │   │  [Turn ON]     │         │
│  └────────────────┘   └────────────────┘         │
├──────────────────────────────────────────────────┤
│  SENSORS                                         │
│  Temp: 26°C   Humidity: 60%   Motion: Detected   │
│  Light Level: 400   Current: 0.5A   Voltage: 5V  │
├──────────────────────────────────────────────────┤
│  ALERTS                                          │
│  ⚠ Light has been ON for 2 hours                 │
└──────────────────────────────────────────────────┘
```

---

## Firebase Realtime Database Schema

```json
{
  "eco_voice": {
    "commands": {
      "light": false,
      "fan": false
    },
    "sensors": {
      "temperature": 26.0,
      "humidity": 60.0,
      "motion": true,
      "light_level": 400,
      "current": 0.5,
      "voltage": 5.0,
      "power": 2.5
    },
    "status": {
      "light_on": false,
      "fan_on": false,
      "last_seen": 1712345678
    }
  }
}
```

---

## Security

- Firebase Authentication enforces login — unauthenticated requests are rejected
- Firebase Security Rules restrict database read/write to authenticated users only
- WiFi credentials and Firebase tokens stored in `secrets.h` (not committed to git)
- HTTPS enforced on all Firebase connections by default

### Firebase Security Rules
```json
{
  "rules": {
    "eco_voice": {
      ".read": "auth != null",
      ".write": "auth != null"
    }
  }
}
```

---

## Hardware Changes vs Offline Version

| Component | Offline Version | Online Version |
|-----------|----------------|---------------|
| INMP441 Microphone | Used for voice recognition | Not used |
| DFPlayer Mini | Used for audio replies | Not used |
| Speaker | Used for audio replies | Not used |
| ESP32-S3 WiFi | Not used | Used for Firebase sync |
| All sensors | Used | Used (same behaviour) |
| Relays | Controlled by voice | Controlled by web app |
| Red/Green LEDs | Lock/unlock status | Online/offline status |

---

## Build Phases

| Phase | What gets built |
|-------|----------------|
| 1 | Firebase project setup + security rules + database schema |
| 2 | ESP32 firmware — WiFi + Firebase connect + sensor push |
| 3 | ESP32 firmware — command listener + relay control |
| 4 | React web app — login page + Firebase auth |
| 5 | React web app — dashboard + real-time sensor display |
| 6 | React web app — appliance toggles + alerts |
| 7 | Firebase Hosting deploy + end-to-end test |
