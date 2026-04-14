idf_component_register(SRCS "led_c.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_driver_ledc esp_driver_gpio
                    )
