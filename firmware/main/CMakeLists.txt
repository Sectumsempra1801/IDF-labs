idf_component_register(SRCS "main.c" "ble.c" "wifi.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi 
                       PRIV_REQUIRES bt
                       )
