#include "radio_hal.h"
#include "cc1101_regs.h"
#include "esp3_proto.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "RADIO_HAL";
static spi_device_handle_t spi_handle;
static SemaphoreHandle_t spi_mutex = NULL;
volatile bool is_transmitting = false;

// 8 MHz resolution for RMT (1 tick = 125ns)
#define RMT_RESOLUTION_HZ   8000000 
#define MAX_RMT_SYMBOLS     1024

// Decoder Timing (8MHz)
#define TICK_1T       32  // 4us (Half-Bit / Chip)
#define TICK_2T       64  // 8us (Full-Bit)
#define TICK_3_4      48  // 6us (Sample Point)
#define MIN_1T        14
#define MAX_1T        48
#define MIN_2T        49
#define MAX_2T        85
#define NOISE_RESET   200 // 25us idle

typedef enum {
    ERP1_STATE_SYNC,
    ERP1_STATE_SOF_WAIT_HIGH,
    ERP1_STATE_DATA
} erp1_state_t;

typedef struct {
    erp1_state_t state;
    int sync_pulses;
    int t_accum;
    int next_sample;
    uint8_t buffer[64];
    int byte_idx;
    int bit_idx;
    uint8_t current_byte;
    bool packet_ready;
} erp1_decoder_t;

static erp1_decoder_t global_decoder;

static rmt_channel_handle_t rx_channel = NULL;
static TaskHandle_t rf_task_handle = NULL;
static SemaphoreHandle_t rmt_done_sem = NULL;
static rmt_symbol_word_t *rmt_rx_buffer = NULL;
static SemaphoreHandle_t carrier_sense_sem = NULL;

static void erp1_decoder_reset(erp1_decoder_t *dec) {
    dec->state = ERP1_STATE_SYNC;
    dec->sync_pulses = 0;
    dec->byte_idx = 0;
    dec->bit_idx = 0;
    dec->current_byte = 0;
    dec->packet_ready = false;
}

static void push_bit(erp1_decoder_t *dec, uint8_t bit) {
    dec->current_byte = (dec->current_byte << 1) | (bit & 0x01);
    dec->bit_idx++;
    if (dec->bit_idx == 8) {
        if (dec->byte_idx < sizeof(dec->buffer)) dec->buffer[dec->byte_idx++] = dec->current_byte;
        dec->bit_idx = 0;
        dec->current_byte = 0;
    }
}

static void evaluate_packet(erp1_decoder_t *dec) {
    if (dec->byte_idx >= 7) {
        uint8_t sum = 0;
        for (int i = 0; i < dec->byte_idx - 1; i++) sum += dec->buffer[i];
        if (sum == dec->buffer[dec->byte_idx - 1]) dec->packet_ready = true;
    }
}

static void erp1_decode_pulse(erp1_decoder_t *dec, int level, int duration) {
    if (duration > NOISE_RESET) {
        if (dec->state == ERP1_STATE_DATA) evaluate_packet(dec);
        if (!dec->packet_ready) erp1_decoder_reset(dec);
        return;
    }
    switch (dec->state) {
        case ERP1_STATE_SYNC:
            if (duration >= MIN_1T && duration <= MAX_1T) dec->sync_pulses++;
            else if (duration >= MIN_2T && duration <= MAX_2T && level == 0 && dec->sync_pulses >= 6) {
                dec->state = ERP1_STATE_SOF_WAIT_HIGH;
            } else dec->sync_pulses = 0;
            break;
        case ERP1_STATE_SOF_WAIT_HIGH:
            if (level == 1 && duration >= MIN_1T && duration <= MAX_1T) {
                dec->state = ERP1_STATE_DATA;
                dec->t_accum = 0; dec->next_sample = TICK_3_4;
                dec->byte_idx = 0; dec->bit_idx = 0; dec->current_byte = 0;
            } else erp1_decoder_reset(dec);
            break;
        case ERP1_STATE_DATA: {
            int expected_center = dec->next_sample - (TICK_1T / 2);
            int edge_time = dec->t_accum + duration;
            if (abs(edge_time - expected_center) < 12) {
                int error = edge_time - expected_center;
                dec->next_sample += error / 4;
            }
            dec->t_accum += duration;
            while (dec->t_accum >= dec->next_sample) {
                push_bit(dec, (level == 0) ? 1 : 0);
                dec->next_sample += TICK_2T;
            }
            break;
        }
    }
}

static bool IRAM_ATTR rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(rmt_done_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static void IRAM_ATTR gdo2_cs_isr_handler(void* arg) {
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(carrier_sense_sem, &high_task_wakeup);
    if (high_task_wakeup) portYIELD_FROM_ISR();
}

static void spi_init(void) {
    spi_bus_config_t buscfg = { .miso_io_num = PIN_MISO, .mosi_io_num = PIN_MOSI, .sclk_io_num = PIN_SCK, .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 256 };
    spi_device_interface_config_t devcfg = { .clock_speed_hz = 5000000, .mode = 0, .spics_io_num = PIN_SS, .queue_size = 7 };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
}

void cc1101_strobe(uint8_t cmd) {
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    spi_transaction_t t = { .length = 8, .tx_data = {cmd}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(spi_handle, &t);
    xSemaphoreGive(spi_mutex);
}

void cc1101_write_reg(uint8_t addr, uint8_t value) {
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    uint8_t tx_data[2] = { addr, value };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data };
    spi_device_polling_transmit(spi_handle, &t);
    xSemaphoreGive(spi_mutex);
}

uint8_t cc1101_read_reg(uint8_t addr) {
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    uint8_t tx_data[2] = { (uint8_t)(addr | 0x80), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data, .rx_buffer = rx_data };
    spi_device_polling_transmit(spi_handle, &t);
    xSemaphoreGive(spi_mutex);
    return rx_data[1];
}

uint8_t cc1101_read_status(uint8_t addr) {
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    uint8_t tx_data[2] = { (uint8_t)(addr | 0xC0), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data, .rx_buffer = rx_data };
    spi_device_polling_transmit(spi_handle, &t);
    xSemaphoreGive(spi_mutex);
    return rx_data[1];
}

void cc1101_write_burst(uint8_t addr, const uint8_t *data, uint8_t len) {
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    uint8_t buf[129]; buf[0] = addr | 0x40; memcpy(&buf[1], data, len);
    spi_transaction_t t = { .length = (len + 1) * 8, .tx_buffer = buf };
    spi_device_polling_transmit(spi_handle, &t);
    xSemaphoreGive(spi_mutex);
}

static void rf_rx_task_impl(void *pvParameters) {
    rmt_receive_config_t cfg = { .signal_range_min_ns = 125, .signal_range_max_ns = 250000 }; 
    rmt_enable(rx_channel);
    erp1_decoder_reset(&global_decoder);
    while (1) {
        if (xSemaphoreTake(carrier_sense_sem, portMAX_DELAY) == pdTRUE) {
            if (is_transmitting) { continue; }
            memset(rmt_rx_buffer, 0, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t));
            if (rmt_receive(rx_channel, rmt_rx_buffer, MAX_RMT_SYMBOLS, &cfg) == ESP_OK) {
                if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(500)) == pdTRUE) {
                    size_t cnt = 0; while (cnt < MAX_RMT_SYMBOLS && rmt_rx_buffer[cnt].duration0 != 0) cnt++;
                    uint16_t debug_data[2] = { (uint16_t)cnt, (uint16_t)global_decoder.sync_pulses };
                    esp3_send_packet(0x34, (uint8_t*)debug_data, 4, NULL, 0);
                    for (size_t i = 0; i < cnt; i++) {
                        erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level0, rmt_rx_buffer[i].duration0);
                        if (global_decoder.packet_ready) {
                            uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00 };
                            esp3_send_packet(ESP3_TYPE_RADIO_ERP1, global_decoder.buffer, global_decoder.byte_idx, opt, 7);
                            erp1_decoder_reset(&global_decoder);
                        }
                        if (rmt_rx_buffer[i].duration1 > 0) {
                            erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level1, rmt_rx_buffer[i].duration1);
                            if (global_decoder.packet_ready) {
                                uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00 };
                                esp3_send_packet(ESP3_TYPE_RADIO_ERP1, global_decoder.buffer, global_decoder.byte_idx, opt, 7);
                                erp1_decoder_reset(&global_decoder);
                            }
                        }
                    }
                }
            }
            xSemaphoreTake(carrier_sense_sem, 0); 
        }
    }
}

void radio_rmt_rx_init(void) {
    if (!carrier_sense_sem) carrier_sense_sem = xSemaphoreCreateBinary();
    if (!rmt_done_sem) rmt_done_sem = xSemaphoreCreateBinary();
    if (!rmt_rx_buffer) rmt_rx_buffer = heap_caps_calloc(MAX_RMT_SYMBOLS, sizeof(rmt_symbol_word_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    rmt_rx_channel_config_t rcfg = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = RMT_RESOLUTION_HZ, .mem_block_symbols = 128, .gpio_num = PIN_GDO0, .flags.with_dma = true };
    rmt_new_rx_channel(&rcfg, &rx_channel);
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_callback };
    rmt_rx_register_event_callbacks(rx_channel, &cbs, NULL);
    xTaskCreate(rf_rx_task_impl, "rf_rx", 4096, NULL, 5, &rf_task_handle);
    gpio_config_t io_cs = { .pin_bit_mask = (1ULL << PIN_GDO2), .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_POSEDGE, .pull_down_en = 1 };
    gpio_config(&io_cs); gpio_isr_handler_add(PIN_GDO2, gdo2_cs_isr_handler, NULL);
}

static void radio_diag_task(void *pv) {
    while(1) {
        uint8_t rssi_raw = cc1101_read_status(0x34);
        int rssi_dbm;
        if (rssi_raw >= 128) rssi_dbm = (rssi_raw - 256) / 2 - 74;
        else rssi_dbm = rssi_raw / 2 - 74;
        uint8_t gdo2 = gpio_get_level(PIN_GDO2);
        uint8_t gdo0 = gpio_get_level(PIN_GDO0);
        uint8_t data[4] = { (uint8_t)rssi_raw, (uint8_t)rssi_dbm, gdo2, gdo0 };
        esp3_send_packet(0x33, data, 4, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void radio_hal_init(void) {
    if (!spi_mutex) spi_mutex = xSemaphoreCreateMutex();
    spi_init();
    gpio_config_t io_led = { .pin_bit_mask = (1ULL << PIN_LED), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_led); gpio_set_level(PIN_LED, 1);
    cc1101_strobe(CC1101_SRES); vTaskDelay(10);
    cc1101_write_reg(0x00, 0x0E); // Carrier Sense
    cc1101_write_reg(0x02, 0x0D); // Async Serial Out
    // Correct Freq for 868.300 MHz (26.0 MHz Crystal): 0x215E63
    cc1101_write_reg(0x0D, 0x21); cc1101_write_reg(0x0E, 0x5E); cc1101_write_reg(0x0F, 0x63); 
    cc1101_write_reg(0x10, 0x8D); // MDMCFG4: BW ~406kHz
    cc1101_write_reg(0x11, 0x3B); // MDMCFG3: 250kbps
    cc1101_write_reg(0x12, 0x30); // MDMCFG2: OOK, No Sync
    cc1101_write_reg(0x22, 0x11); cc1101_write_reg(0x21, 0xB6); 
    cc1101_write_reg(0x1B, 0x03); // AGCCTRL2: MAX_LNA_GAIN (Highest sensitivity)
    cc1101_write_reg(0x1C, 0x00); // AGCCTRL1: Max CS Sensitivity
    cc1101_write_reg(0x1D, 0x91); // AGCCTRL0: Freeze on CS
    cc1101_write_reg(0x18, 0x18); 
    cc1101_write_reg(0x23, 0x81); cc1101_write_reg(0x24, 0x35); cc1101_write_reg(0x25, 0x09); 
    static const uint8_t patable_ook[] = {0x00, 0xC0};
    cc1101_write_burst(0x3E, patable_ook, 2);
    gpio_install_isr_service(0); radio_rmt_rx_init();
    xTaskCreate(radio_diag_task, "diag", 2048, NULL, 1, NULL);
    cc1101_strobe(CC1101_SIDLE);
    cc1101_write_reg(0x08, 0x32); cc1101_strobe(CC1101_SRX);
}

typedef struct { uint8_t *buf; int pos; } bit_s;
static void push_c(bit_s *s, uint8_t c, int n) { for (int i = n - 1; i >= 0; i--) { if ((c >> i) & 1) s->buf[s->pos / 8] |= (1 << (7 - (s->pos % 8))); else s->buf[s->pos / 8] &= ~(1 << (7 - (s->pos % 8))); s->pos++; } }
static void push_m(bit_s *s, int b) { if (b) push_c(s, 0x02, 2); else push_c(s, 0x01, 2); }

void radio_transmit(const uint8_t *data, uint8_t len) {
    if (len > 31) return; 
    uint8_t chip_buf[128]; memset(chip_buf, 0, 128); bit_s s = { .buf = chip_buf, .pos = 0 };
    for (int i = 0; i < 4; i++) push_c(&s, 0xAA, 8); // 4 bytes Warmup
    for (int i = 0; i < 10; i++) push_m(&s, 1); // 10 bits Preamble
    push_m(&s, 0); // SOF
    for (int i = 0; i < len; i++) { for (int j = 7; j >= 0; j--) push_m(&s, (data[i] >> j) & 1); }
    push_c(&s, 0x00, 16); 
    int byte_len = (s.pos + 7) / 8;
    if (byte_len > 64) byte_len = 64; // Safety cut-off for FIFO
    is_transmitting = true; 
    cc1101_strobe(CC1101_SIDLE);
    cc1101_write_reg(0x08, 0x00); 
    cc1101_write_reg(0x06, byte_len); 
    for(int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE); cc1101_strobe(CC1101_SFTX); cc1101_write_burst(0x3F, chip_buf, byte_len); cc1101_strobe(CC1101_STX);
        int t = 50; while((cc1101_read_status(CC1101_MARCSTATE) & 0x1F) != 0x01 && t-- > 0) vTaskDelay(1);
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(15 + (esp_random() % 10)));
    }
    cc1101_strobe(CC1101_SIDLE);
    cc1101_write_reg(0x08, 0x32); cc1101_strobe(CC1101_SFRX); cc1101_strobe(CC1101_SRX);
    is_transmitting = false;
}
