#pragma once

#include "esp_err.h"
#include "stepper_28byj48.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAR_DRIVE_STOP = 0,
    CAR_DRIVE_FORWARD,
    CAR_DRIVE_BACKWARD,
    CAR_DRIVE_LEFT,
    CAR_DRIVE_RIGHT,
} car_drive_motion_t;

typedef struct {
    stepper_28byj48_config_t left;
    stepper_28byj48_config_t right;
    uint16_t cruise_sps;
    uint16_t turn_sps;
} car_drive_config_t;

esp_err_t car_drive_start(const car_drive_config_t *config);

void car_drive_set_motion(car_drive_motion_t motion);
car_drive_motion_t car_drive_get_motion(void);

const char *car_drive_motion_name(car_drive_motion_t motion);

float car_drive_get_odometry_cm(void);
void car_drive_reset_odometry(void);

#ifdef __cplusplus
}
#endif
