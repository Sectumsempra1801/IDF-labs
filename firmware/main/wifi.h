#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

EventGroupHandle_t wifi_get_event_group(void);

extern bool wifi_connected;

/**
 * @brief Initialize Wi-Fi in station mode and
 * connect to the AP using the provided credentials.
 */
void wifi_init_sta(void);

void wifi_connect(void);

#endif