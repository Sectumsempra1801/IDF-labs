idf_component_register(SRCS "IDF_labs.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_wifi 
                    REQUIRES bluetooth_manager wifi_manager
                    )
