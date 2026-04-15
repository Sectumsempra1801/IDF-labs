#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

static const char *TAG = "BLUEDROID_SERVER";

#define DEVICE_NAME "ESP32_BLUEDROID_LAB"

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertise data set is completed, starting advertising...");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "ERROR");
        }
        else
        {
            ESP_LOGI(TAG, "Advertising start operation success");
        }
        break;

    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {

    // Connected event
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "--------------------------------------------");
        ESP_LOGW(TAG, "CONNECTED!");
        ESP_LOGI(TAG, "MAC Adress of Client: %02x:%02x:%02x:%02x:%02x:%02x",
                 param->connect.remote_bda[0], param->connect.remote_bda[1],
                 param->connect.remote_bda[2], param->connect.remote_bda[3],
                 param->connect.remote_bda[4], param->connect.remote_bda[5]);
        ESP_LOGI(TAG, "Connection ID: %d", param->connect.conn_id);
        ESP_LOGI(TAG, "--------------------------------------------");
        break;

    // Disconnected event
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGW(TAG, "DISCONNECTED!");
        ESP_LOGI(TAG, "ISUS: 0x%x", param->disconnect.reason);
        ESP_LOGI(TAG, "Turning on Advertise again");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    default:
        break;
    }
}

void app_main(void)
{
    // Initialize NVS memory
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Initializing the Bluedroid Stack...");

    // Initialize and enable the Bluetooth Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE)); // Turn on mode BLE

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // GATT Callback event
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    esp_ble_gatts_app_register(0);
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));

    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
    while (1)
    {
        /* code */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}