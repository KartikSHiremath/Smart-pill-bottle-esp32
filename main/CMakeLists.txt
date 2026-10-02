idf_component_register(SRCS "ble_service.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    bt
                    nvs_flash
                    esp_timer
                    esp_driver_gpio
                    esp_event
                     )
