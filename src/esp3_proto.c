#include "esp3_proto.h"
#include "radio_hal.h"
#include "driver/usb_serial_jtag.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    STATE_SYNC, STATE_HEADER, STATE_CRC8H, STATE_DATA, STATE_CRC8D
} esp3_state_t;

static esp3_state_t rx_state = STATE_SYNC;
static uint8_t header_buf[4];
static uint8_t header_idx = 0;
static uint8_t payload_static_buf[512];
static uint16_t payload_idx = 0;
static esp3_packet_t current_packet;

// EnOcean CRC8
static uint8_t crc8_table[256];
static bool crc8_table_init = false;

static void init_crc8_table(void) {
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
        crc8_table[i] = crc;
    }
    crc8_table_init = true;
}

static uint8_t calc_buffer_crc(const uint8_t *buf, uint16_t len) {
    if (!crc8_table_init) init_crc8_table();
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ buf[i]];
    }
    return crc;
}

void esp3_init(void) {
    rx_state = STATE_SYNC;
    if (!crc8_table_init) init_crc8_table();
}

void esp3_send_packet(uint8_t type, const uint8_t *data, uint16_t data_len, const uint8_t *opt, uint8_t opt_len) {
    uint8_t header[4] = { (uint8_t)(data_len >> 8), (uint8_t)(data_len & 0xFF), opt_len, type };
    uint8_t crc8h = calc_buffer_crc(header, 4);
    
    uint8_t crc8d = 0;
    if (data_len > 0 || opt_len > 0) {
        // Calculate CRC8 over data then opt_data
        if (data_len > 0) {
             for(int i=0; i<data_len; i++) crc8d = crc8_table[crc8d ^ data[i]];
        }
        if (opt_len > 0) {
             for(int i=0; i<opt_len; i++) crc8d = crc8_table[crc8d ^ opt[i]];
        }
    }

    uint8_t sync = ESP3_SYNC_BYTE;
    usb_serial_jtag_write_bytes(&sync, 1, portMAX_DELAY);
    usb_serial_jtag_write_bytes(header, 4, portMAX_DELAY);
    usb_serial_jtag_write_bytes(&crc8h, 1, portMAX_DELAY);
    if (data_len > 0) usb_serial_jtag_write_bytes(data, data_len, portMAX_DELAY);
    if (opt_len > 0) usb_serial_jtag_write_bytes(opt, opt_len, portMAX_DELAY);
    usb_serial_jtag_write_bytes(&crc8d, 1, portMAX_DELAY);
}

void esp3_send_response(uint8_t ret_code) {
    esp3_send_packet(ESP3_TYPE_RESPONSE, &ret_code, 1, NULL, 0);
}

void esp3_process_byte(uint8_t byte) {
    switch (rx_state) {
        case STATE_SYNC:
            if (byte == ESP3_SYNC_BYTE) {
                header_idx = 0;
                rx_state = STATE_HEADER;
            }
            break;
        case STATE_HEADER:
            header_buf[header_idx++] = byte;
            if (header_idx == 4) rx_state = STATE_CRC8H;
            break;
        case STATE_CRC8H:
            if (byte == calc_buffer_crc(header_buf, 4)) {
                current_packet.data_len = (header_buf[0] << 8) | header_buf[1];
                current_packet.opt_len = header_buf[2];
                current_packet.packet_type = header_buf[3];
                uint16_t total_len = current_packet.data_len + current_packet.opt_len;
                if (total_len > 0 && total_len < 1024) {
                    payload_buf = malloc(total_len);
                    if (!payload_buf) { rx_state = STATE_SYNC; return; }
                    payload_idx = 0;
                    rx_state = STATE_DATA;
                } else if (total_len == 0) {
                    rx_state = STATE_CRC8D;
                } else {
                    rx_state = STATE_SYNC;
                }
            } else {
                rx_state = STATE_SYNC;
            }
            break;
        case STATE_DATA:
            payload_buf[payload_idx++] = byte;
            if (payload_idx == (current_packet.data_len + current_packet.opt_len)) {
                rx_state = STATE_CRC8D;
            }
            break;
        case STATE_CRC8D:
            {
                uint16_t total_len = current_packet.data_len + current_packet.opt_len;
                uint8_t expected_crc = calc_buffer_crc(payload_buf, total_len);
                if (byte == expected_crc) {
                    current_packet.data = payload_buf;
                    current_packet.opt_data = payload_buf + current_packet.data_len;
                    if (current_packet.packet_type == ESP3_TYPE_RADIO_ERP1) {
                        radio_transmit(current_packet.data, current_packet.data_len);
                        esp3_send_response(RET_OK);
                    }
                }
            }
            if (payload_buf) free(payload_buf);
            payload_buf = NULL;
            rx_state = STATE_SYNC;
            break;
    }
}
