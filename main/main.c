#include "car_drive.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hcsr04.h"
#include "nvs_flash.h"
#include "web_control.h"

#define LEFT_IN1_GPIO   32
#define LEFT_IN2_GPIO   33
#define LEFT_IN3_GPIO   25
#define LEFT_IN4_GPIO   26

#define RIGHT_IN1_GPIO  27
#define RIGHT_IN2_GPIO  14
#define RIGHT_IN3_GPIO  12
#define RIGHT_IN4_GPIO  13

#define HCSR04_TRIG_GPIO 17
#define HCSR04_ECHO_GPIO 16

#define CRUISE_SPS      700
#define TURN_SPS        500
#define OBSTACLE_CM     20.0f
#define OBSTACLE_CLEAR_CM 25.0f

#define DISTANCE_PERIOD_US 250000
#define CONTROL_PERIOD_MS  20

#define WIFI_SSID     "ESP32-AUTIC"
#define WIFI_PASSWORD "autic1234"

static const char *TAG = "autic";

static volatile float s_distance_cm = -1.0f;
static volatile bool s_obstacle;
static volatile car_drive_motion_t s_user_request = CAR_DRIVE_STOP;

static void on_command(char command)
{
    switch (command) {
    case 'F': s_user_request = CAR_DRIVE_FORWARD;  break;
    case 'B': s_user_request = CAR_DRIVE_BACKWARD; break;
    case 'L': s_user_request = CAR_DRIVE_LEFT;     break;
    case 'R': s_user_request = CAR_DRIVE_RIGHT;    break;
    case 'Z':
        car_drive_reset_odometry();
        break;
    case 'S':
    default:
        s_user_request = CAR_DRIVE_STOP;
        break;
    }
}

static void on_status(web_control_status_t *out)
{
    out->distance_cm = s_distance_cm;
    out->obstacle = s_obstacle;
    out->motion = car_drive_motion_name(car_drive_get_motion());
    out->odometry_cm = car_drive_get_odometry_cm();
}

static void distance_task(void *arg)
{
    (void)arg;
    while (true) {
        float cm;
        if (hcsr04_measure_median_cm(&cm) == ESP_OK) {
            s_distance_cm = cm;
            if (cm < OBSTACLE_CM) {
                s_obstacle = true;
            } else if (cm > OBSTACLE_CLEAR_CM) {
                s_obstacle = false;
            }
        } else {
            s_distance_cm = -1.0f;
            s_obstacle = false;
        }
        vTaskDelay(pdMS_TO_TICKS(DISTANCE_PERIOD_US / 1000));
    }
}

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    ESP_ERROR_CHECK(hcsr04_init(&(hcsr04_config_t){
        .trig_gpio = HCSR04_TRIG_GPIO,
        .echo_gpio = HCSR04_ECHO_GPIO,
    }));

    ESP_ERROR_CHECK(car_drive_start(&(car_drive_config_t){
        .left = {
            .in1_gpio = LEFT_IN1_GPIO,
            .in2_gpio = LEFT_IN2_GPIO,
            .in3_gpio = LEFT_IN3_GPIO,
            .in4_gpio = LEFT_IN4_GPIO,

            .invert = true,
        },
        .right = {
            .in1_gpio = RIGHT_IN1_GPIO,
            .in2_gpio = RIGHT_IN2_GPIO,
            .in3_gpio = RIGHT_IN3_GPIO,
            .in4_gpio = RIGHT_IN4_GPIO,

            .invert = false,
        },
        .cruise_sps = CRUISE_SPS,
        .turn_sps = TURN_SPS,
    }));

    ESP_ERROR_CHECK(web_control_start(WIFI_SSID, WIFI_PASSWORD, on_command, on_status));

    xTaskCreate(distance_task, "distance", 3072, NULL, 4, NULL);

    ESP_LOGI(TAG, "spreman: Wi-Fi %s, lozinka %s, http://192.168.4.1", WIFI_SSID, WIFI_PASSWORD);

    bool obstacle_logged = false;
    while (true) {
        car_drive_motion_t wanted = s_user_request;
        if (s_obstacle && wanted == CAR_DRIVE_BACKWARD) {
            wanted = CAR_DRIVE_STOP;
            if (!obstacle_logged) {
                ESP_LOGW(TAG, "prepreka iza na %.1f cm - blokiram voznju natrag", (double)s_distance_cm);
                obstacle_logged = true;
            }
        } else if (!s_obstacle) {
            obstacle_logged = false;
        }
        car_drive_set_motion(wanted);

        vTaskDelay(pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}
