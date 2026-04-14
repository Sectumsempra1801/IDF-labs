#include <stdio.h>
#include "led_c.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#define LED_PIN_1 5
#define LED_PIN_2 6

static const char *TAG = "SIZE";

void app_main(void)
{
    struct led_c led_door;
    struct led_c led_win;

    led_c_init(LED_PIN_1, 0, 0, &led_door);
    led_c_init(LED_PIN_2, 1, 1, &led_win);
    led_c_blink(2, &led_door);
    led_c_dim(1, &led_win);
    //  --- ADC init ---  //
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    //  --- ADC config ---  //
    adc_oneshot_chan_cfg_t aoc_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &aoc_cfg));
    while (1)
    {
        int adc_raw = 0;
        esp_err_t err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw);
        if (err == ESP_OK)
        {
            uint8_t percentage = (uint8_t)((adc_raw * 100) / 4095);

            ESP_LOGI(TAG, "Gia tri Raw: %d \t| Phan tram: %d%%", adc_raw, percentage);
            led_c_dim(percentage, &led_win);
        }
        else
        {
            ESP_LOGE(TAG, "Loi doc ADC! Ma loi: %d", err);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    };
}
