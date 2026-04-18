#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "bluetooth_manager.h"
#include "wifi_manager.h"

static const char *TAG = "APP_MAIN";

char saved_ssid[64] = {0};
char saved_pw[64] = {0};

void on_wifi_received_from_ble(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "==== MAIN RECEIVED DATA FROM BLE ====");
    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "Pass: %s", password);

    wifi_manager_save_creds(ssid, password);
    wifi_manager_read_creds(saved_ssid, saved_pw, sizeof(saved_ssid));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_manager_init();

    wifi_manager_read_creds(saved_ssid, saved_pw, sizeof(saved_ssid));

    if (strlen(saved_ssid) > 0)
    {
        ESP_LOGI(TAG, "Found saved WiFi, auto connecting...");
        wifi_manager_connect(saved_ssid, saved_pw);
    }

    ble_manager_set_callback(on_wifi_received_from_ble);
    ble_manager_init();

    while (1)
    {
        ESP_LOGI(TAG, "SSID :%s", saved_ssid);
        ESP_LOGI(TAG, "PW   :%s", saved_pw);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}