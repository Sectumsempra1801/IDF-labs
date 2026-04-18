#ifndef _WIFI_H_
#define _WIFI_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern bool got_wifi_credentials;

extern char g_ssid[32];

extern char g_password[64];

/**
 * @brief Initialize Wi-Fi in station mode and 
 * connect to the AP using the provided credentials.
 */
void wifi_init_sta(void);


#endif /* _WIFI_H_ */