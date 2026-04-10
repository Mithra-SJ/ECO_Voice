/*
 * voice_handler.h — ECO Voice Online Version
 * ESP-SR wake word + command recognition via INMP441
 *
 * Wake word : "hi esp"
 * Commands  : "light on", "light off", "fan on", "fan off"
 * Flow      : always listening → wake word → 10s command window → execute → repeat
 */

#ifndef VOICE_HANDLER_H
#define VOICE_HANDLER_H

#include <Arduino.h>
#include "appliance_control.h"
#include "firebase_handler.h"
#include "esp_wn_iface.h"
#include "esp_mn_iface.h"
#include "model_path.h"
#include "esp_mn_speech_commands.h"

typedef struct {
    model_iface_data_t *wn_model;
    model_iface_data_t *mn_model;
    esp_wn_iface_t     *wn_iface;
    esp_mn_iface_t     *mn_iface;
} sr_handle_t;

class VoiceHandler {
public:
    VoiceHandler();
    bool init(ApplianceControl* appliances, FirebaseHandler* firebase);
    void startTask();
    bool isInitialized() const { return initialized; }

private:
    ApplianceControl* _appliances;
    FirebaseHandler*  _firebase;
    bool              initialized;
    sr_handle_t       sr;
    int16_t           audioBuffer[16000];  // 1s at 16kHz, 16-bit mono

    static void voiceTask(void* param);
    void runPipeline();
    void handleCommand(int cmdId);
    void configureI2S();
};

#endif // VOICE_HANDLER_H
