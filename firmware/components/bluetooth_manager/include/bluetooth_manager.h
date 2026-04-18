#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

typedef void (*ble_wifi_cred_cb_t)(const char *ssid, const char *password);

void ble_manager_set_callback(ble_wifi_cred_cb_t cb);
void ble_manager_init(void);

#endif