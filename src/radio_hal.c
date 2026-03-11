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

static const char *TAG = "RADIO_HAL";
static spi_device_handle_t spi_handle;
static SemaphoreHandle_t spi_mutex = NULL;
volatile bool is_transmitting = false;

#define RMT_RESOLUTION_HZ   1000000 
#define MAX_RMT_SYMBOLS     1024

static rmt_channel_handle_t rx_channel = NULL;
static TaskHandle_t rf_task_handle = NULL;
static SemaphoreHandle_t rmt_done_sem = NULL;
static rmt_symbol_word_t *rmt_rx_buffer = NULL;
static SemaphoreHandle_t carrier_sense_sem = NULL;

static bool IRAM_ATTR rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(rmt_done_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static void IRAM_ATTR gdo2_cs_isr_handler(void* arg) {
    BaseType_t high_task_wakeup = pdFALSE;
    gpio_intr_disable(PIN_GDO2);
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

static void rmt_to_manchester_decode(const rmt_symbol_word_t *symbols, size_t num_symbols) {
    uint32_t sync_reg = 0;
    bool in_frame = false;
    uint8_t payload[64]; int payload_len = 0;
    uint8_t cur_byte = 0; int bit_cnt = 0;
    int chip_pair = 0; uint8_t chip1 = 0;

    for (size_t i = 0; i < num_symbols; i++) {
        uint32_t d[2] = {symbols[i].duration0, symbols[i].duration1};
        uint8_t l[2] = {symbols[i].level0, symbols[i].level1};
        for (int p = 0; p < 2; p++) {
            if (d[p] == 0) continue;
            int count = (d[p] + 2) / 4; if (count > 4) count = 4;
            for (int c = 0; c < count; c++) {
                sync_reg = (sync_reg << 1) | l[p];
                if (!in_frame) {
                    // Start of frame: transition from preamble '1' (10) to SOF '0' (01)
                    // Preamble ends with ...1010. SOF is 01. So we look for ...101001
                    if ((sync_reg & 0x0F) == 0x09) { // ...1001
                        in_frame = true; chip_pair = 0; bit_cnt = 0; payload_len = 0; cur_byte = 0;
                    }
                } else {
                    if (chip_pair == 0) { chip1 = l[p]; chip_pair = 1; }
                    else {
                        uint8_t chip2 = l[p]; int bit = -1;
                        if (chip1 == 1 && chip2 == 0) bit = 1;
                        else if (chip1 == 0 && chip2 == 1) bit = 0;
                        if (bit != -1) {
                            cur_byte = (cur_byte << 1) | bit; bit_cnt++;
                            if (bit_cnt == 8) {
                                if (payload_len < sizeof(payload)) payload[payload_len++] = cur_byte;
                                bit_cnt = 0;
                            }
                        } else { in_frame = false; goto end; }
                        chip_pair = 0;
                    }
                }
            }
        }
    }
end:
    if (payload_len >= 7) { // Min ERP1 size
        uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00 };
        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, payload, payload_len, opt, 7);
    }
}

static void rf_rx_task_impl(void *pvParameters) {
    rmt_receive_config_t cfg = { .signal_range_min_ns = 1000, .signal_range_max_ns = 60000 };
    rmt_enable(rx_channel);
    while (1) {
        if (xSemaphoreTake(carrier_sense_sem, portMAX_DELAY) == pdTRUE) {
            gpio_set_level(PIN_LED, 0); // LED ON
            if (is_transmitting) { gpio_intr_enable(PIN_GDO2); gpio_set_level(PIN_LED, 1); continue; }
            memset(rmt_rx_buffer, 0, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t));
            if (rmt_receive(rx_channel, rmt_rx_buffer, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t), &cfg) == ESP_OK) {
                if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
                    size_t cnt = 0; while (cnt < MAX_RMT_SYMBOLS && rmt_rx_buffer[cnt].duration0 != 0) cnt++;
                    if (cnt > 15) rmt_to_manchester_decode(rmt_rx_buffer, cnt);
                }
            }
            int t = 100; while (gpio_get_level(PIN_GDO2) == 1 && t-- > 0) vTaskDelay(1);
            vTaskDelay(5); xSemaphoreTake(carrier_sense_sem, 0); gpio_intr_enable(PIN_GDO2);
            gpio_set_level(PIN_LED, 1); // LED OFF
        }
    }
}

void radio_rmt_rx_init(void) {
    if (!carrier_sense_sem) carrier_sense_sem = xSemaphoreCreateBinary();
    if (!rmt_done_sem) rmt_done_sem = xSemaphoreCreateBinary();
    if (!rmt_rx_buffer) rmt_rx_buffer = heap_caps_calloc(MAX_RMT_SYMBOLS, sizeof(rmt_symbol_word_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    rmt_rx_channel_config_t rcfg = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = RMT_RESOLUTION_HZ, .mem_block_symbols = 48, .gpio_num = PIN_GDO0, .flags.with_dma = true };
    rmt_new_rx_channel(&rcfg, &rx_channel);
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_callback };
    rmt_rx_register_event_callbacks(rx_channel, &cbs, NULL);
    xTaskCreate(rf_rx_task_impl, "rf_rx", 4096, NULL, 5, &rf_task_handle);
    gpio_config_t io = { .pin_bit_mask = (1ULL << PIN_GDO2), .mode = GPIO_MODE_INPUT, .pull_down_en = 1, .intr_type = GPIO_INTR_POSEDGE };
    gpio_config(&io); gpio_isr_handler_add(PIN_GDO2, gdo2_cs_isr_handler, NULL);
}

void radio_hal_init(void) {
    if (!spi_mutex) spi_mutex = xSemaphoreCreateMutex();
    spi_init();
    gpio_config_t io_led = { .pin_bit_mask = (1ULL << PIN_LED), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_led); gpio_set_level(PIN_LED, 1);
    cc1101_strobe(CC1101_SRES); vTaskDelay(10);
    for (int i = 0; i < sizeof(erp1_config)/sizeof(cc1101_cfg_t); i++) cc1101_write_reg(erp1_config[i].addr, erp1_config[i].val);
    cc1101_write_reg(0x0D, 0x21); cc1101_write_reg(0x0E, 0x65); cc1101_write_reg(0x0F, 0x6A); 
    cc1101_write_reg(0x10, 0x2D); cc1101_write_reg(0x11, 0x3B); cc1101_write_reg(0x12, 0x30); 
    cc1101_write_burst(0x3E, patable_ook, 2);
    gpio_install_isr_service(0); radio_rmt_rx_init(); cc1101_strobe(CC1101_SRX);
}

typedef struct { uint8_t *buf; int pos; } bit_s;
static void push_c(bit_s *s, uint8_t c, int n) { for (int i = n - 1; i >= 0; i--) { if ((c >> i) & 1) s->buf[s->pos / 8] |= (1 << (7 - (s->pos % 8))); else s->buf[s->pos / 8] &= ~(1 << (7 - (s->pos % 8))); s->pos++; } }
static void push_m(bit_s *s, int b) { if (b) push_c(s, 0x02, 2); else push_c(s, 0x01, 2); }

void radio_transmit(const uint8_t *data, uint8_t len) {
    if (len > 31) return; 
    uint8_t chip_buf[128]; memset(chip_buf, 0, 128); bit_s s = { .buf = chip_buf, .pos = 0 };
    for (int i = 0; i < 24; i++) push_m(&s, 1); // Preamble (24x '1')
    push_m(&s, 0); // SOF ('0')
    for (int i = 0; i < len; i++) { for (int j = 7; j >= 0; j--) push_m(&s, (data[i] >> j) & 1); }
    int byte_len = (s.pos + 7) / 8;
    is_transmitting = true; gpio_intr_disable(PIN_GDO2);
    cc1101_strobe(CC1101_SIDLE);
    cc1101_write_reg(0x10, 0x2D); cc1101_write_reg(0x11, 0x3B); cc1101_write_reg(0x08, 0x00); cc1101_write_reg(0x06, byte_len); cc1101_write_reg(0x12, 0x30); 
    for(int i = 0; i < 10; i++) { if (cc1101_read_status(0x34) < 140) break; vTaskDelay(pdMS_TO_TICKS(5)); }
    for(int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE); cc1101_strobe(CC1101_SFTX); cc1101_write_burst(0x3F, chip_buf, byte_len); cc1101_strobe(CC1101_STX);
        int t = 50; while((cc1101_read_status(CC1101_MARCSTATE) & 0x1F) != 0x01 && t-- > 0) vTaskDelay(1);
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(15));
    }
    cc1101_write_reg(0x10, 0x2D); cc1101_write_reg(0x08, 0x32); cc1101_write_reg(0x12, 0x30); cc1101_strobe(CC1101_SFRX); cc1101_strobe(CC1101_SRX);
    gpio_intr_enable(PIN_GDO2); is_transmitting = false;
}
