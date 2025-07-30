file(GLOB_RECURSE SOURCES ws2812/*.c  oled/*.c wifi/*c sntp/*c )

set(include_dirs 
    ws2812
    oled
    wifi
    sntp
    )
idf_component_register(SRCS ${SOURCES}
                    REQUIRES driver
                             nvs_flash 
                             esp_event 
                             esp_wifi 
                             wpa_supplicant 
                    INCLUDE_DIRS ${include_dirs})
