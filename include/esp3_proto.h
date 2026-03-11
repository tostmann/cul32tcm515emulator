#ifndef ESP3_PROTO_H
#define ESP3_PROTO_H

#include <stdint.h>
#include <stdbool.h>

#define ESP3_SYNC_BYTE 0x55

// ESP3 Packet Types
#define ESP3_TYPE_RADIO_ERP1 0x01
#define ESP3_TYPE_RESPONSE   0x02

// Response Codes
#define RET_OK 0x00

typedef struct {
    uint16_t data_len;
    uint8_t opt_len;
    uint8_t packet_type;
    uint8_t *data;
    uint8_t *opt_data;
} esp3_packet_t;

// API
void esp3_init(void);
void esp3_process_byte(uint8_t byte);
void esp3_send_packet(uint8_t type, const uint8_t *data, uint16_t data_len, const uint8_t *opt, uint8_t opt_len);
void esp3_send_response(uint8_t ret_code);

#endif // ESP3_PROTO_H
