#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HCSR04_MIN_CM 2.0f
#define HCSR04_MAX_CM 400.0f

typedef struct {
    int trig_gpio;
    int echo_gpio;
} hcsr04_config_t;

esp_err_t hcsr04_init(const hcsr04_config_t *config);

esp_err_t hcsr04_measure_cm(float *distance_cm);

esp_err_t hcsr04_measure_median_cm(float *distance_cm);

#ifdef __cplusplus
}
#endif
