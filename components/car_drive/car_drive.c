#include "car_drive.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "car_drive";

#define WHEEL_DIAMETER_CM 6.5f
#define PI_F 3.14159265f

static stepper_28byj48_handle_t s_left;
static stepper_28byj48_handle_t s_right;
static uint16_t s_cruise_sps;
static uint16_t s_turn_sps;
static volatile car_drive_motion_t s_requested = CAR_DRIVE_STOP;
static car_drive_motion_t s_applied = CAR_DRIVE_STOP;
static int32_t s_left_zero;
static int32_t s_right_zero;

static void apply_motion(car_drive_motion_t motion)
{
    stepper_28byj48_direction_t left = STEPPER_28BYJ48_STOP;
    stepper_28byj48_direction_t right = STEPPER_28BYJ48_STOP;
    uint16_t sps = s_cruise_sps;

    switch (motion) {
    case CAR_DRIVE_FORWARD:
        left = STEPPER_28BYJ48_CW;
        right = STEPPER_28BYJ48_CW;
        break;
    case CAR_DRIVE_BACKWARD:
        left = STEPPER_28BYJ48_CCW;
        right = STEPPER_28BYJ48_CCW;
        break;
    case CAR_DRIVE_LEFT:
        left = STEPPER_28BYJ48_CCW;
        right = STEPPER_28BYJ48_CW;
        sps = s_turn_sps;
        break;
    case CAR_DRIVE_RIGHT:
        left = STEPPER_28BYJ48_CW;
        right = STEPPER_28BYJ48_CCW;
        sps = s_turn_sps;
        break;
    case CAR_DRIVE_STOP:
    default:
        break;
    }

    stepper_28byj48_set_motion(s_left, left, sps);
    stepper_28byj48_set_motion(s_right, right, sps);
}

static void drive_task(void *arg)
{
    (void)arg;
    while (true) {
        car_drive_motion_t wanted = s_requested;
        if (wanted != s_applied) {
            apply_motion(wanted);
            s_applied = wanted;
        }
        stepper_28byj48_service(s_left);
        stepper_28byj48_service(s_right);

        vTaskDelay(1);
    }
}

esp_err_t car_drive_start(const car_drive_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "null config");
    ESP_RETURN_ON_FALSE(!s_left, ESP_ERR_INVALID_STATE, TAG, "vec pokrenuto");

    s_cruise_sps = config->cruise_sps ? config->cruise_sps : 700;
    s_turn_sps = config->turn_sps ? config->turn_sps : 500;

    ESP_RETURN_ON_ERROR(stepper_28byj48_create(&config->left, &s_left), TAG, "lijevi motor");
    esp_err_t err = stepper_28byj48_create(&config->right, &s_right);
    if (err != ESP_OK) {
        stepper_28byj48_delete(s_left);
        s_left = NULL;
        ESP_RETURN_ON_ERROR(err, TAG, "desni motor");
    }

    BaseType_t ok = xTaskCreatePinnedToCore(drive_task, "car_drive", 3072, NULL, 6, NULL, 1);
    if (ok != pdPASS) {
        stepper_28byj48_delete(s_left);
        stepper_28byj48_delete(s_right);
        s_left = s_right = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "pogon spreman: voznja %u pk/s, okret %u pk/s", s_cruise_sps, s_turn_sps);
    return ESP_OK;
}

void car_drive_set_motion(car_drive_motion_t motion)
{
    s_requested = motion;
}

car_drive_motion_t car_drive_get_motion(void)
{
    return s_requested;
}

const char *car_drive_motion_name(car_drive_motion_t motion)
{
    switch (motion) {
    case CAR_DRIVE_FORWARD:  return "NAPRIJED";
    case CAR_DRIVE_BACKWARD: return "NATRAG";
    case CAR_DRIVE_LEFT:     return "LIJEVO";
    case CAR_DRIVE_RIGHT:    return "DESNO";
    default:                 return "STOP";
    }
}

float car_drive_get_odometry_cm(void)
{
    if (!s_left || !s_right) {
        return 0.0f;
    }

    float half_steps = 0.5f * ((float)(stepper_28byj48_get_position(s_left) - s_left_zero) +
                               (float)(stepper_28byj48_get_position(s_right) - s_right_zero));
    float revolutions = half_steps / (float)STEPPER_28BYJ48_HALF_STEPS_PER_REV;
    return revolutions * PI_F * WHEEL_DIAMETER_CM;
}

void car_drive_reset_odometry(void)
{
    if (s_left && s_right) {
        s_left_zero = stepper_28byj48_get_position(s_left);
        s_right_zero = stepper_28byj48_get_position(s_right);
    }
}
