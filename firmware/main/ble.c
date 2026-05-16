#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"

#include "ble.h"
#include "wifi.h"
#include "flash_manager.h"
#include "http_request.h"

#define GATTS_TAG "BLE"
#define GATTS_SERVICE_UUID 0x00FF
#define GATTS_CHAR_UUID 0xFF01
#define GATTS_NUM_HANDLE 4
#define PROFILE_APP_ID 0

static uint16_t g_gatts_if = ESP_GATT_IF_NONE;

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

static bool extract_value(const char *data, const char *key, char *out, size_t max_len)
{
    char *start = strstr(data, key);
    if (!start)
        return false;
    start += strlen(key);
    char *end = strchr(start, '"');
    if (!end)
        return false;

    size_t len = end - start;
    if (len >= max_len)
        len = max_len - 1;

    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
        esp_ble_gap_set_device_name("ESP_DEVICE");
        esp_ble_gap_config_adv_data(&adv_data);

        esp_gatt_srvc_id_t service_id;
        service_id.is_primary = true;
        service_id.id.inst_id = 0x00;
        service_id.id.uuid.len = ESP_UUID_LEN_16;
        service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;
        esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE);
        break;
    }

    case ESP_GATTS_CREATE_EVT:
    {
        esp_ble_gatts_start_service(param->create.service_handle);
        esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;

        esp_bt_uuid_t char_uuid;
        char_uuid.len = ESP_UUID_LEN_16;
        char_uuid.uuid.uuid16 = GATTS_CHAR_UUID;

        esp_ble_gatts_add_char(param->create.service_handle, &char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               property, NULL, NULL);
        break;
    }

    case ESP_GATTS_WRITE_EVT:
    {
        if (!param->write.is_prep)
        {
            char data[256] = {0};
            size_t len = param->write.len < sizeof(data) - 1 ? param->write.len : sizeof(data) - 1;
            memcpy(data, param->write.value, len);

            ESP_LOGI(GATTS_TAG, "Received BLE Data: %s", data);

            char temp_val[256];

            if (extract_value(data, "url=\"", temp_val, sizeof(temp_val)))
            {
                strncpy(g_url, temp_val, sizeof(g_url));
                ESP_LOGI(GATTS_TAG, "-> Saved URL: %s", g_url);
            }
            else if (extract_value(data, "method=\"", temp_val, sizeof(temp_val)))
            {
                if (strcmp(temp_val, "GET") == 0 && strlen(g_url) > 0)
                {
                    http_cmd_t cmd;
                    strncpy(cmd.url, g_url, sizeof(cmd.url));
                    strncpy(cmd.method, "GET", sizeof(cmd.method));

                    if (http_queue != NULL)
                    {
                        xQueueSend(http_queue, &cmd, 0);
                    }
                    else
                    {
                        ESP_LOGW(GATTS_TAG, "HTTP Queue is not initialized!");
                    }
                }
            }
            else if (extract_value(data, "ssid=\"", temp_val, sizeof(temp_val)))
            {
                strncpy(g_ssid, temp_val, sizeof(g_ssid));
                ESP_LOGI(GATTS_TAG, "-> Saved SSID: %s", g_ssid);
            }
            else if (extract_value(data, "password=\"", temp_val, sizeof(temp_val)))
            {
                strncpy(g_password, temp_val, sizeof(g_password));
                ESP_LOGI(GATTS_TAG, "-> Saved PASS: %s", g_password);
                save_wifi_credentials(g_ssid, g_password);
            }
            else if (extract_value(data, "ip=\"", temp_val, sizeof(temp_val)))
            {
                strncpy(g_server_ip, temp_val, sizeof(g_server_ip));
                ESP_LOGI(GATTS_TAG, "-> Saved IP: %s", g_server_ip);
            }

            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        break;
    }

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client connected!");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client disconnected. Restarting advertising...");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            g_gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGE(GATTS_TAG, "GATT Register failed, status %d", param->reg.status);
            return;
        }
    }
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == g_gatts_if)
    {
        gatts_profile_event_handler(event, gatts_if, param);
    }
}

esp_err_t ble_controller_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
        return ret;
    return esp_bt_controller_enable(ESP_BT_MODE_BLE);
}

esp_err_t ble_stack_init(void)
{
    esp_err_t ret = esp_bluedroid_init();
    if (ret)
        return ret;
    return esp_bluedroid_enable();
}

esp_err_t ble_callbacks_register(void)
{
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    return ESP_OK;
}

esp_err_t ble_gatts_app_init(void)
{
    esp_ble_gatts_app_register(PROFILE_APP_ID);
    esp_ble_gatt_set_local_mtu(247);
    return ESP_OK;
}

void ble_enable(void)
{
    ble_controller_init();
    ble_stack_init();
    ble_callbacks_register();
    ble_gatts_app_init();
}