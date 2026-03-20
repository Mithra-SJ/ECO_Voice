/*
 * Voice Recognition Implementation
 *
 * DEMO MODE:  Serial input simulates voice commands for bench testing.
 * PRODUCTION: Energy-based VAD (Voice Activity Detection) reads from
 *             INMP441 microphone. Replace inner detection with ESP-SR
 *             WakeNet/MultiNet for full offline keyword recognition.
 */

#include "voice_recognition.h"
#include "sensor_handler.h"   // full include needed here for update() call
#include "config.h"
#include "secrets.h"

VoiceRecognition::VoiceRecognition() : initialized(false), sensorHandler(nullptr) {
}

bool VoiceRecognition::init(SensorHandler* sensors) {
    Serial.println("Initializing Voice Recognition...");

    sensorHandler = sensors;
    configureI2S();

    Serial.println("Voice Recognition Ready.");
    Serial.println("DEMO MODE: Serial input active. Replace with ESP-SR for production.");

    initialized = true;
    return true;
}

void VoiceRecognition::configureI2S() {
    // Configure I2S for INMP441 microphone — all values sourced from config.h
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = BITS_PER_SAMPLE,           // from config.h
        .channel_format       = I2S_CHANNEL,               // from config.h
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 1024,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pin_config = {
        .mck_io_num   = I2S_PIN_NO_CHANGE,
        .bck_io_num   = I2S_SCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD_PIN
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);

    Serial.println("I2S Microphone Configured.");
}

bool VoiceRecognition::detectWakeWord() {
    // --- DEMO MODE: Serial input for bench testing ---
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toLowerCase();
        if (input == WAKE_WORD) return true;   // WAKE_WORD from config.h
        return false;  // non-wake input consumed — do not fall through to VAD
    }

    // --- PRODUCTION MODE: Energy-based VAD + ESP-SR WakeNet ---
    readAudioSample();
    float energy = calculateEnergy();  // compute once, pass down — no double calculation

    if (energy > WAKE_THRESHOLD) {
        // Voice activity detected. Pass energy to VAD.
        // TODO: Replace with ESP-SR WakeNet:
        //   int result = wakenet->detect(wakenet_model, (int16_t*)audioBuffer);
        //   return (result > 0);
        return detectVoiceActivity(energy);
    }

    return false;
}

String VoiceRecognition::recognizeCommand() {
    // Blocking loop — sensors are updated each tick to stay fresh
    unsigned long startTime = millis();

    while (millis() - startTime < COMMAND_TIMEOUT_MS) {
        // Keep sensors current during the wait
        if (sensorHandler) sensorHandler->update();

        // --- DEMO MODE ---
        if (Serial.available() > 0) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            input.toLowerCase();

            if (input == "light on"  || input == "1") return "LIGHT_ON";
            if (input == "light off" || input == "2") return "LIGHT_OFF";
            if (input == "fan on"    || input == "3") return "FAN_ON";
            if (input == "fan off"   || input == "4") return "FAN_OFF";
            if (input == "status"    || input == "5") return "STATUS";
            if (input == "lock"      || input == "6") return "LOCK";
            Serial.println("[DEMO] Unrecognized. Try: 'light on', 'light off', 'fan on', 'fan off', 'status', 'lock'");
        }

        // --- PRODUCTION MODE: ESP-SR MultiNet ---
        // readAudioSample();
        // mn_result_t *result = multinet->detect(multinet_model, (int16_t*)audioBuffer);
        // if (result) return String(result->string);

        delay(100);
    }

    return ""; // timeout
}

String VoiceRecognition::recognizeSecretCode() {
    // Non-blocking by design: called repeatedly from handleAuthenticationState()
    // which owns the 30-second AUTH_TIMEOUT_MS window. No internal loop needed.
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        return input;
    }

    // --- PRODUCTION MODE ---
    // readAudioSample();
    // if (calculateEnergy() > WAKE_THRESHOLD) return processAudio();

    return "";
}

bool VoiceRecognition::verifySecretCode(const String& code) {
    String input = code;         // local copy to allow toLowerCase()
    input.toLowerCase();
    String secret = String(SECRET_CODE);  // SECRET_CODE from secrets.h
    secret.toLowerCase();
    return (input == secret);
}

String VoiceRecognition::recognizeYesNo() {
    Serial.println("Waiting for Yes/No...");
    unsigned long startTime = millis();

    while (millis() - startTime < YES_NO_TIMEOUT_MS) {  // YES_NO_TIMEOUT_MS from config.h
        // Keep sensors current during the wait
        if (sensorHandler) sensorHandler->update();

        // --- DEMO MODE ---
        if (Serial.available() > 0) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            input.toLowerCase();

            if (input == "yes" || input == "y") return "YES";
            if (input == "no"  || input == "n") return "NO";
        }

        // --- PRODUCTION MODE ---
        // readAudioSample();
        // if (calculateEnergy() > WAKE_THRESHOLD) {
        //     String response = processAudio();
        //     if (response == "yes" || response == "no") return response.toUpperCase();
        // }

        delay(100);
    }

    return "NO"; // default to NO on timeout — safer
}

void VoiceRecognition::readAudioSample() {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);
}

float VoiceRecognition::calculateEnergy() {
    float energy = 0;
    for (int i = 0; i < 512; i++) {
        // INMP441 outputs left-aligned 24-bit audio in a 32-bit I2S word.
        // Shift right 16 bits to bring the significant bits into 16-bit range.
        energy += abs((int32_t)audioBuffer[i] >> 16);
    }
    return energy / 512.0;
}

bool VoiceRecognition::detectVoiceActivity(float energy) {
    // Energy-based Voice Activity Detection — placeholder for ESP-SR WakeNet.
    // Energy is pre-computed by caller — no redundant buffer re-scan.
    // Does NOT discriminate keywords — any loud sound triggers this.
    // Replace entirely with:
    //   int r = wakenet->detect(wakenet_model, (int16_t*)audioBuffer);
    //   return (r > 0);
    return (energy > WAKE_THRESHOLD * WAKE_CONFIDENCE);  // WAKE_CONFIDENCE from config.h
}

/* ===== ESP-SR INTEGRATION (Production) =====
 *
 * 1. Install ESP-SR: idf.py add-dependency "espressif/esp-sr"
 * 2. In menuconfig: Enable WakeNet + MultiNet
 * 3. Train custom "ECO" wake word via ESP-SR tools (~100 voice samples)
 * 4. Replace detectVoiceActivity() with:
 *       int r = wakenet->detect(wakenet_model, (int16_t*)audioBuffer);
 *       return (r > 0);
 * 5. Replace recognizeCommand() body with:
 *       mn_result_t *r = multinet->detect(multinet_model, (int16_t*)audioBuffer);
 *       if (r) return String(r->string);
 *
 * See Docs/ESP_SR_INTEGRATION.md for full setup guide.
 */
