#ifndef LED_C
#define LED_C
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "esp_adc/adc_oneshot.h"
// typedef enum
// {
//     LED_MODE_OFF,
//     LED_MODE_ON,
//     LED_MODE_BLINK,
//     LED_MODE_PWM
// } led_mode_t;
typedef struct led_c
{
    // 4 byte
    TaskHandle_t blink_task;
    uint32_t time_blink_on_ms;
    uint32_t time_blink_off_ms;
    ledc_channel_t pwm_channel;
    ledc_timer_t pwm_timer;

    // 1 byte
    uint8_t gpio_pin;
    uint8_t channel;
    uint8_t timer;
    uint8_t current_mode;
    // 0 . led mode OFF
    // 1 . led mode ON
    // 2 . led mode BLINK
    // 3 . led mode DIM
    uint8_t frequency;
    // 1 bit
    bool pwm_initialized;
} led_c_t; // 20byte

void led_c_init(uint8_t pin, uint8_t channel, uint8_t timer, led_c_t *ledc);

void led_c_off(led_c_t *ledc);

void led_c_on(led_c_t *ledc);

void led_c_blink(uint8_t hz, led_c_t *ledc);

void led_c_blink_stop(led_c_t *ledc);

void led_c_dim(uint8_t percentage, led_c_t *ledc);

// void led_c_dim_adc(adc_oneshot_unit_handle_t adc_handle, adc_channel_t channel, led_c_t *ledc);

uint8_t led_c_current_mode(led_c_t *ledc);

#endif