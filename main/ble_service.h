#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize NimBLE
 * Start BLE advertising
 */
void ble_service_init(void);

/*
 * Send notification to connected phone
 */
void ble_service_notify(const char *message);

/*
 * Check whether phone is connected
 */
bool ble_service_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_SERVICE_H */