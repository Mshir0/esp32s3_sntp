set(priv_requires fatfs console)

if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND priv_requires wear_levelling esp_partition)
endif()

idf_component_register(
    SRCS "tusb_msc_main.c"
    PRIV_REQUIRES "${priv_requires}"
                  spi_flash
    REQUIRES  driver
              nvs_flash 
              esp_event 
              esp_wifi 
              wpa_supplicant 
    INCLUDE_DIRS "../components/ws2812" "../components/oled" "../components/wifi" "../components/sntp" "../usb_components/usb"
)
