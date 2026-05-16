set(core_mqtt_srcs
    "source/core_mqtt.c"
    "source/core_mqtt_serializer.c"
    "source/core_mqtt_state.c"
)

idf_component_register(SRCS "${core_mqtt_srcs}"
                       INCLUDE_DIRS "source/include" "source/interface" "."
                       REQUIRES lwip main)