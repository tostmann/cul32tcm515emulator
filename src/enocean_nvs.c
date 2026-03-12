/*
 * Copyright (c) 2026 Dirk Tostmann tostmann@busware.de
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "enocean_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ENOCEAN_NVS";
static const char *NVS_NAMESPACE = "enocean_sec";

esp_err_t enocean_nvs_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t enocean_nvs_get_device(uint32_t sender_id, enocean_sec_device_t *device) {
    nvs_handle_t handle;
    char key[12];
    snprintf(key, sizeof(key), "%08X", (unsigned int)sender_id);

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t size = sizeof(enocean_sec_device_t);
    err = nvs_get_blob(handle, key, device, &size);
    nvs_close(handle);

    return err;
}

esp_err_t enocean_nvs_save_device(const enocean_sec_device_t *device) {
    nvs_handle_t handle;
    char key[12];
    snprintf(key, sizeof(key), "%08X", (unsigned int)device->sender_id);

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, key, device, sizeof(enocean_sec_device_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved device %s", key);
    }
    return err;
}
