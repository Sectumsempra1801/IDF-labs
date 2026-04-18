#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stddef.h>
#include "esp_err.h"

void wifi_manager_save_creds(const char *ssid, const char *password);
void wifi_manager_read_creds(char *out_ssid, char *out_password, size_t max_len);
void wifi_manager_init(void);
void wifi_manager_connect(const char *ssid, const char *password);

#endif