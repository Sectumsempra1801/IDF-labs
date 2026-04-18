idf_component_register(SRCS "bluetooth_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver bt nvs_flash
                    )
