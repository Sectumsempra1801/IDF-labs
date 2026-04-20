#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "ble.h"
#include "wifi.h"
#include "socket_app.h"
#include "led_c.h"

#define MAIN_TAG "main"

#define LED_1 4
#define LED_2 5

led_c_t LED1;
led_c_t LED2;

static esp_err_t wifi_enable(void)
{
    while (!got_wifi_credentials)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    wifi_init_sta();

    return ESP_OK;
}

static esp_err_t ble_enable(void)
{
    esp_err_t ret;

    ret = ble_controller_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "ble_controller_init failed");
        return ret;
    }

    ret = ble_stack_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "ble_stack_init failed");
        return ret;
    }

    ret = ble_callbacks_register();
    if (ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "ble_callbacks_register failed");
        return ret;
    }

    ret = ble_gatts_app_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "ble_gatts_app_init failed");
        return ret;
    }

    return ESP_OK;
}

void app_main(void)
{
    led_c_init(LED_1, 0, 0, &LED1);
    led_c_init(LED_2, 1, 1, &LED2);

    esp_err_t ret;
    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE
    ret = ble_enable();
    if (ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to initialize BLE");
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ret = wifi_enable();
    if (ret != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to initialize WiFi");
    }
    ESP_ERROR_CHECK(ret);

    return;

    // while (1)
    // {
    // }
}
