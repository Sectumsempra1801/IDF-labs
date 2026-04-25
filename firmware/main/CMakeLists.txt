idf_component_register(SRCS "main.c" "ble.c" "wifi.c" "socket_app.c" "led_c.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_driver_ledc esp_driver_gpio
                       PRIV_REQUIRES bt 
                       )
