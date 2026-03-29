# ECO Voice — Developer Workflow Guide
> How to go from code change → build → test → flash → GitHub push

---

## 1. Understanding the Two Types of "Push"

Before anything, know that **"push" means two completely different things** in this project:

| Term | What it does | Where it goes |
|------|-------------|----------------|
| `pio run -t upload` | Flashes compiled firmware to the physical ESP32-S3 chip | Your board via USB |
| `git push` | Uploads your code to GitHub for version control | GitHub (Mithra-SJ/ECO_Voice) |

These are **independent operations**. One does not trigger the other. You control when each happens.

---

## 2. Project Structure — What Each File Does

```
ECO_Voice/
├── src/
│   ├── main.cpp                  ← Main app logic, state machine
│   ├── voice_recognition.cpp     ← Wake word + command recognition (ESP-SR)
│   ├── voice_recognition.h
│   ├── sensor_handler.cpp        ← PIR, LDR, INA219 readings
│   ├── sensor_handler.h
│   ├── appliance_control.cpp     ← Relay, LED control
│   ├── appliance_control.h
│   ├── audio_handler.cpp         ← DFPlayer audio responses
│   ├── audio_handler.h
│   ├── config.h                  ← All pin numbers, thresholds, timeouts
│   ├── secrets.h                 ← Secret code phrase (DO NOT commit)
│   └── secrets.h.example         ← Template for secrets.h
├── managed_components/
│   └── espressif__esp-sr/
│       └── model/target/         ← Wake word + command model files
│           ├── wn9_hiesp/        ← "Hi ESP" wake word model
│           ├── mn6_en_ctc/       ← English command model
│           └── srmodels.bin      ← Packed binary (generated once)
├── platformio.ini                ← Build config, port, upload settings
├── partitions.csv                ← Flash memory layout for ESP32-S3
├── flash_model.py                ← Auto-flashes model partition after upload
└── CMakeLists.txt                ← IDF build system config
```

---

## 3. Prerequisites (One-Time Setup)

### Install PlatformIO
```powershell
pip install platformio
```

### Verify USB Driver (CH343)
Plug in your ESP32-S3 via USB. Open Device Manager → Ports (COM & LPT).
You should see **USB-Enhanced-SERIAL CH343 (COM8)** or similar.
If not, download and install the CH343 driver from the WCH website.

### Verify PlatformIO sees the board
```powershell
pio device list
```
Look for COM8 (or your port) in the output.

---

## 4. Generate the Voice Model Binary (One-Time Only)

The ESP-SR voice models (wake word "Hi ESP" + commands) must be packed into a single binary and flashed to a separate partition on the chip. **This only needs to be done once**, unless you delete `srmodels.bin` or wipe the chip completely.

```powershell
python managed_components/espressif__esp-sr/model/pack_model.py -m managed_components/espressif__esp-sr/model/target -o srmodels.bin
```

Verify it was created:
```powershell
dir managed_components/espressif__esp-sr/model/target/srmodels.bin
```
Expected size: 3–5 MB.

> **Why this is needed:** PlatformIO's normal upload only flashes the app binary. The voice model lives in a separate `model` partition at flash offset `0x310000`. Without it, the ESP boots but fails with: `No wake-net model found`.

---

## 5. The Full Development Cycle

### Step 1 — Edit your code
Make changes in the `src/` folder using VS Code or any editor.
- For pin/threshold changes → edit `src/config.h`
- For voice logic → edit `src/voice_recognition.cpp`
- For appliance logic → edit `src/appliance_control.cpp`
- For sensor logic → edit `src/sensor_handler.cpp`
- For main flow → edit `src/main.cpp`

### Step 2 — Build (compile only, no flash)
```powershell
cd E:\MITHRASJ\LICET\FinalYearProject\ECO_Voice
pio run
```
This compiles your code and reports any errors **without touching the ESP32**.
Fix all errors before proceeding. Look for:
```
[SUCCESS] Took X.XX seconds    ← Good, proceed
[FAILED]                        ← Fix the errors shown above it
```

### Step 3 — Flash to ESP32-S3
Close the serial monitor if it is open, then:
```powershell
pio run -t upload
```
This does three things automatically:
1. Recompiles if needed
2. Flashes the app binary to flash offset `0x10000`
3. Runs `flash_model.py` → flashes `srmodels.bin` to flash offset `0x310000`

### Step 4 — Open serial monitor to verify
```powershell
pio device monitor --port COM8 --baud 115200
```
Expected output on successful boot:
```
I (XXX) MAIN: === ECO Voice System Starting ===
I (XXX) SENSOR: All sensors initialized successfully.
I (XXX) VOICE: first_model='wakeNet9_v1h24_hiesp_...'
I (XXX) VOICE: ESP-SR initialized successfully.
```
Then say **"Hi ESP"** to test wake word detection.

Press `Ctrl+C` to exit the monitor.

### Step 5 — Push to GitHub (only after verified working)
```powershell
git add src/yourchangedfile.cpp
git commit -m "Brief description of what you changed and why"
git push origin Thirdbranch
```

> **Rule:** Never push broken code to GitHub. Always test on the board first.

---

## 6. Scenario-Based Command Reference

### Scenario A — Made code changes, want to test
```powershell
pio run                          # Check for compile errors
pio run -t upload                # Flash to board
pio device monitor --port COM8 --baud 115200   # Watch output
```

### Scenario B — Changed `platformio.ini` or `partitions.csv`
These are config-level changes. A clean build is needed.
```powershell
pio run -t clean                 # Wipe previous build
pio run -t upload                # Rebuild from scratch + flash
pio device monitor --port COM8 --baud 115200
```

### Scenario C — Board was wiped or replaced, no code changes
```powershell
pio run -t upload                # Reflash existing compiled firmware
```

### Scenario D — Voice stopped working after a board wipe (no app change)
The model partition was erased. Reflash it manually:
```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash 0x310000 managed_components/espressif__esp-sr/model/target/srmodels.bin
```

### Scenario E — Flash + monitor in a single command
```powershell
pio run -t upload && pio device monitor --port COM8 --baud 115200
```

### Scenario F — Want to save work to GitHub without flashing
```powershell
git add src/changedfile.cpp
git commit -m "WIP: describe what you changed"
git push origin Thirdbranch
```

### Scenario G — Pull latest code from GitHub (team sync)
```powershell
git pull origin Thirdbranch
```
Then rebuild and reflash:
```powershell
pio run -t upload
```

---

## 7. Flash Memory Layout (For Reference)

| Partition | Offset | Size | Contents |
|-----------|--------|------|----------|
| nvs | 0x9000 | 20 KB | WiFi/NVS data |
| otadata | 0xE000 | 8 KB | OTA metadata |
| app0 | 0x10000 | 3 MB | Your compiled firmware |
| model | 0x310000 | 4.75 MB | ESP-SR voice models (srmodels.bin) |

`pio run -t upload` flashes `app0`.
`flash_model.py` flashes `model`.

---

## 8. Troubleshooting

| Problem | What it means | Fix |
|---------|--------------|-----|
| `COM8 not found` | Driver missing or board not plugged in | Check Device Manager, reinstall CH343 driver, replug USB |
| `A fatal error: Failed to connect` | ESP32 not in download mode | Hold **BOOT** button on the board while upload starts |
| `No wake-net model found` | Model partition is empty | Run Step 4 (pack) then Scenario D (flash model) |
| `first_model='(none)'` | Same as above | Same fix |
| Build fails after pulling new code | Dependencies changed | Run `pio run -t clean` then `pio run` |
| Serial monitor shows garbled text | Wrong baud rate | Make sure monitor is set to `115200` |
| Upload fails: port busy | Serial monitor is still open | Press Ctrl+C to close monitor, then upload |

---

## 9. The Golden Rule

```
Edit code
   ↓
pio run          ← compile, catch errors early
   ↓
Fix errors
   ↓
pio run -t upload    ← flash to ESP32
   ↓
pio device monitor   ← verify it works
   ↓
git push             ← save to GitHub only after confirmed working
```

---

## 10. Quick Cheat Sheet

```powershell
# Full cycle (most common)
pio run && pio run -t upload && pio device monitor --port COM8 --baud 115200

# Build only
pio run

# Flash only
pio run -t upload

# Monitor only
pio device monitor --port COM8 --baud 115200

# Clean build
pio run -t clean

# Reflash voice model only
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash 0x310000 managed_components/espressif__esp-sr/model/target/srmodels.bin

# Push to GitHub
git add src/<file> && git commit -m "message" && git push origin Thirdbranch
```
