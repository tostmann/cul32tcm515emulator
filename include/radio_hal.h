#ifndef RADIO_HAL_H
#define RADIO_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// --- PIN DEFINITIONS (ESP32-C6) ---
#define PIN_GDO0   2  // Async Data Out -> RMT RX
#define PIN_GDO2   3  // Carrier Sense -> GPIO Interrupt
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
void radio_transmit(const uint8_t *data, uint8_t len);
void radio_rmt_rx_init(void);

// SPI Primitives
void cc1101_strobe(uint8_t cmd);
void cc1101_write_reg(uint8_t addr, uint8_t value);
uint8_t cc1101_read_reg(uint8_t addr);
uint8_t cc1101_read_status(uint8_t addr);

// Data Primitives
void cc1101_write_burst(uint8_t addr, const uint8_t *data, uint8_t len);
void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len);

extern SemaphoreHandle_t rf_rx_semaphore;

#ifdef __cplusplus
}
#endif

#endif // RADIO_HAL_H
