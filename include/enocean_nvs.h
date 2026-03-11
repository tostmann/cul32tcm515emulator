#ifndef ENOCEAN_NVS_H
#define ENOCEAN_NVS_H

#include "enocean_security.h"
#include <esp_err.h>

/**
 * @brief Initialize NVS for EnOcean device storage.
 */
esp_err_t enocean_nvs_init(void);

/**
 * @brief Get security configuration for a device.
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND otherwise.
 */
esp_err_t enocean_nvs_get_device(uint32_t sender_id, enocean_sec_device_t *device);

/**
 * @brief Save/Update security configuration for a device.
 */
esp_err_t enocean_nvs_save_device(const enocean_sec_device_t *device);

#endif // ENOCEAN_NVS_H
