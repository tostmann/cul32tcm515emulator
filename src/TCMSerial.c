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

#include "TCMSerial.h"
#include "esp3_proto.h"
#include "radio_hal.h"
#include "enocean_nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#define RX_BUFFER_SIZE 2048

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile size_t rx_head = 0;
static volatile size_t rx_tail = 0;
static SemaphoreHandle_t buffer_mutex = NULL;

void TCMSerial_begin(uint32_t baud) {
    if (!buffer_mutex) buffer_mutex = xSemaphoreCreateMutex();
    enocean_nvs_init();
    esp3_init();
    radio_hal_init();
}

int TCMSerial_available(void) {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    int count = (rx_head - rx_tail + RX_BUFFER_SIZE) % RX_BUFFER_SIZE;
    xSemaphoreGive(buffer_mutex);
    return count;
}

int TCMSerial_read(void) {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    if (rx_head == rx_tail) {
        xSemaphoreGive(buffer_mutex);
        return -1;
    }
    int byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    xSemaphoreGive(buffer_mutex);
    return byte;
}

size_t TCMSerial_read_buf(uint8_t *buf, size_t size) {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    size_t i = 0;
    while (i < size && rx_head != rx_tail) {
        buf[i++] = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    }
    xSemaphoreGive(buffer_mutex);
    return i;
}

size_t TCMSerial_write(uint8_t byte) {
    esp3_process_byte(byte);
    return 1;
}

size_t TCMSerial_write_buf(const uint8_t *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        esp3_process_byte(buf[i]);
    }
    return size;
}

// Internal function called by esp3_proto or radio_hal to send data back to the host
void TCMSerial_internal_push(const uint8_t *data, size_t len) {
    if (!buffer_mutex) return;
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (size_t i = 0; i < len; i++) {
        size_t next = (rx_head + 1) % RX_BUFFER_SIZE;
        // Ignore overflow for testing, overwrite!
        rx_buffer[rx_head] = data[i];
        rx_head = next;
        if (rx_head == rx_tail) rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE; // Advance tail to drop oldest
    }
    xSemaphoreGive(buffer_mutex);
}
