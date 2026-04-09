/*
 * main.cpp — ECO Voice Online Version
 * ESP32-S3 + Firebase Realtime Database
 *
 * Flow:
 *   1. Connect to WiFi
 *   2. Connect to Firebase (authenticate as device account)
 *   3. Every 2s: push sensor readings + device status
 *   4. Every 500ms: check for appliance commands from web app
 */

#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include "config.h"
#include "sensor_handler.h"
#include "appliance_control.h"
#include "firebase_handler.h"

static SensorHandler    sensors;
static ApplianceControl appliances;
static FirebaseHandler  firebase;

static unsigned long lastSensorPush   = 0;
static unsigned long lastCommandCheck = 0;

// ── WiFi connection ─────────────────────────────────────────────
static void connectWiFi() {
    Serial.printf("\n[WIFI] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 20000) {
            Serial.println("\n[WIFI] Connection failed — restarting in 5s");
            delay(5000);
            ESP.restart();
        }
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\n[WIFI] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
}

// ── Arduino entry points ────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ECO Voice Online Version ===");

    appliances.init();
    sensors.init();

    connectWiFi();

    firebase.init();

    if (firebase.isReady()) {
        appliances.setOnlineLED(true);
        Serial.println("[MAIN] System ready.");
    } else {
        Serial.println("[MAIN] Firebase not ready — running offline (sensors only).");
    }
}

void loop() {
    // Reconnect WiFi if dropped
    if (WiFi.status() != WL_CONNECTED) {
        appliances.setOnlineLED(false);
        connectWiFi();
    }

    unsigned long now = millis();

    // Push sensor data + status every 2 seconds
    if (now - lastSensorPush >= SENSOR_PUSH_INTERVAL_MS) {
        lastSensorPush = now;
        sensors.update();
        firebase.pushSensors(sensors);
        firebase.pushStatus(appliances);
        appliances.setOnlineLED(firebase.isReady());
    }

    // Check for commands from web app every 500ms
    if (now - lastCommandCheck >= COMMAND_POLL_INTERVAL_MS) {
        lastCommandCheck = now;
        firebase.checkCommands(appliances);
    }
}
