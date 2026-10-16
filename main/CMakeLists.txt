idf_component_register(SRCS "web.c" "oled.c" "esp32-guitar-effect.c" "wifi.c" "mqtt.c" "stm32_uart.c"
                    INCLUDE_DIRS "." "include"
                    EMBED_FILES "web/index.html")
