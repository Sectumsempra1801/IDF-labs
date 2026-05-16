#ifndef FLASH_MANAGER_H
#define FLASH_MANAGER_H

#include <stddef.h>
#include "esp_err.h"

extern bool got_wifi_credentials;

extern char g_ssid[32];

extern char g_password[64];

extern char g_server_ip[32];

void nvs_manager_init(void);

esp_err_t nvs_read_string(const char *namespace_name, const char *key, char *out_value, size_t max_len);

esp_err_t nvs_write_string(const char *namespace_name, const char *key, const char *value);

esp_err_t read_wifi_credentials(void);

esp_err_t save_wifi_credentials(const char *ssid, const char *password);

#endif