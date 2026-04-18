#include "bluetooth_manager.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BLE_MANAGER";
#define PROFILE_APP_ID 0
#define DEVICE_NAME "ESP32_IDF"

static ble_wifi_cred_cb_t wifi_callback = NULL;

static uint8_t raw_adv_data[] = {0x02, 0x01, 0x06, 0x0A, 0x09, 'E', 'S', 'P', '3', '2', '_', 'B', 'L', 'E'};
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static const uint16_t GATTS_SERVICE_UUID_TEST = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_TEST = 0xFF01;
uint16_t gatts_if_handle;
uint16_t service_handle;
esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;

void ble_manager_set_callback(ble_wifi_cred_cb_t cb)
{
    wifi_callback = cb;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT)
    {
        esp_ble_gap_start_advertising(&adv_params);
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "-------------------");
        ESP_LOGI(TAG, "GUI CONNECTED!");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "-------------------");
        ESP_LOGI(TAG, "GUI DISCONNECTED!");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_REG_EVT:
        esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        gatts_if_handle = gatts_if;
        esp_gatt_srvc_id_t srvc_id = {.is_primary = true, .id.inst_id = 0, .id.uuid.len = ESP_UUID_LEN_16, .id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST};
        esp_ble_gatts_create_service(gatts_if, &srvc_id, 4);
        break;

    case ESP_GATTS_CREATE_EVT:
        service_handle = param->create.service_handle;
        esp_bt_uuid_t char_uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_TEST};
        esp_ble_gatts_add_char(service_handle, &char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, property, NULL, NULL);
        esp_ble_gatts_start_service(service_handle);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.need_rsp)
        {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        }
        if (!param->write.is_prep && param->write.len > 0)
        {
            uint8_t *data = param->write.value;
            if (data[0] == 0xAA && param->write.len > 4 && data[3] == 0xA8)
            {
                int string_len = param->write.len - 5;
                char wifi_info[string_len + 1];
                memcpy(wifi_info, &data[4], string_len);
                wifi_info[string_len] = '\0';

                char *ssid = wifi_info;
                char *password = "";
                char *separator = strchr(wifi_info, '\x1F');
                if (separator != NULL)
                {
                    *separator = '\0';
                    password = separator + 1;
                }

                if (strlen(ssid) > 0)
                {
                    if (wifi_callback != NULL)
                    {
                        wifi_callback(ssid, password);
                    }
                }
                // ==========================================
            }
        }
        break;
    default:
        break;
    }
}

void ble_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE Manager...");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gatt_set_local_mtu(500);

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);

    esp_ble_gatts_app_register(PROFILE_APP_ID);
    esp_ble_gap_set_device_name(DEVICE_NAME);
}