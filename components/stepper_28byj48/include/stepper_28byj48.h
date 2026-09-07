#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STEPPER_28BYJ48_HALF_STEPS_PER_REV 4096u

#define STEPPER_28BYJ48_MAX_SPS 1000u

typedef enum {
    STEPPER_28BYJ48_STOP = 0,
    STEPPER_28BYJ48_CW,
    STEPPER_28BYJ48_CCW,
} stepper_28byj48_direction_t;

typedef struct stepper_28byj48_t *stepper_28byj48_handle_t;

typedef struct {
    int in1_gpio;
    int in2_gpio;
    int in3_gpio;
    int in4_gpio;

    bool invert;
} stepper_28byj48_config_t;

esp_err_t stepper_28byj48_create(const stepper_28byj48_config_t *config,
                                 stepper_28byj48_handle_t *out_handle);
void stepper_28byj48_delete(stepper_28byj48_handle_t handle);

void stepper_28byj48_set_motion(stepper_28byj48_handle_t handle,
                                stepper_28byj48_direction_t direction,
                                uint16_t half_steps_per_second);

bool stepper_28byj48_service(stepper_28byj48_handle_t handle);

void stepper_28byj48_release(stepper_28byj48_handle_t handle);

int32_t stepper_28byj48_get_position(stepper_28byj48_handle_t handle);

#ifdef __cplusplus
}
#endif
