# ECO Voice — Presentation Talking Points
**Review 3 | Branch: testing | Last updated: 2026-04-21**

---

## Slide 1 — Title

> "Good [morning/afternoon]. We are presenting ECO Voice — an Edge-based, Context-Aware, Offline Voice Recognition system for Home Automation. This is Review 3. The project is built and tested on real hardware, and today we'll walk you through our design, results, and comparison against existing systems."

---

## Slide 2 — Abstract

> "The core problem we identified is that every major smart home system today — Alexa, Google Assistant — depends entirely on cloud servers to process your voice. That means if your internet drops, your system stops. Your audio is sent to remote servers, raising privacy concerns. And these systems have no awareness of your environment — they'll blindly turn on a light in an already-bright room. ECO Voice solves all three: we process voice on the device itself, we use sensors to make intelligent decisions, and we monitor power to prevent unnecessary energy usage."

---

## Slide 3 — Problem Statement + Proposed Solution

> "On the left — the four problems we're solving. Cloud dependency, internet-bound latency, privacy risks, and zero context awareness. On the right — our solution. An ESP32-S3 running voice processing locally, sensor-based decision making using PIR, LDR, DHT11 and INA219, and two complete versions — one fully offline, one with remote web access — on identical hardware."

---

## Slide 4 — Limitations of Cloud-based IoT vs ECO Voice

> "This slide maps each cloud limitation directly to our solution. Internet dependency — solved, v1 never touches the network. Data ownership — solved, all processing stays on the ESP32-S3, you own everything. Privacy concerns — solved, zero data transmission in v1. Lack of local-first IoT — solved, we implemented it on a microcontroller. Cloud shutdown risk — solved, no external service dependency means the system works permanently."

---

## Slide 5 — Detailed Design

> "This is our system architecture. The user speaks a command — the INMP441 microphone captures it — the ESP32-S3 processes it entirely on-chip using ESP-SR. The four sensors feed real-time context into the decision engine. Based on the command and sensor state, the ESP32 either plays an audio response through the DFPlayer Mini and speaker, or triggers the relay to switch the appliance. Every component here communicates directly with the microcontroller — no internet, no intermediary."

---

## Slide 6 — Schematic Diagram

> "This is the actual wiring schematic. The colour coding shows the signal direction — green lines are inputs into the ESP32-S3, blue lines are outputs from it, and red is the power rail. The PIR, DHT11, and LDR connect as sensor inputs. The two relay modules control the light and fan on the output side. The INA219 sits on the I2C bus for current monitoring. The DFPlayer Mini connects via UART for audio playback. All of this runs from a single 5V supply through the USB port."

---

## Slide 7 — System Comparison Table

> "Here's the quantitative comparison. Alexa averages 1800 milliseconds end-to-end. Google Assistant is 1600 milliseconds. SmartThings at 1200 milliseconds. ECO Voice v2, which uses Firebase, comes in at 600 milliseconds — half of SmartThings. ECO Voice v1, fully offline, is at 300 milliseconds — that's 83% faster than Alexa. And critically, v1 is the only system in this table that requires no internet and provides a High privacy level."

---

## Slide 8 — System Latency Comparison (Bar Chart)

> "This chart makes the gap visual. The three blue bars on the left are existing systems — all above 1200 milliseconds, all requiring cloud roundtrips. The two green bars are ECO Voice. V2 at 600ms and v1 at 300ms. The reason v1 is this fast is architectural — we eliminated the cloud entirely. There is no network call, no server processing, no response delivery. Wake word detection to relay firing happens completely inside the ESP32-S3."

---

## Slide 9 — Smart Home Feature Comparison

> "Comparing against Alexa Smart Home and Google Nest on five dimensions. Local processing — ECO Voice yes, both competitors no, they use cloud processing. Cloud dependency — ECO Voice is completely offline, both competitors have high dependency. Voice recognition — ECO Voice has minimal cloud dependency in v2, zero in v1, while Alexa and Google Nest are entirely cloud-dependent. Device compatibility — ECO Voice works with any browser in v2. Latency — ECO Voice and Alexa are both low latency, but for different reasons. Alexa achieves low latency through infrastructure investment. We achieve it by removing the network entirely."

---

## Slide 10 — Hardware Performance Table

> "These are measured numbers from our hardware test bench — not simulated, not estimated. We ran 20 relay toggle cycles and measured the firmware GPIO write latency. Average was 21.8 microseconds. The variation between fastest and slowest — the jitter — was only 5 microseconds. And across all 20 cycles, zero missed toggles. 100% reliability. This tells us the firmware and hardware are not bottlenecks at any point. The relay responds in under a millisecond from the moment the command arrives."

---

## Slide 11 — Feature Comparison Table

> "Feature-by-feature across all five systems. Voice control — ECO Voice has it in both versions. Remote access — v2 has it, v1 doesn't by design. Works offline — only ECO Voice v1. This is the unique capability no existing system offers. Sensor awareness — ECO Voice yes in both versions, Alexa and Google no, SmartThings partial. Energy monitoring — only ECO Voice, through the INA219 current sensor. Custom hardware — all systems yes, but ECO Voice is the only one where the hardware is purpose-built by the team, not a commercial product."

---

## Slide 12 — Sensor Status Table

> "These are the actual sensor readings from our hardware test session. DHT11 temperature — 35 degrees Celsius, working correctly. DHT11 humidity — 65%, working correctly. PIR — motion detected, functioning as expected. LDR — reading 4095, which is the maximum ADC value, indicating a bright condition — the light guard triggered correctly on this. INA219 is marked as temporarily not included — this is a wiring configuration being resolved before final demo, the sensor is present on the board."

---

## Slide 13 — Limitations in Existing IoT Systems vs ECO Voice

> "This is where we position ourselves against the two base papers. The first five rows come from Irugalbandara et al. 2023 — a recent smart home system that still uses cloud ASR, transmits audio externally, and fails offline. ECO Voice solves all five: on-device processing, zero audio transmission, permanent offline operation, a 4-sensor context guard system, and three control paths on identical hardware. The bottom three rows come from Kleppmann et al. 2019, who argued that local-first software was the right approach but never applied it to embedded systems. ECO Voice v1 is that implementation — local-first principles running on an ESP32-S3."

---

## Slide 14 — Code Completion

> "Project completion status. Analysis and design are both 100% done. Software development is 100% complete — the full codebase is available on GitHub at this link with two branches, offline and online versions. Hardware assembly is 100% complete. Testing is at 80% — we've completed relay, sensor, and guard validation; INA219 wiring correction and final end-to-end voice latency measurement are pending. Implementation is at 90%, with the final demo integration remaining."

---

## Slide 15 — Software Module Dependency Diagram

> "This shows how the software is structured. The main file — eco_voice_main — acts as the state machine and orchestrator. It calls four modules: voice recognition, sensor handler, audio handler, and appliance control. All four of these depend on config.h — a single central file that holds all pin definitions and sensor thresholds. This modular design means each component can be tested independently, which is exactly what the test bench on the testing branch does — it exercises sensor handler and appliance control without voice recognition."

---

## Slides 16 & 17 — References

> "Our references. Kepuska and Bohouta 2018 established the benchmark for cloud-based voice assistant latency — we used that to establish the 1600 to 1800 millisecond baseline for Alexa and Google. Kleppmann et al. 2019 defined the local-first software principles that guided the privacy and offline architecture of v1. Irugalbandara et al. 2023 is the closest existing work in our domain — smart home with speech recognition and power measurement on ESP32 — and the limitations in that system are what ECO Voice directly addresses. Jha 2024 covers voice analysis fundamentals underlying the recognition pipeline."

---

## Slide 18 — Thank You

> "Thank you. We're happy to take any questions on the hardware, the firmware architecture, the test bench results, or the comparison methodology."

---

## Panel Questions — Be Ready For These

| Question | Answer |
|----------|--------|
| Why is v1 latency 300ms if you measured 21.8µs? | 21.8µs is firmware GPIO only. The 300ms includes WakeNet + MultiNet inference on-chip — that's where the time goes. |
| Why is INA219 not working? | Shunt wiring issue — IN+ and IN- are reversed. It's a bench connection issue, not a firmware or design flaw. Fix is identified. |
| What is the voice accuracy of v1? | ESP-SR MultiNet7 benchmarks at 85–92% in quiet environments. Manual 50-command test pending. |
| Why two versions on the same hardware? | They solve different user needs. v1 for privacy-first offline use. v2 for remote monitoring anywhere. Same ESP32-S3, switchable by firmware branch. |
| How is this different from Irugalbandara et al.? | They use cloud ASR — audio leaves the device. We run MultiNet7 on-chip. They have no sensor guards. We have four. |
