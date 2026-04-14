idf_component_register(SRCS "IDF_labs.c"
                    INCLUDE_DIRS "."
                    REQUIRES led_c 
                    PRIV_REQUIRES esp_adc esp_driver_ledc
                    )
