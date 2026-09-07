#include "hcsr04.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hcsr04";

#define ECHO_TIMEOUT_US 30000

#define US_PER_CM 58.0f

static hcsr04_config_t s_pins;
static bool s_initialized;

static int64_t wait_for_level(int level, int64_t timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_pins.echo_gpio) != level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return -1;
        }
    }
    return esp_timer_get_time();
}

esp_err_t hcsr04_init(const hcsr04_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "null config");
    s_pins = *config;

    gpio_config_t trig = {
        .pin_bit_mask = 1ULL << s_pins.trig_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config_t echo = {
        .pin_bit_mask = 1ULL << s_pins.echo_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,

        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&trig), TAG, "TRIG");
    ESP_RETURN_ON_ERROR(gpio_config(&echo), TAG, "ECHO");

    gpio_set_level(s_pins.trig_gpio, 0);
    s_initialized = true;
    return ESP_OK;
}

esp_err_t hcsr04_measure_cm(float *distance_cm)
{
    ESP_RETURN_ON_FALSE(s_initialized && distance_cm, ESP_ERR_INVALID_STATE, TAG, "nije init");

    gpio_set_level(s_pins.trig_gpio, 0);
    esp_rom_delay_us(3);
    gpio_set_level(s_pins.trig_gpio, 1);
    esp_rom_delay_us(10);
    gpio_set_level(s_pins.trig_gpio, 0);

    int64_t rise = wait_for_level(1, ECHO_TIMEOUT_US);
    if (rise < 0) {
        return ESP_ERR_TIMEOUT;
    }
    int64_t fall = wait_for_level(0, ECHO_TIMEOUT_US);
    if (fall < 0) {
        return ESP_ERR_TIMEOUT;
    }

    float cm = (float)(fall - rise) / US_PER_CM;
    if (cm < HCSR04_MIN_CM || cm > HCSR04_MAX_CM) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *distance_cm = cm;
    return ESP_OK;
}

static void sort3(float *a, float *b, float *c)
{
    float t;
    if (*a > *b) { t = *a; *a = *b; *b = t; }
    if (*b > *c) { t = *b; *b = *c; *c = t; }
    if (*a > *b) { t = *a; *a = *b; *b = t; }
}

esp_err_t hcsr04_measure_median_cm(float *distance_cm)
{
    ESP_RETURN_ON_FALSE(distance_cm, ESP_ERR_INVALID_ARG, TAG, "null out");

    float samples[3];
    int taken = 0;
    for (int i = 0; i < 3; i++) {
        if (hcsr04_measure_cm(&samples[taken]) == ESP_OK) {
            taken++;
        }

        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (taken == 0) {
        return ESP_ERR_TIMEOUT;
    }
    if (taken == 3) {
        sort3(&samples[0], &samples[1], &samples[2]);
        *distance_cm = samples[1];
    } else {
        *distance_cm = samples[0];
    }
    return ESP_OK;
}
