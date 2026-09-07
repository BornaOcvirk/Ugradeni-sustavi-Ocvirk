idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES hcsr04 car_drive web_control nvs_flash)
