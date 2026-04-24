#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool got_wifi_credentials;

extern char g_ssid[32];

extern char g_password[64];

extern char g_server_ip[32];
/**
 * @brief Initialize Wi-Fi in station mode and
 * connect to the AP using the provided credentials.
 */
void wifi_init_sta(void);

esp_err_t read_wifi_credentials(void);

esp_err_t save_wifi_credentials(const char *ssid, const char *password);

esp_err_t wifi_enable(void);

void update_wifi_config_and_reconnect(void);

#endif /* WIFI_H */