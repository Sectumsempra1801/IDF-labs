#include "led_c.h"
#define LEDC_MAX_DUTY ((1 << 13) - 1) // 8191
// blink task
static const char *TAG = "LED_C";
static void _blink_task(void *arg)
{
    led_c_t *ledc = (led_c_t *)arg;
    while (1)
    {
        gpio_set_level(ledc->gpio_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(ledc->time_blink_on_ms));
        gpio_set_level(ledc->gpio_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(ledc->time_blink_off_ms));
    }
}
//
// PWM init
static void _pwm_init(led_c_t *ledc)

{
    if (ledc->pwm_initialized)
        return;

    // PWM init

    // init timer to use pwm
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 4000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = ledc->timer,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    // config channal in use
    ledc_channel_config_t ledc_channel = {
        .channel = ledc->channel,
        .duty = 0,
        .gpio_num = ledc->gpio_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = ledc->timer,
        .flags.output_invert = 0};
    ledc_channel_config(&ledc_channel);

    ledc->pwm_initialized = 1;
}
// static adc_oneshot_unit_handle_t adc1_handle = NULL;

void led_c_init(uint8_t pin, uint8_t channel, uint8_t timer, led_c_t *ledc)
{
    ledc->blink_task = NULL;
    ledc->time_blink_on_ms = 500;
    ledc->time_blink_off_ms = 500;

    if (channel < LEDC_CHANNEL_MAX)
    {
        ledc->channel = channel;
    }
    else
    {
        ledc->channel = LEDC_CHANNEL_0;
    }

    if (timer < LEDC_TIMER_MAX)
    {
        ledc->timer = timer;
    }
    else
    {
        ledc->timer = LEDC_TIMER_0;
    }

    ledc->gpio_pin = pin;
    ledc->current_mode = 0;
    ledc->frequency = 0;

    ledc->pwm_initialized = 0;
    gpio_reset_pin(ledc->gpio_pin);
    gpio_set_direction(ledc->gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(ledc->gpio_pin, 0);
};

void led_c_off(led_c_t *ledc)
{
    // gpio_set_direction(ledc->gpio_pin, GPIO_MODE_OUTPUT);

    if (ledc->current_mode == 2)
    {
        led_c_blink_stop(ledc);
    }
    if (ledc->current_mode == 3)
    {
        ledc_stop(LEDC_LOW_SPEED_MODE, ledc->channel, 0);
    }

    gpio_set_direction(ledc->gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(ledc->gpio_pin, 0);
    ledc->current_mode = 0;
};

void led_c_on(led_c_t *ledc)
{
    if (ledc->current_mode == 2)
        led_c_blink_stop(ledc);
    if (ledc->current_mode == 3)
        ledc_stop(LEDC_LOW_SPEED_MODE, ledc->channel, 0);

    gpio_set_direction(ledc->gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(ledc->gpio_pin, 1);
    ledc->current_mode = 1;
};

void led_c_blink(uint8_t hz, led_c_t *ledc)
{
    ledc->frequency = hz;
    if (ledc->frequency == 0)
    {
        ESP_LOGE(TAG, "Invalid frequency");
        return;
    }
    else
    {
        ledc->time_blink_off_ms = 1000 / 2 / ledc->frequency;
        ledc->time_blink_on_ms = 1000 / 2 / ledc->frequency;
        if (ledc->blink_task)
        {
            ledc->current_mode = 2;
            return;
        }
        gpio_set_direction(ledc->gpio_pin, GPIO_MODE_OUTPUT);
        xTaskCreate(_blink_task, "led_blink", 1024,
                    ledc, 5, &ledc->blink_task);
        ledc->current_mode = 2;
    }
};

void led_c_blink_stop(led_c_t *ledc)
{
    if (ledc->blink_task)
    {
        vTaskDelete(ledc->blink_task);
        ledc->blink_task = NULL;
        gpio_set_level(ledc->gpio_pin, 0);
    }
};

void led_c_dim(uint8_t percentage, led_c_t *ledc)
{
    if (percentage > 100)
    {
        percentage = 100;
    }

    led_c_blink_stop(ledc);
    _pwm_init(ledc);

    // ADC 13bit => (2^13 - 1) / 100 = 8191
    uint32_t duty = ((uint32_t)percentage * 8191) / 100;

    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, ledc->channel, duty, 0);
    ledc->current_mode = 3;
};

// void led_c_dim_adc(adc_oneshot_unit_handle_t adc_handle, adc_channel_t channel, led_c_t *ledc)
// {
//     int adc_raw = 0;
//     uint8_t percentage = (uint8_t)((adc_raw * 100) / 4095);

//     led_c_dim(percentage, ledc);
// }

uint8_t led_c_current_mode(led_c_t *ledc)
{
    return ledc->current_mode;
};
