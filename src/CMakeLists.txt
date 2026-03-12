idf_component_register(SRCS "main.c" "radio_hal.c" "esp3_proto.c" "enocean_security.c" "enocean_nvs.c" "TCMSerial.c"
                    INCLUDE_DIRS "." "../include"
                    REQUIRES mbedtls nvs_flash)
