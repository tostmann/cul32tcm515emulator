#ifndef RADIO_HAL_H
#define RADIO_HAL_H

#include <stdint.h>
#include <stdbool.h>

// --- PIN DEFINITIONS (ESP32-C6) ---
#define PIN_GDO0   2
#define PIN_GDO2   3
#define PIN_LED    8
#define PIN_SWITCH 9
#define PIN_SS     18
#define PIN_SCK    19
#define PIN_MISO   20
#define PIN_MOSI   21

#ifdef __cplusplus
extern "C" {
#endif

void radio_hal_init(void);

// SPI Primitives
void cc1101_strobe(uint8_t cmd);
void cc1101_write_reg(uint8_t addr, uint8_t value);
uint8_t cc1101_read_reg(uint8_t addr);
uint8_t cc1101_read_status(uint8_t addr);

// Data Primitives
void cc1101_write_burst(uint8_t addr, const uint8_t *data, uint8_t len);
void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len);

// Interrupts
void radio_hal_enable_rx_isr(void (*isr_callback)(void*), void* arg);

#ifdef __cplusplus
}
#endif

#endif // RADIO_HAL_H
