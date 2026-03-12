#include "radio_hal.h"
#include "cc1101_regs.h"
#include "esp3_proto.h"
#include "enocean_security.h"
#include "enocean_nvs.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
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

// --- Timing & Toleranzen (Basis: 8MHz, 1 Tick = 125ns) ---
#define ERP1_PULSE_SHORT_MIN  12    // ~1.5 µs
#define ERP1_PULSE_SHORT_MAX  47    // ~5.8 µs
#define ERP1_PULSE_LONG_MIN   48    // ~6.0 µs
#define ERP1_PULSE_LONG_MAX   95    // ~11.8 µs
#define ERP1_MIN_PREAMBLE     6     
#define ERP1_MAX_PAYLOAD      64    
#define ERP1_MIN_LENGTH       7     
#define NOISE_RESET           200

typedef enum {
    ERP1_STATE_SYNC,   
    ERP1_STATE_DATA    
} erp1_state_t;

typedef struct {
    erp1_state_t state;
    uint8_t  half;           
    uint8_t  c1_val;         
    uint32_t preamble_bits;  
    uint8_t  buffer[ERP1_MAX_PAYLOAD];
    uint16_t byte_idx;
    uint8_t  bit_idx;
} erp1_decoder_t;

static erp1_decoder_t global_decoder;

static rmt_channel_handle_t rx_channel = NULL;
static rmt_channel_handle_t tx_channel = NULL;
static rmt_encoder_handle_t tx_encoder = NULL;
static TaskHandle_t rf_task_handle = NULL;
static SemaphoreHandle_t rmt_done_sem = NULL;
static rmt_symbol_word_t *rmt_rx_buffer = NULL;
static SemaphoreHandle_t carrier_sense_sem = NULL;

// Forward declarations
static void erp1_decoder_reset(erp1_decoder_t *dec);
static void erp1_decode_pulse(erp1_decoder_t *dec, int level, int duration, bool noise_reset);

static void erp1_decoder_reset(erp1_decoder_t *dec) {
    dec->state = ERP1_STATE_SYNC;
    dec->half = 0;
    dec->c1_val = 0;
    dec->preamble_bits = 0;
    dec->byte_idx = 0;
    dec->bit_idx = 0;
}

static void handle_complete_telegram(const uint8_t *data, uint16_t length) {
    if (length < ERP1_MIN_LENGTH) return;
    
    uint8_t checksum = 0;
    for (int i = 0; i < length - 1; i++) checksum += data[i];
    if (checksum != data[length - 1]) return;

    uint8_t r_org = data[0];
    uint32_t sender_id = (data[length-6] << 24) | (data[length-5] << 16) | 
                         (data[length-4] << 8)  | data[length-3];
    uint8_t status = data[length-2];

    if ((r_org == 0x30 || r_org == 0x31) && length >= 15) {
        enocean_sec_device_t dev;
        if (enocean_nvs_get_device(sender_id, &dev) == ESP_OK) {
            uint32_t rlc_rx = (data[2] << 16) | (data[3] << 8) | data[4];
            
            // Anti-Replay: RLC must be strictly greater
            if (rlc_rx <= dev.rlc && dev.rlc != 0) {
                ESP_LOGW(TAG, "Replay attack detected for %08lX (RX: %lu, stored: %lu)", (unsigned long)sender_id, (unsigned long)rlc_rx, (unsigned long)dev.rlc);
                return;
            }

            const uint8_t *mac_rx = &data[5];
            uint16_t payload_len = length - 14; 
            
            uint8_t *payload = malloc(payload_len);
            if (payload) {
                memcpy(payload, &data[1], payload_len);
                if (r_org == 0x31) enocean_sec_decrypt_ctr(&dev, payload, payload_len, rlc_rx);
                
                uint8_t mi[64]; uint16_t mi_idx = 0;
                mi[mi_idx++] = (rlc_rx >> 24) & 0xFF; 
                mi[mi_idx++] = (rlc_rx >> 16) & 0xFF;
                mi[mi_idx++] = (rlc_rx >> 8) & 0xFF;
                mi[mi_idx++] = rlc_rx & 0xFF;
                mi[mi_idx++] = r_org;
                memcpy(&mi[mi_idx], payload, payload_len); mi_idx += payload_len;
                mi[mi_idx++] = (sender_id >> 24) & 0xFF;
                mi[mi_idx++] = (sender_id >> 16) & 0xFF;
                mi[mi_idx++] = (sender_id >> 8) & 0xFF;
                mi[mi_idx++] = sender_id & 0xFF;
                mi[mi_idx++] = status;

                if (enocean_sec_verify_mac_raw(&dev, mi, mi_idx, mac_rx, 4)) {
                    dev.rlc = rlc_rx;
                    enocean_nvs_save_device(&dev);
                    
                    uint8_t diag[2] = { 0xCC, payload[0] };
                    esp3_send_packet(0x35, diag, 2, NULL, 0);

                    uint8_t *dec_data = malloc(length);
                    if (dec_data) {
                        memcpy(dec_data, data, length);
                        memcpy(&dec_data[1], payload, payload_len);
                        uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x01 }; 
                        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, dec_data, length, opt, 7);
                        free(dec_data);
                    }
                } else {
                    ESP_LOGW(TAG, "MAC fail for %08lX", (unsigned long)sender_id);
                }
                free(payload);
            }
            return;
        }
    }

    uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00 };
    esp3_send_packet(ESP3_TYPE_RADIO_ERP1, data, length, opt, 7);
}

static void erp1_push_bit(erp1_decoder_t *dec, uint8_t bit) {
    if (dec->state == ERP1_STATE_SYNC) {
        if (bit == 1) {
            dec->preamble_bits++;
        } else if (bit == 0) {
            if (dec->preamble_bits >= ERP1_MIN_PREAMBLE) {
                dec->state = ERP1_STATE_DATA;
                dec->byte_idx = 0;
                dec->bit_idx = 0;
                memset(dec->buffer, 0, sizeof(dec->buffer));
            } else {
                erp1_decoder_reset(dec);
            }
        }
    } else if (dec->state == ERP1_STATE_DATA) {
        if (dec->byte_idx < ERP1_MAX_PAYLOAD) {
            if (bit) dec->buffer[dec->byte_idx] |= (1 << (7 - dec->bit_idx));
            dec->bit_idx++;
            if (dec->bit_idx >= 8) {
                dec->bit_idx = 0;
                dec->byte_idx++;
            }
        } else {
            erp1_decoder_reset(dec);
        }
    }
}

static void erp1_decode_pulse(erp1_decoder_t *dec, int level, int duration, bool noise_reset) {
    if (noise_reset) {
        if (dec->state == ERP1_STATE_DATA && dec->byte_idx >= ERP1_MIN_LENGTH) {
            handle_complete_telegram(dec->buffer, dec->byte_idx);
        }
        erp1_decoder_reset(dec);
        return;
    }

    bool is_short = (duration >= ERP1_PULSE_SHORT_MIN && duration <= ERP1_PULSE_SHORT_MAX);
    bool is_long  = (duration >= ERP1_PULSE_LONG_MIN && duration <= ERP1_PULSE_LONG_MAX);

    if (!is_short && !is_long) {
        erp1_decoder_reset(dec);
        return;
    }

    if (is_short) {
        if (dec->half == 0) {
            dec->c1_val = level;
            dec->half = 1;
        } else {
            if (dec->c1_val != level) {
                erp1_push_bit(dec, dec->c1_val);
                dec->half = 0;
            } else {
                erp1_decoder_reset(dec);
            }
        }
    } else { // is_long
        if (dec->half == 0) {
            erp1_decoder_reset(dec);
        } else {
            if (dec->c1_val != level) {
                erp1_push_bit(dec, dec->c1_val);
                dec->c1_val = level;
                dec->half = 1;
            } else {
                erp1_decoder_reset(dec);
            }
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
                    uint16_t debug_data[2] = { (uint16_t)cnt, (uint16_t)(global_decoder.state) };
                    esp3_send_packet(0x34, (uint8_t*)debug_data, 4, NULL, 0);
                    for (size_t i = 0; i < cnt; i++) {
                        erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level0, rmt_rx_buffer[i].duration0, false);
                        if (rmt_rx_buffer[i].duration1 > 0) {
                            erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level1, rmt_rx_buffer[i].duration1, false);
                        }
                    }
                    erp1_decode_pulse(&global_decoder, 0, 0, true); 
                }
            }
            xSemaphoreTake(carrier_sense_sem, 0); 
        }
    }
}

void radio_rmt_init(void) {
    if (!carrier_sense_sem) carrier_sense_sem = xSemaphoreCreateBinary();
    if (!rmt_done_sem) rmt_done_sem = xSemaphoreCreateBinary();
    if (!rmt_rx_buffer) rmt_rx_buffer = heap_caps_calloc(MAX_RMT_SYMBOLS, sizeof(rmt_symbol_word_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    
    // RX Channel
    rmt_rx_channel_config_t rcfg = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = RMT_RESOLUTION_HZ, .mem_block_symbols = 128, .gpio_num = PIN_GDO0, .flags.with_dma = true };
    rmt_new_rx_channel(&rcfg, &rx_channel);
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_callback };
    rmt_rx_register_event_callbacks(rx_channel, &cbs, NULL);
    
    // TX Channel (Same GPIO as RX!)
    rmt_tx_channel_config_t tcfg = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = RMT_RESOLUTION_HZ, .mem_block_symbols = 128, .gpio_num = PIN_GDO0, .trans_queue_depth = 4 };
    rmt_new_tx_channel(&tcfg, &tx_channel);

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
    // Correct Pin Assignment:
    cc1101_write_reg(0x00, 0x0D); // IOCFG0: GDO0 = Serial Data Output (Async)
    cc1101_write_reg(0x02, 0x0E); // IOCFG2: GDO2 = Carrier Sense
    
    // Frequency: 868.3 MHz (26MHz XTAL) -> 0x216550
    cc1101_write_reg(0x0D, 0x21); cc1101_write_reg(0x0E, 0x65); cc1101_write_reg(0x0F, 0x50); 

    cc1101_write_reg(0x10, 0x2D); // MDMCFG4: BW ~540kHz
    cc1101_write_reg(0x11, 0x3B); // MDMCFG3: 250kbps
    cc1101_write_reg(0x12, 0x30); // MDMCFG2: OOK, No Sync
    
    // PKTCTRL0: Bit 5:4 = 11 (Async serial mode)
    cc1101_write_reg(0x08, 0x32); 

    cc1101_write_reg(0x1B, 0x07); // AGCCTRL2: Max target amplitude
    cc1101_write_reg(0x1C, 0x00); // AGCCTRL1: Max relative CS sensitivity
    cc1101_write_reg(0x1D, 0x92); // AGCCTRL0: Freeze on CS, high filter BW
    
    static const uint8_t patable_ook[] = {0x00, 0xC0};
    cc1101_write_burst(0x3E, patable_ook, 2);

    gpio_install_isr_service(0); radio_rmt_init();
    xTaskCreate(radio_diag_task, "diag", 2048, NULL, 1, NULL);
    cc1101_strobe(CC1101_SIDLE);
    cc1101_strobe(CC1101_SRX);
}

static const uint8_t manchester_lut[16] = {
    0x55, 0x56, 0x59, 0x5A, 0x65, 0x66, 0x69, 0x6A,
    0x95, 0x96, 0x99, 0x9A, 0xA5, 0xA6, 0xA9, 0xAA
};

bool g_radio_loopback_enabled = false; // Disable for link test

static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {
    if (bit) { // 1 -> 10 (High, Low) - Alternative Polarity
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 1, .duration1 = 32, .level1 = 0 };
    } else {   // 0 -> 01 (Low, High)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 0, .duration1 = 32, .level1 = 1 };
    }
}

void radio_transmit(const uint8_t *data, uint8_t len) {
    if (!data || len == 0 || len > 32) return;

    if (g_radio_loopback_enabled) {
        uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x30, 0x00 }; 
        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, data, len, opt, 7);
    }

    rmt_symbol_word_t *tx_symbols = malloc(1024 * sizeof(rmt_symbol_word_t));
    if (!tx_symbols) return;

    size_t s_idx = 0;
    // Preamble: 12 bits of 1
    for (int i = 0; i < 12; i++) push_manchester_bit(tx_symbols, &s_idx, 1);
    // Sync bit: 0
    push_manchester_bit(tx_symbols, &s_idx, 0);
    // Data bits
    for (int i = 0; i < len; i++) {
        for (int b = 7; b >= 0; b--) {
            push_manchester_bit(tx_symbols, &s_idx, (data[i] >> b) & 1);
        }
    }
    tx_symbols[s_idx++] = (rmt_symbol_word_t){ .duration0 = 100, .level0 = 0, .duration1 = 0, .level1 = 0 };

    is_transmitting = true;
    rmt_disable(rx_channel);

    for (int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE);
        cc1101_strobe(CC1101_STX);
        esp_rom_delay_us(100); 

        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, NULL, tx_symbols, s_idx, &tx_cfg);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));
    }

    cc1101_strobe(CC1101_SIDLE);
    cc1101_strobe(CC1101_SRX);
    rmt_enable(rx_channel);
    is_transmitting = false;
    free(tx_symbols);
}

static inline int hex2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void hal_inject_virtual_rmt(const char* hex_pulses) {
    if (!hex_pulses) return;
    size_t len = strlen(hex_pulses);
    for (size_t i = 0; i + 3 < len; i += 4) {
        uint16_t val = (hex2int(hex_pulses[i]) << 12) |
                       (hex2int(hex_pulses[i+1]) << 8) |
                       (hex2int(hex_pulses[i+2]) << 4) |
                       (hex2int(hex_pulses[i+3]));
        int level = (val & 0x8000) ? 1 : 0;
        int duration = val & 0x7FFF;
        erp1_decode_pulse(&global_decoder, level, duration, false);
    }
    erp1_decode_pulse(&global_decoder, 0, 0, true);
}
