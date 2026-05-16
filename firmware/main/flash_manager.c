#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "flash_manager.h"

static const char *TAG = "FLASH_MGR";

bool got_wifi_credentials = false;

char g_ssid[32] = {0};

char g_password[64] = {0};

char g_server_ip[32] = "192.168.90.8";

void nvs_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS Flash initialized successfully");
}

esp_err_t nvs_read_string(const char *namespace_name, const char *key, char *out_value, size_t max_len)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_str(my_handle, key, out_value, &max_len);
    nvs_close(my_handle);
    return err;
}

esp_err_t nvs_write_string(const char *namespace_name, const char *key, const char *value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(my_handle, key, value);
    if (err == ESP_OK)
    {
        err = nvs_commit(my_handle);
    }
    nvs_close(my_handle);
    return err;
}

esp_err_t read_wifi_credentials(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open("wifi_data", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "WiFi credentials might not be saved yet");
        return err;
    }

    // read SSID
    size_t ssid_len = sizeof(g_ssid);
    err = nvs_get_str(my_handle, "ssid", g_ssid, &ssid_len);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "  'ssid'   not found in NVS");
    }

    // read Password
    size_t pass_len = sizeof(g_password);
    err = nvs_get_str(my_handle, "password", g_password, &pass_len);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "'password' not found in NVS");
    }

    nvs_close(my_handle);

    if (err == ESP_OK)
    {
        got_wifi_credentials = true;
    }

    return err;
}

esp_err_t save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open("wifi_data", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    // write SSID
    err = nvs_set_str(my_handle, "ssid", ssid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error occured while saving ssid(%s)", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    // write password
    err = nvs_set_str(my_handle, "password", password);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error occured while saving password(%s)", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    // commit data
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error occured while committing NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Data has been saved");
    }
    nvs_close(my_handle);

    return err;
}