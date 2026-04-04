# ECO_Voice — main component build file

set(app_sources
    "main.cpp"
    "audio_handler.cpp"
    "sensor_handler.cpp"
    "appliance_control.cpp"
    "voice_recognition.cpp"
    "dht11.c"
)

idf_component_register(
    SRCS
        ${app_sources}
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp-sr
        freertos
        esp_common
        esp_timer
        log
        nvs_flash
        spiffs
)
