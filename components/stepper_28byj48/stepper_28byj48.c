#include "stepper_28byj48.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "stepper";

static const uint8_t k_half_step[8] = {0x01, 0x03, 0x02, 0x06,
                                       0x04, 0x0C, 0x08, 0x09};

struct stepper_28byj48_t {
    int gpio[4];
    bool invert;
    stepper_28byj48_direction_t motion;
    uint8_t phase;
    uint32_t interval_us;
    int64_t next_step_us;
    int32_t position;
};

static void write_phase(stepper_28byj48_handle_t h, uint8_t bits)
{
    for (int i = 0; i < 4; i++) {
        gpio_set_level(h->gpio[i], (bits >> i) & 0x01);
    }
}

esp_err_t stepper_28byj48_create(const stepper_28byj48_config_t *config,
                                 stepper_28byj48_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "null arg");

    esp_err_t ret = ESP_OK;
    stepper_28byj48_handle_t h = calloc(1, sizeof(struct stepper_28byj48_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "no mem");

    h->gpio[0] = config->in1_gpio;
    h->gpio[1] = config->in2_gpio;
    h->gpio[2] = config->in3_gpio;
    h->gpio[3] = config->in4_gpio;
    h->invert = config->invert;
    h->motion = STEPPER_28BYJ48_STOP;

    uint64_t mask = 0;
    for (int i = 0; i < 4; i++) {
        ESP_GOTO_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(h->gpio[i]), ESP_ERR_INVALID_ARG,
                          fail, TAG, "GPIO %d nije izlazni pin", h->gpio[i]);
        mask |= 1ULL << h->gpio[i];
    }

    gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&io), fail, TAG, "gpio_config");

    write_phase(h, 0);
    *out_handle = h;
    return ESP_OK;

fail:
    free(h);
    return ret;
}

void stepper_28byj48_delete(stepper_28byj48_handle_t handle)
{
    if (!handle) {
        return;
    }
    write_phase(handle, 0);
    free(handle);
}

void stepper_28byj48_set_motion(stepper_28byj48_handle_t handle,
                                stepper_28byj48_direction_t direction,
                                uint16_t half_steps_per_second)
{
    if (!handle) {
        return;
    }

    if (direction == STEPPER_28BYJ48_STOP || half_steps_per_second == 0) {
        handle->motion = STEPPER_28BYJ48_STOP;
        write_phase(handle, 0);
        return;
    }

    if (half_steps_per_second > STEPPER_28BYJ48_MAX_SPS) {
        half_steps_per_second = STEPPER_28BYJ48_MAX_SPS;
    }

    if (handle->invert) {
        direction = (direction == STEPPER_28BYJ48_CW) ? STEPPER_28BYJ48_CCW
                                                      : STEPPER_28BYJ48_CW;
    }

    handle->interval_us = 1000000UL / half_steps_per_second;

    handle->next_step_us = esp_timer_get_time();
    handle->motion = direction;
}

bool stepper_28byj48_service(stepper_28byj48_handle_t handle)
{
    if (!handle || handle->motion == STEPPER_28BYJ48_STOP) {
        return false;
    }

    int64_t now = esp_timer_get_time();
    if (now < handle->next_step_us) {
        return false;
    }

    int32_t sign = handle->invert ? -1 : 1;
    if (handle->motion == STEPPER_28BYJ48_CW) {
        handle->phase = (uint8_t)((handle->phase + 1) & 0x07);
        handle->position += sign;
    } else {
        handle->phase = (uint8_t)((handle->phase + 7) & 0x07);
        handle->position -= sign;
    }
    write_phase(handle, k_half_step[handle->phase]);

    handle->next_step_us += handle->interval_us;
    if (handle->next_step_us < now) {
        handle->next_step_us = now + handle->interval_us;
    }
    return true;
}

void stepper_28byj48_release(stepper_28byj48_handle_t handle)
{
    if (handle) {
        write_phase(handle, 0);
    }
}

int32_t stepper_28byj48_get_position(stepper_28byj48_handle_t handle)
{
    return handle ? handle->position : 0;
}
