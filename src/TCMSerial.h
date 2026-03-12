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

#ifndef TCM_SERIAL_H
#define TCM_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TCMSerial Class-like interface for drop-in replacement of HardwareSerial.
 */

void TCMSerial_begin(uint32_t baud);
int  TCMSerial_available(void);
int  TCMSerial_read(void);
size_t TCMSerial_write(uint8_t byte);
size_t TCMSerial_write_buf(const uint8_t *buf, size_t size);
size_t TCMSerial_read_buf(uint8_t *buf, size_t size);

// Internal use only
void TCMSerial_internal_push(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // TCM_SERIAL_H
