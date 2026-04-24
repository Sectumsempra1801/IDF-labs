#ifndef BLE_H
#define BLE_H

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"

/**
 * @brief Initialize the Bluetooth controller in BLE mode.
 */
esp_err_t ble_controller_init(void);

/**
 * @brief Initialize the Bluedroid stack.
 */
esp_err_t ble_stack_init(void);

/**
 * @brief Register GATT and GAP event callbacks.
 */
esp_err_t ble_callbacks_register(void);

/**
 * @brief Initialize the GATT server application.
 */
esp_err_t ble_gatts_app_init(void);

esp_err_t ble_enable(void);

#endif /* _BLE_H_ */