# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(app_sources 
    main.cpp
    sensor_handler.cpp
    appliance_control.cpp
)

idf_component_register(SRCS ${app_sources}
                      REQUIRES driver freertos esp_common esp_timer log
                      INCLUDE_DIRS ".")
