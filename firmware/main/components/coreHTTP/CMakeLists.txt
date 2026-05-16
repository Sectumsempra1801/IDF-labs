file(GLOB_RECURSE SOURCES "source/*.c")

idf_component_register(SRCS "${SOURCES}"
                    INCLUDE_DIRS "source/include" 
                                 "source/dependency/coreHTTP-Client/interface"
                    REQUIRES esp_tls)