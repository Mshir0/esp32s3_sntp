set(priv_requires fatfs console)

if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND priv_requires wear_levelling esp_partition)
endif()

file(GLOB_RECURSE SOURCES usb/*.c )

idf_component_register(SRCS ${SOURCES}
    REQUIRES  driver
              nvs_flash 
              esp_event 
              esp_wifi 
              wpa_supplicant 
    INCLUDE_DIRS ${include_dirs} "../components/wifi")