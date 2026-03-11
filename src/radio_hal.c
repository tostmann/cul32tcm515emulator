#include "radio_hal.h"
#include "cc1101_regs.h"
#include "esp3_proto.h"
#include "enocean_security.h"
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
#define TICK_TOLERANCE_LOW   16
#define TICK_SHORT_MAX       48
#define TICK_LONG_MAX        85
#define NOISE_RESET          200

typedef enum {
    DEC_IDLE,
    DEC_SYNCING,
    DEC_MID_BIT,
    DEC_EDGE
} dec_state_t;

typedef struct {
    dec_state_t state;
    uint8_t buffer[64];
    uint16_t bit_count;
    uint32_t sync_register;
    bool packet_ready;
} erp1_decoder_t;

static erp1_decoder_t global_decoder;

static rmt_channel_handle_t rx_channel = NULL;
static TaskHandle_t rf_task_handle = NULL;
static SemaphoreHandle_t rmt_done_sem = NULL;
static rmt_symbol_word_t *rmt_rx_buffer = NULL;
static SemaphoreHandle_t carrier_sense_sem = NULL;

// Dummy implementation for device lookup
enocean_sec_device_t* get_device_by_id(uint32_t sender_id) {
    return NULL; // To be implemented with NVS storage
}

static void erp1_decoder_reset(erp1_decoder_t *dec) {
    dec->state = DEC_IDLE;
    dec->bit_count = 0;
    dec->sync_register = 0;
    dec->packet_ready = false;
    memset(dec->buffer, 0, sizeof(dec->buffer));
}

static void handle_complete_telegram(uint8_t *raw_data, uint16_t len) {
    if (len < 7) return;
    
    // Check checksum (last byte is sum of all previous bytes)
    uint8_t sum = 0;
    for (int i = 0; i < len - 1; i++) sum += raw_data[i];
    if (sum != raw_data[len - 1]) return;

    uint8_t r_org = raw_data[1];
    uint32_t sender_id = (raw_data[len-5] << 24) | (raw_data[len-4] << 16) | 
                         (raw_data[len-3] << 8)  | raw_data[len-2];

    if (r_org == 0x30 || r_org == 0x31) {
        enocean_sec_device_t *dev = get_device_by_id(sender_id);
        if (dev) {
            // Placeholder for real security processing
            // For now, just pass through if MAC matches or send raw to host
        }
    }

    uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00 };
    esp3_send_packet(ESP3_TYPE_RADIO_ERP1, &raw_data[1], len - 1, opt, 7);
}

static void erp1_decode_pulse(erp1_decoder_t *dec, int level, int duration) {
    bool is_short = (duration >= TICK_TOLERANCE_LOW && duration <= TICK_SHORT_MAX);
    bool is_long  = (duration > TICK_SHORT_MAX && duration <= TICK_LONG_MAX);

    if (duration > NOISE_RESET || (!is_short && !is_long)) {
        dec->state = DEC_IDLE;
        return;
    }

    uint8_t decoded_bit = 0;
    bool bit_ready = false;

    switch (dec->state) {
        case DEC_IDLE:
        case DEC_SYNCING:
            dec->sync_register = (dec->sync_register << 1) | (level & 0x01);
            if ((dec->sync_register & 0xFFFFFF) == 0xAAAAAA) {
                dec->state = DEC_SYNCING;
            } else if (dec->state == DEC_SYNCING && (dec->sync_register & 0xFF) == 0xA5) {
                dec->state = DEC_MID_BIT;
                dec->bit_count = 0;
            }
            break;
        case DEC_MID_BIT:
            if (is_short) {
                dec->state = DEC_EDGE;
            } else if (is_long) {
                decoded_bit = level;
                bit_ready = true;
            }
            break;
        case DEC_EDGE:
            if (is_short) {
                decoded_bit = level;
                bit_ready = true;
                dec->state = DEC_MID_BIT;
            } else {
                dec->state = DEC_IDLE;
            }
            break;
    }

    if (bit_ready) {
        int byte_idx = dec->bit_count / 8;
        int bit_offset = 7 - (dec->bit_count % 8);
        if (decoded_bit) dec->buffer[byte_idx] |= (1 << bit_offset);
        else dec->buffer[byte_idx] &= ~(1 << bit_offset);
        dec->bit_count++;

        if (dec->bit_count >= 8) {
            uint8_t packet_len = dec->buffer[0];
            if (dec->bit_count >= (packet_len + 1) * 8) {
                handle_complete_telegram(dec->buffer, packet_len + 1);
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
                        erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level0, rmt_rx_buffer[i].duration0);
                        if (rmt_rx_buffer[i].duration1 > 0) {
                            erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level1, rmt_rx_buffer[i].duration1);
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
    // Correct Freq for 868.302 MHz (26.0 MHz Crystal): 0x21656A
    cc1101_write_reg(0x0D, 0x21); cc1101_write_reg(0x0E, 0x65); cc1101_write_reg(0x0F, 0x6A); 
    cc1101_write_reg(0x10, 0x4C); // MDMCFG4: BW ~406kHz (E=1, M=0) for 26MHz
    cc1101_write_reg(0x11, 0x3B); // MDMCFG3: 250kbps
    cc1101_write_reg(0x12, 0x30); // MDMCFG2: OOK, No Sync
    cc1101_write_reg(0x22, 0x11); cc1101_write_reg(0x21, 0xB6); 
    cc1101_write_reg(0x1B, 0x03); // AGCCTRL2: MAX_LNA_GAIN
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

static const uint8_t manchester_lut[16] = {
    0x55, 0x56, 0x59, 0x5A, 0x65, 0x66, 0x69, 0x6A,
    0x95, 0x96, 0x99, 0x9A, 0xA5, 0xA6, 0xA9, 0xAA
};

bool g_radio_loopback_enabled = true; // Standardmäßig AN zum Testen

static void erp1_decoder_reset(erp1_decoder_t *dec);
static void erp1_decode_pulse(erp1_decoder_t *dec, int level, int duration);

void radio_transmit(const uint8_t *data, uint8_t len) {
    if (!data || len == 0 || len > 24) return;

    // LOOPBACK: Sofortiges Rücksenden an Host (SIMULATION)
    if (g_radio_loopback_enabled) {
        uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x30, 0x00 }; // -48dBm RSSI Marker
        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, data, len, opt, 7);
    }

    uint8_t tx_buf[64];
    size_t tx_idx = 0;

    for(int i=0; i<7; i++) tx_buf[tx_idx++] = 0xAA;
    tx_buf[tx_idx++] = 0xA9;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        tx_buf[tx_idx++] = manchester_lut[byte >> 4];
        tx_buf[tx_idx++] = manchester_lut[byte & 0x0F];
    }
    tx_buf[tx_idx++] = 0x00;

    is_transmitting = true;

    for (int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE);
        cc1101_write_reg(0x02, 0x06);
        cc1101_write_reg(0x12, 0x30);
        cc1101_write_reg(0x08, 0x00);
        cc1101_write_reg(0x06, tx_idx);

        cc1101_strobe(CC1101_SFTX);
        cc1101_write_burst(0x3F, tx_buf, tx_idx);
        cc1101_strobe(CC1101_STX);

        int timeout = 1000;
        while(gpio_get_level(GPIO_NUM_3) && --timeout > 0) {
            esp_rom_delay_us(10);
        }

        if (sub < 2) {
            vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));
        }
    }

    cc1101_strobe(CC1101_SIDLE);
    cc1101_write_reg(0x02, 0x0D);
    cc1101_write_reg(0x08, 0x32);
    cc1101_strobe(CC1101_SRX);
    
    is_transmitting = false;
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
        erp1_decode_pulse(&global_decoder, level, duration);
    }
}
