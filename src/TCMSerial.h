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

// Internal use only
void TCMSerial_internal_push(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // TCM_SERIAL_H
