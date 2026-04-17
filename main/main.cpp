/*
 * main.cpp — ECO Voice TEST BENCH
 *
 * Purpose : Serial-driven hardware validation for college project data collection.
 *           Tests all components except voice (ESP-SR requires model partition flash).
 *
 * How to use:
 *   1. Flash this firmware via: pio run --target upload
 *   2. Open serial monitor at 115200 baud: pio device monitor
 *   3. Type a single letter command and press SEND
 *
 * ─── MENU ──────────────────────────────────────────────────────────────────
 *   s — Sensor snapshot        : read all sensors once, print table
 *   c — Continuous stream      : toggle auto-print every 2s
 *   l — Light relay            : toggle ON/OFF
 *   f — Fan relay              : toggle ON/OFF
 *   a — All ON                 : turn both relays ON
 *   z — All OFF                : turn both relays OFF
 *   t — Latency test           : 10 relay toggle cycles, print µs per cycle
 *   g — Guard check            : show which sensor guards would trigger NOW
 *   p — Power snapshot         : print INA219 idle vs active power comparison
 *   r — Test report            : print full summary of all tests run this session
 *   h — Help                   : show this menu
 * ───────────────────────────────────────────────────────────────────────────
 *
 * NOTE — Voice Recognition:
 *   Voice (ESP-SR WakeNet + MultiNet) is NOT tested here.
 *   Reason: requires the 'model' partition to be flashed separately.
 *   To test voice accuracy and latency, use the offline_version branch.
 */

#include <Arduino.h>
#include "config.h"
#include "sensor_handler.h"
#include "appliance_control.h"

// ── Globals ─────────────────────────────────────────────────────────────────
static SensorHandler    sensors;
static ApplianceControl appliances;

static bool continuousMode    = false;
static unsigned long lastAuto = 0;

// ── Session test report data ─────────────────────────────────────────────────
struct TestReport {
    int  sensorSnapshots    = 0;
    int  relayToggles       = 0;
    bool latencyDone        = false;
    float latencyAvgUs      = 0.0f;
    float latencyMinUs      = 0.0f;
    float latencyMaxUs      = 0.0f;
    bool guardDone          = false;
    bool powerDone          = false;
    float idlePower_mW      = 0.0f;
    float activePower_mW    = 0.0f;
    float idleCurrent_mA    = 0.0f;
    float activeCurrent_mA  = 0.0f;
    float idleVoltage_V     = 0.0f;
    float activeVoltage_V   = 0.0f;
} report;

// ── Forward declarations ─────────────────────────────────────────────────────
static void printMenu();
static void doSensorSnapshot();
static void doLatencyTest();
static void doGuardCheck();
static void doPowerSnapshot();
static void doPrintReport();
static void printSeparator(char c = '-', int len = 54);

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(600);

    Serial.println();
    printSeparator('=');
    Serial.println("  ECO Voice — Hardware Test Bench");
    Serial.println("  Branch: testing  |  Baud: 115200");
    printSeparator('=');
    Serial.println();

    appliances.init();
    sensors.init();

    // Blink green LED twice to signal ready
    for (int i = 0; i < 2; i++) {
        digitalWrite(LED_GREEN_PIN, HIGH);
        delay(200);
        digitalWrite(LED_GREEN_PIN, LOW);
        delay(200);
    }
    digitalWrite(LED_GREEN_PIN, HIGH);  // keep green on = bench running

    Serial.println("\n[BENCH] All hardware initialized. System ready.");
    printMenu();
}

// ── Main loop ────────────────────────────────────────────────────────────────
void loop() {
    // Continuous auto-stream
    if (continuousMode && (millis() - lastAuto >= 2000)) {
        lastAuto = millis();
        sensors.update();
        report.sensorSnapshots++;

        Serial.printf("[AUTO] T=%.1f°C  H=%.1f%%  LDR=%d  Motion=%s  "
                      "V=%.2fV  I=%.1fmA  P=%.1fmW\n",
                      sensors.getTemperature(),
                      sensors.getHumidity(),
                      sensors.getLightLevel(),
                      sensors.isMotionDetected() ? "YES" : "NO",
                      sensors.getVoltage(),
                      sensors.getCurrent() * 1000.0f,
                      sensors.getPower() * 1000.0f);
    }

    // Serial command handler
    if (Serial.available()) {
        char cmd = (char)Serial.read();
        // flush rest of line (newline / carriage return)
        while (Serial.available()) Serial.read();

        Serial.printf("\n> Command: '%c'\n", cmd);

        switch (cmd) {
            case 's': doSensorSnapshot();     break;

            case 'c':
                continuousMode = !continuousMode;
                Serial.printf("[BENCH] Continuous stream: %s\n",
                              continuousMode ? "ON (every 2s)" : "OFF");
                break;

            case 'l':
                appliances.setLight(!appliances.isLightOn());
                report.relayToggles++;
                Serial.printf("[BENCH] Light relay -> %s\n",
                              appliances.isLightOn() ? "ON" : "OFF");
                break;

            case 'f':
                appliances.setFan(!appliances.isFanOn());
                report.relayToggles++;
                Serial.printf("[BENCH] Fan relay -> %s\n",
                              appliances.isFanOn() ? "ON" : "OFF");
                break;

            case 'a':
                appliances.setLight(true);
                appliances.setFan(true);
                report.relayToggles += 2;
                Serial.println("[BENCH] All appliances ON");
                break;

            case 'z':
                appliances.turnOffAll();
                report.relayToggles += 2;
                Serial.println("[BENCH] All appliances OFF");
                break;

            case 't': doLatencyTest();        break;
            case 'g': doGuardCheck();         break;
            case 'p': doPowerSnapshot();      break;
            case 'r': doPrintReport();        break;
            case 'h': printMenu();            break;

            default:
                if (cmd >= 32)  // ignore control chars silently
                    Serial.printf("[BENCH] Unknown command '%c' — type 'h' for menu\n", cmd);
                break;
        }
    }
}

// ── Sensor Snapshot ──────────────────────────────────────────────────────────
static void doSensorSnapshot() {
    sensors.update();
    report.sensorSnapshots++;

    printSeparator();
    Serial.println("  SENSOR SNAPSHOT");
    printSeparator();
    Serial.printf("  Temperature   : %.1f °C    (fan guard threshold : %.1f °C)\n",
                  sensors.getTemperature(), (float)TEMP_LOW_THRESHOLD);
    Serial.printf("  Humidity      : %.1f %%    (fan guard threshold : %.1f %%)\n",
                  sensors.getHumidity(), (float)HUMIDITY_LOW_THRESHOLD);
    Serial.printf("  Light Level   : %d / 4095  (light guard threshold: %d)\n",
                  sensors.getLightLevel(), BRIGHTNESS_THRESHOLD);
    Serial.printf("  Motion (PIR)  : %s\n",
                  sensors.isMotionDetected() ? "DETECTED" : "not detected");
    Serial.printf("  Voltage       : %.3f V    (low threshold: %.1f V)\n",
                  sensors.getVoltage(), (float)LOW_VOLTAGE_THRESHOLD);
    Serial.printf("  Current       : %.2f mA\n",   sensors.getCurrent() * 1000.0f);
    Serial.printf("  Power         : %.2f mW\n",   sensors.getPower()   * 1000.0f);
    Serial.printf("  Voltage OK    : %s\n",
                  sensors.isVoltageLow() ? "LOW — WARNING" : "OK");
    Serial.printf("  Voltage Stable: %s\n",
                  sensors.isVoltageFluctuating() ? "FLUCTUATING — WARNING" : "Stable");
    printSeparator();
}

// ── Latency Test ─────────────────────────────────────────────────────────────
/*
 * Measures relay toggle latency at firmware level (GPIO write time).
 * This is the time from "command received" to "relay pin written".
 * 10 ON + 10 OFF cycles = 20 measurements.
 */
static void doLatencyTest() {
    const int CYCLES = 10;

    printSeparator();
    Serial.println("  LATENCY TEST — Relay Toggle (firmware GPIO level)");
    Serial.printf ("  %d ON + %d OFF cycles = %d measurements\n",
                   CYCLES, CYCLES, CYCLES * 2);
    printSeparator();

    float times[CYCLES * 2];
    float total = 0;
    float minUs = 1e9f;
    float maxUs = 0;

    // Make sure relay starts OFF
    appliances.setLight(false);
    delay(50);

    for (int i = 0; i < CYCLES; i++) {
        // Measure ON
        uint32_t t0 = micros();
        appliances.setLight(true);
        uint32_t t1 = micros();
        float onUs = (float)(t1 - t0);
        times[i * 2] = onUs;

        delay(30);

        // Measure OFF
        t0 = micros();
        appliances.setLight(false);
        t1 = micros();
        float offUs = (float)(t1 - t0);
        times[i * 2 + 1] = offUs;

        delay(30);

        Serial.printf("  Cycle %2d:  ON = %6.1f µs   OFF = %6.1f µs\n",
                      i + 1, onUs, offUs);

        total += onUs + offUs;
        if (onUs  < minUs) minUs = onUs;
        if (offUs < minUs) minUs = offUs;
        if (onUs  > maxUs) maxUs = onUs;
        if (offUs > maxUs) maxUs = offUs;
    }

    float avg = total / (float)(CYCLES * 2);
    report.latencyDone   = true;
    report.latencyAvgUs  = avg;
    report.latencyMinUs  = minUs;
    report.latencyMaxUs  = maxUs;
    report.relayToggles += CYCLES * 2;

    printSeparator();
    Serial.printf("  Average : %.1f µs  (%.3f ms)\n", avg, avg / 1000.0f);
    Serial.printf("  Min     : %.1f µs\n", minUs);
    Serial.printf("  Max     : %.1f µs\n", maxUs);
    printSeparator();
    Serial.println("  NOTE: This is firmware GPIO latency.");
    Serial.println("  End-to-end voice latency must be measured manually");
    Serial.println("  (stopwatch from speaking → relay click).");
    printSeparator();
}

// ── Guard Check ──────────────────────────────────────────────────────────────
static void doGuardCheck() {
    sensors.update();
    report.guardDone = true;

    bool pirGuard   = !sensors.isMotionDetected();
    bool ldrGuard   = sensors.getLightLevel() > BRIGHTNESS_THRESHOLD;
    bool tempGuard  = sensors.getTemperature() < TEMP_LOW_THRESHOLD;
    bool humGuard   = sensors.getHumidity() < HUMIDITY_LOW_THRESHOLD;
    bool fanGuard   = tempGuard || humGuard;
    bool voltGuard  = sensors.isVoltageLow();
    bool fluctGuard = sensors.isVoltageFluctuating();

    printSeparator();
    Serial.println("  SENSOR GUARD CHECK — Would any guard trigger RIGHT NOW?");
    printSeparator();

    // PIR guard
    Serial.printf("  PIR  (motion)   : %-10s → Light/Fan guard: %s\n",
                  sensors.isMotionDetected() ? "DETECTED" : "NO motion",
                  pirGuard ? "ACTIVE  ← would warn before command" : "inactive");

    // LDR guard
    Serial.printf("  LDR  (light)    : %4d / 4095  → Light-ON guard: %s\n",
                  sensors.getLightLevel(),
                  ldrGuard ? "ACTIVE  ← already bright, would warn" : "inactive");

    // DHT11 guards
    Serial.printf("  TEMP (DHT11)    : %.1f °C     → Fan guard: %s\n",
                  sensors.getTemperature(),
                  tempGuard ? "ACTIVE  ← too cold for fan" : "inactive");
    Serial.printf("  HUMI (DHT11)    : %.1f %%     → Fan guard: %s\n",
                  sensors.getHumidity(),
                  humGuard ? "ACTIVE  ← too dry for fan" : "inactive");

    // INA219 guards
    Serial.printf("  VOLT (INA219)   : %.3f V   → Voltage guard: %s\n",
                  sensors.getVoltage(),
                  voltGuard ? "ACTIVE  ← voltage too low" : "inactive");
    Serial.printf("  FLUC (INA219)   : %-10s → Stability guard: %s\n",
                  sensors.isVoltageFluctuating() ? "unstable" : "stable",
                  fluctGuard ? "ACTIVE  ← fluctuation detected" : "inactive");

    printSeparator();
    int triggered = (int)pirGuard + (int)ldrGuard + (int)fanGuard +
                    (int)voltGuard + (int)fluctGuard;
    Serial.printf("  Guards active   : %d / 5\n", triggered);
    printSeparator();
}

// ── Power Snapshot ────────────────────────────────────────────────────────────
/*
 * Records INA219 readings in two states:
 *   Phase 1 — Idle  : both relays OFF
 *   Phase 2 — Active: both relays ON
 *
 * Teammate records these values for the energy efficiency section.
 */
static void doPowerSnapshot() {
    printSeparator();
    Serial.println("  POWER SNAPSHOT (INA219)");
    Serial.println("  Measures idle vs active state power draw");
    printSeparator();

    // Phase 1: Idle (relays off)
    appliances.turnOffAll();
    delay(500);
    sensors.update();

    float idleV = sensors.getVoltage();
    float idleI = sensors.getCurrent() * 1000.0f;  // → mA
    float idleP = sensors.getPower()   * 1000.0f;  // → mW

    Serial.println("  [IDLE] Both relays OFF:");
    Serial.printf ("    Voltage : %.3f V\n", idleV);
    Serial.printf ("    Current : %.2f mA\n", idleI);
    Serial.printf ("    Power   : %.2f mW\n", idleP);

    // Phase 2: Active (both relays on)
    appliances.setLight(true);
    appliances.setFan(true);
    delay(500);
    sensors.update();

    float actV = sensors.getVoltage();
    float actI = sensors.getCurrent() * 1000.0f;
    float actP = sensors.getPower()   * 1000.0f;

    Serial.println("  [ACTIVE] Both relays ON:");
    Serial.printf ("    Voltage : %.3f V\n", actV);
    Serial.printf ("    Current : %.2f mA\n", actI);
    Serial.printf ("    Power   : %.2f mW\n", actP);

    // Delta
    float deltaP = actP - idleP;
    float deltaI = actI - idleI;

    printSeparator();
    Serial.printf("  Power increase (load delta) : +%.2f mW\n", deltaP);
    Serial.printf("  Current increase            : +%.2f mA\n", deltaI);
    printSeparator();

    // Save to report
    report.powerDone        = true;
    report.idleVoltage_V    = idleV;
    report.idleCurrent_mA   = idleI;
    report.idlePower_mW     = idleP;
    report.activeVoltage_V  = actV;
    report.activeCurrent_mA = actI;
    report.activePower_mW   = actP;

    // Leave relays off after test
    appliances.turnOffAll();
    Serial.println("  Relays turned OFF after power test.");
}

// ── Full Test Report ─────────────────────────────────────────────────────────
static void doPrintReport() {
    printSeparator('=');
    Serial.println("  ECO VOICE — SESSION TEST REPORT");
    printSeparator('=');

    // ── Sensor readings
    sensors.update();
    Serial.println("\n  [1] CURRENT SENSOR VALUES");
    Serial.printf ("    Temperature  : %.1f °C\n",  sensors.getTemperature());
    Serial.printf ("    Humidity     : %.1f %%\n",  sensors.getHumidity());
    Serial.printf ("    Light Level  : %d / 4095\n", sensors.getLightLevel());
    Serial.printf ("    Motion       : %s\n",
                   sensors.isMotionDetected() ? "DETECTED" : "Not detected");
    Serial.printf ("    Voltage      : %.3f V\n",   sensors.getVoltage());
    Serial.printf ("    Current      : %.2f mA\n",  sensors.getCurrent() * 1000.0f);
    Serial.printf ("    Power        : %.2f mW\n",  sensors.getPower()   * 1000.0f);

    // ── Latency
    Serial.println("\n  [2] LATENCY TEST");
    if (report.latencyDone) {
        Serial.printf ("    Average GPIO latency : %.1f µs  (%.3f ms)\n",
                       report.latencyAvgUs, report.latencyAvgUs / 1000.0f);
        Serial.printf ("    Min                  : %.1f µs\n", report.latencyMinUs);
        Serial.printf ("    Max                  : %.1f µs\n", report.latencyMaxUs);
        Serial.println("    * End-to-end voice latency: measure manually with stopwatch");
    } else {
        Serial.println("    Not run yet — type 't' to run");
    }

    // ── Guard check
    Serial.println("\n  [3] SENSOR GUARD TEST");
    if (report.guardDone) {
        Serial.println("    Run this session — see guard check output above");
        Serial.println("    To re-run: type 'g'");
    } else {
        Serial.println("    Not run yet — type 'g' to run");
    }

    // ── Power
    Serial.println("\n  [4] POWER / ENERGY TEST (INA219)");
    if (report.powerDone) {
        Serial.println("    State       Voltage      Current     Power");
        Serial.printf ("    Idle        %.3f V     %.2f mA    %.2f mW\n",
                       report.idleVoltage_V, report.idleCurrent_mA, report.idlePower_mW);
        Serial.printf ("    Active      %.3f V     %.2f mA    %.2f mW\n",
                       report.activeVoltage_V, report.activeCurrent_mA, report.activePower_mW);
        Serial.printf ("    Delta       --           +%.2f mA   +%.2f mW\n",
                       report.activeCurrent_mA - report.idleCurrent_mA,
                       report.activePower_mW   - report.idlePower_mW);
    } else {
        Serial.println("    Not run yet — type 'p' to run");
    }

    // ── Voice (not tested here)
    Serial.println("\n  [5] VOICE RECOGNITION");
    Serial.println("    NOT tested in this build.");
    Serial.println("    Use offline_version branch for voice accuracy + latency.");
    Serial.println("    Manual test: speak 50 commands, count correct responses.");

    // ── Session stats
    Serial.println("\n  [SESSION STATS]");
    Serial.printf ("    Sensor snapshots taken : %d\n", report.sensorSnapshots);
    Serial.printf ("    Relay toggles triggered: %d\n", report.relayToggles);
    Serial.printf ("    Latency test complete  : %s\n", report.latencyDone ? "YES" : "no");
    Serial.printf ("    Guard check complete   : %s\n", report.guardDone   ? "YES" : "no");
    Serial.printf ("    Power test complete    : %s\n", report.powerDone   ? "YES" : "no");

    printSeparator('=');
    Serial.println("  Copy this output and send to Kabilan.");
    printSeparator('=');
    Serial.println();
}

// ── Menu ─────────────────────────────────────────────────────────────────────
static void printMenu() {
    printSeparator('=');
    Serial.println("  ECO Voice Test Bench — Command Menu");
    printSeparator('=');
    Serial.println("  s — Sensor snapshot  (read all sensors once)");
    Serial.println("  c — Continuous mode  (toggle auto-stream every 2s)");
    Serial.println("  l — Light relay      (toggle ON/OFF)");
    Serial.println("  f — Fan relay        (toggle ON/OFF)");
    Serial.println("  a — All ON           (both relays)");
    Serial.println("  z — All OFF          (both relays)");
    Serial.println("  t — Latency test     (10 relay cycles, measures µs)");
    Serial.println("  g — Guard check      (sensor threshold status)");
    Serial.println("  p — Power snapshot   (INA219 idle vs active)");
    Serial.println("  r — Test report      (full session summary)");
    Serial.println("  h — Help             (show this menu)");
    printSeparator('=');
    Serial.println("  Type ONE letter and press SEND (Enter)");
    printSeparator('=');
    Serial.println();
}

// ── Utility ──────────────────────────────────────────────────────────────────
static void printSeparator(char c, int len) {
    Serial.print("  ");
    for (int i = 0; i < len; i++) Serial.print(c);
    Serial.println();
}
