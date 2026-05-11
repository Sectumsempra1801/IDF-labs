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
#include "flash_manager.h"

#define LED_1 4
#define LED_2 5

static const char *TAG = "Main";
esp_err_t err;
led_c_t LED1 = {0}; // GPIO 4
led_c_t LED2 = {0}; // GPIO 5
void app_main(void)
{
    led_c_init(LED_1, 0, 0, &LED1);
    led_c_init(LED_2, 1, 1, &LED2);
    // Initialize NVS
    nvs_manager_init();
    // Initialize BLE
    ble_enable();
    // Initialize WiFi
    wifi_init_sta();
    // Read SSID and Password in flash
    err = read_wifi_credentials();
    // init socket sys
    socket_system_init();
    while (1)
    {
        if ((got_wifi_credentials) && !(wifi_connected))
        {
            wifi_connect();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
