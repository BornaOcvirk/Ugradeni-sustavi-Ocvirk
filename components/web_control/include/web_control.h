#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*web_control_command_cb_t)(char command);

typedef struct {
    float distance_cm;
    float odometry_cm;
    bool obstacle;
    const char *motion;
} web_control_status_t;

typedef void (*web_control_status_cb_t)(web_control_status_t *out);

esp_err_t web_control_start(const char *ssid, const char *password,
                            web_control_command_cb_t command_cb,
                            web_control_status_cb_t status_cb);

#ifdef __cplusplus
}
#endif
