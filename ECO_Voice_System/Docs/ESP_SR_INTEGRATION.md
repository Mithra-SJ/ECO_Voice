# ESP-SR Integration Guide
## Offline Voice Recognition with Espressif Speech Recognition

ESP-SR is the official speech recognition framework from Espressif, optimized for ESP32-S3's AI acceleration capabilities.

## Overview

ESP-SR provides two main components:
1. **WakeNet**: Wake word detection engine
2. **MultiNet**: Command word recognition engine

## Prerequisites

- ESP-IDF v5.1 or later
- ESP32-S3 with PSRAM
- INMP441 or similar I2S microphone

## Installation

### Step 1: Clone ESP-SR Repository
```bash
cd ~/esp
git clone --recursive https://github.com/espressif/esp-sr.git
```

### Step 2: Set Up ESP-IDF Environment
```bash
cd ~/esp/esp-idf
. ./export.sh
```

### Step 3: Create Project
```bash
cd ~/esp
cp -r esp-sr/examples/wake_word_detection eco_voice_esp_sr
cd eco_voice_esp_sr
```

## Configuration

### Step 1: Configure WakeNet for "ECO" Wake Word

ESP-SR comes with pre-trained models, but you can also train custom wake words.

#### Using Pre-trained Model (Quick Start)
ESP-SR includes "Hi Lexin" and other Chinese wake words. For English, you'll need to train a custom model.

#### Training Custom "ECO" Wake Word

1. **Collect Audio Samples**
   - Record 100+ samples of "ECO" in different:
     - Voices (male, female, children)
     - Accents
     - Speaking speeds
     - Background noise conditions
   - Format: 16kHz, 16-bit, mono WAV

2. **Prepare Dataset Structure**
   ```
   wake_word_dataset/
   ├── eco/
   │   ├── sample_001.wav
   │   ├── sample_002.wav
   │   └── ...
   └── negative/
       ├── noise_001.wav
       ├── speech_001.wav
       └── ...
   ```

3. **Train Model Using ESP-SR Tools**
   ```bash
   cd esp-sr/tool/wake_word_training
   python train.py --dataset ../../../wake_word_dataset \
                   --word eco \
                   --epochs 50 \
                   --output eco_model
   ```

4. **Export Quantized Model**
   ```bash
   python quantize.py --model eco_model \
                      --output eco_model_quant.esrnn
   ```

5. **Deploy Model**
   ```bash
   cp eco_model_quant.esrnn ~/esp/eco_voice_esp_sr/model/
   ```

### Step 2: Configure MultiNet for Commands

Create speech command configuration file `speech_commands.h`:

```c
#ifndef SPEECH_COMMANDS_H
#define SPEECH_COMMANDS_H

typedef enum {
    CMD_LIGHT_ON = 0,
    CMD_LIGHT_OFF,
    CMD_FAN_ON,
    CMD_FAN_OFF,
    CMD_STATUS,
    CMD_LOCK,
    CMD_YES,
    CMD_NO
} command_id_t;

// Command phrases
static const esp_mn_phrase_t phrases[] = {
    {CMD_LIGHT_ON,   "Turn on the light",  "light on"},
    {CMD_LIGHT_OFF,  "Turn off the light", "light off"},
    {CMD_FAN_ON,     "Turn on the fan",    "fan on"},
    {CMD_FAN_OFF,    "Turn off the fan",   "fan off"},
    {CMD_STATUS,     "Tell me status",     "status"},
    {CMD_LOCK,       "Lock the system",    "lock"},
    {CMD_YES,        "Yes",                "yes"},
    {CMD_NO,         "No",                 "no"}
};

#endif
```

## ESP-SR Code Implementation

### main.c (ESP-IDF Version)

```c
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "driver/i2s.h"
#include "speech_commands.h"

// WakeNet model
static const esp_wn_iface_t *wakenet = &WAKENET_MODEL;
static model_iface_data_t *wn_model_data;

// MultiNet model
static const esp_mn_iface_t *multinet = &MULTINET_MODEL;
static model_iface_data_t *mn_model_data;

// AFE (Audio Front End)
static const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
static afe_config_t afe_config;
static esp_afe_sr_data_t *afe_data;

// I2S configuration for INMP441
#define I2S_CHANNEL_NUM  1
#define I2S_SAMPLE_RATE  16000

void i2s_init() {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 41,      // SCK
        .ws_io_num = 42,       // WS
        .data_out_num = -1,
        .data_in_num = 2       // SD
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void wake_word_detection_task(void *arg) {
    // Initialize WakeNet
    wn_model_data = wakenet->create(DET_MODE_90);

    printf("ECO Voice System Ready\n");
    printf("Say 'ECO' to wake up...\n");

    int16_t *audio_buffer = (int16_t *)malloc(sizeof(int16_t) * 512);
    size_t bytes_read;

    while (1) {
        // Read audio from I2S
        i2s_read(I2S_NUM_0, audio_buffer, sizeof(int16_t) * 512,
                 &bytes_read, portMAX_DELAY);

        // Feed to AFE (noise reduction, AEC, etc.)
        afe_handle->feed(afe_data, audio_buffer);

        // Fetch processed audio
        int afe_fetch_chan = afe_handle->fetch(afe_data);

        if (afe_fetch_chan == AFE_FETCH_CHANNEL_1) {
            int16_t *processed = afe_handle->get_samp_chunkdata(afe_data, 0);
            int chunk_len = afe_handle->get_samp_chunklen(afe_data);

            // Detect wake word
            int wakeword_detected = wakenet->detect(wn_model_data, processed);

            if (wakeword_detected) {
                printf("Wake word 'ECO' detected!\n");
                // Trigger audio feedback
                // speak("Listening for secret code");

                // Start command recognition
                xTaskCreate(command_recognition_task, "cmd_recog",
                           4096, NULL, 5, NULL);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void command_recognition_task(void *arg) {
    // Initialize MultiNet with command phrases
    mn_model_data = multinet->create(phrases, sizeof(phrases)/sizeof(phrases[0]));

    printf("Listening for commands...\n");

    int16_t *audio_buffer = (int16_t *)malloc(sizeof(int16_t) * 512);
    size_t bytes_read;

    TickType_t start_time = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(30000); // 30 second timeout

    while (1) {
        // Check timeout
        if ((xTaskGetTickCount() - start_time) > timeout) {
            printf("Command timeout\n");
            break;
        }

        // Read audio
        i2s_read(I2S_NUM_0, audio_buffer, sizeof(int16_t) * 512,
                 &bytes_read, portMAX_DELAY);

        // Feed to AFE
        afe_handle->feed(afe_data, audio_buffer);

        int afe_fetch_chan = afe_handle->fetch(afe_data);

        if (afe_fetch_chan == AFE_FETCH_CHANNEL_1) {
            int16_t *processed = afe_handle->get_samp_chunkdata(afe_data, 0);

            // Recognize command
            esp_mn_state_t mn_state = multinet->detect(mn_model_data, processed);

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *result = multinet->get_results(mn_model_data);

                printf("Command detected: %s (ID: %d)\n",
                       result->phrase_id < sizeof(phrases)/sizeof(phrases[0])
                       ? phrases[result->phrase_id].phrase
                       : "Unknown",
                       result->phrase_id);

                // Process command
                process_command(result->phrase_id);

                // Reset timeout after successful command
                start_time = xTaskGetTickCount();
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // Cleanup
    multinet->destroy(mn_model_data);
    free(audio_buffer);
    vTaskDelete(NULL);
}

void process_command(int cmd_id) {
    switch (cmd_id) {
        case CMD_LIGHT_ON:
            // Check sensors and turn on light
            handle_light_on();
            break;
        case CMD_LIGHT_OFF:
            set_light(false);
            speak("Light turned off");
            break;
        case CMD_FAN_ON:
            handle_fan_on();
            break;
        case CMD_FAN_OFF:
            set_fan(false);
            speak("Fan turned off");
            break;
        case CMD_STATUS:
            report_status();
            break;
        case CMD_LOCK:
            lock_system();
            break;
        default:
            speak("Command not recognized");
    }
}

void app_main() {
    // Initialize I2S
    i2s_init();

    // Initialize AFE
    afe_config.wakenet_model = wakenet;
    afe_config.aec_init = false;
    afe_config.se_init = true;
    afe_config.vad_init = true;
    afe_config.wakenet_init = true;
    afe_config.voice_communication_init = false;
    afe_config.voice_communication_agc_init = false;
    afe_config.voice_communication_agc_gain = 15;
    afe_config.vad_mode = VAD_MODE_3;
    afe_config.wakenet_mode = DET_MODE_90;
    afe_config.afe_mode = SR_MODE_LOW_COST;
    afe_config.afe_perferred_core = 0;
    afe_config.afe_perferred_priority = 5;
    afe_config.afe_ringbuf_size = 50;
    afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    afe_config.afe_linear_gain = 1.0;
    afe_config.agc_mode = AFE_MN_PEAK_AGC_MODE_2;
    afe_config.pcm_config.total_ch_num = 1;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 0;

    afe_data = afe_handle->create_from_config(&afe_config);

    // Initialize sensors and appliances
    sensors_init();
    appliances_init();

    // Start wake word detection task
    xTaskCreate(wake_word_detection_task, "wake_detect", 8192, NULL, 5, NULL);
}
```

## Building and Flashing

### Step 1: Configure Project
```bash
cd ~/esp/eco_voice_esp_sr
idf.py menuconfig
```

Navigate to:
- **Component config → ESP Speech Recognition**
  - Select WakeNet model
  - Select MultiNet model
  - Enable PSRAM support

### Step 2: Build
```bash
idf.py build
```

### Step 3: Flash
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Performance Optimization

### 1. Model Selection
- **WakeNet9**: Best accuracy, higher resource usage
- **WakeNet8**: Good balance
- **WakeNet5**: Lower resource usage

### 2. Detection Mode
- **DET_MODE_90**: 90% sensitivity (recommended)
- **DET_MODE_95**: 95% sensitivity (more false positives)

### 3. PSRAM Configuration
Enable PSRAM in menuconfig:
- **Component config → ESP PSRAM → Support for external SPI-connected RAM**

### 4. CPU Frequency
Set to 240MHz for best performance:
- **Component config → ESP System Settings → CPU frequency → 240MHz**

## Accuracy Improvements

1. **Noise Suppression**: Enable AFE noise suppression
2. **AGC (Automatic Gain Control)**: Adjust for varying voice volumes
3. **VAD (Voice Activity Detection)**: Reduce false triggers
4. **Multi-Microphone**: Use 2-3 mics for beamforming (advanced)

## Testing Checklist

- [ ] Wake word detection accuracy > 90%
- [ ] False positive rate < 1 per hour
- [ ] Command recognition accuracy > 85%
- [ ] Response time < 500ms
- [ ] Works at 1-3 meter distance
- [ ] Works with background noise (TV, music)

## Troubleshooting

### Issue: Wake word not detected
- Check microphone connection
- Verify I2S configuration matches INMP441
- Speak louder or closer to mic
- Reduce detection threshold (use DET_MODE_80)

### Issue: Too many false positives
- Increase detection threshold (use DET_MODE_95)
- Enable VAD
- Improve acoustic environment (reduce echoes)

### Issue: Commands not recognized
- Train MultiNet with more samples
- Speak clearly and pause between words
- Check command phrase definitions

## Resources

- ESP-SR GitHub: https://github.com/espressif/esp-sr
- ESP-SR Documentation: https://docs.espressif.com/projects/esp-sr/
- ESP32-S3 Datasheet: https://www.espressif.com/en/products/socs/esp32-s3
- INMP441 Datasheet: https://invensense.tdk.com/products/digital/inmp441/
