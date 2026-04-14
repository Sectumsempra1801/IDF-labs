#include <stdio.h>
#include "led_c.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#define LED_PIN 5
static const char *TAG = "SIZE";
void app_main(void)
{
    struct led_c led_door;
    // struct led_c *prt = &led_door;
    led_c_init(LED_PIN, &led_door);
    ESP_LOGI(TAG, "Kích thước của TaskHandle_t: %zu byte", sizeof(TaskHandle_t));
    ESP_LOGI(TAG, "Kích thước của ledc_channel_t: %zu byte", sizeof(ledc_channel_t));
    ESP_LOGI(TAG, "Kích thước của ledc_timer_t: %zu byte", sizeof(ledc_timer_t));
    ESP_LOGI(TAG, "Kích thước của led_c_t: %zu byte", sizeof(led_c_t));
    led_c_blink(5, &led_door);
    vTaskDelay(pdMS_TO_TICKS(10));
    while (1)
    {
    };
}
