# idf_component_register(SRCS   "main.c" "ble.c" "wifi.c" 
#                               "socket_app.c" "led_c.c" "flash_manager.c" "http_request.c"          
#                               "source/core_http_client.c"
#                               "source/dependency/3rdparty/llhttp/src/api.c"
#                               "source/dependency/3rdparty/llhttp/src/http.c"
#                               "source/dependency/3rdparty/llhttp/src/llhttp.c"     
#                        INCLUDE_DIRS "."
#                                         "source/include"
#                                         "source/dependency/3rdparty/llhttp/include"
#                        REQUIRES nvs_flash esp_wifi esp_driver_ledc esp_driver_gpio esp_http_client coreHTTP lwip
#                        PRIV_REQUIRES bt 
#                        )
file(GLOB srcs "*.c")
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_driver_ledc esp_driver_gpio esp_http_client coreHTTP lwip
                       PRIV_REQUIRES bt)