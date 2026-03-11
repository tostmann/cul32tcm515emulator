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

static const char *TAG = "RADIO_HAL";
static spi_device_handle_t spi_handle;
SemaphoreHandle_t rf_rx_semaphore = NULL;
volatile bool is_transmitting = false;

// RMT Configuration
#define RMT_RESOLUTION_HZ   4000000 // 4 MHz -> 0.25 µs
#define MAX_RMT_SYMBOLS     512
#define THRESHOLD_SHORT_MIN 8   // 2.0 µs
#define THRESHOLD_SHORT_MAX 23  // 5.75 µs
#define THRESHOLD_LONG_MIN  24  // 6.0 µs
#define THRESHOLD_LONG_MAX  48  // 12.0 µs

static rmt_channel_handle_t rx_channel = NULL;
static TaskHandle_t rf_task_handle = NULL;
static SemaphoreHandle_t rmt_done_sem = NULL;
static rmt_symbol_word_t rmt_rx_buffer[MAX_RMT_SYMBOLS];

// ISR / Callbacks
static bool IRAM_ATTR rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(rmt_done_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static void IRAM_ATTR gdo2_cs_isr_handler(void* arg) {
    BaseType_t high_task_wakeup = pdFALSE;
    gpio_intr_disable(PIN_GDO2);
    vTaskNotifyGiveFromISR(rf_task_handle, &high_task_wakeup);
    if (high_task_wakeup) portYIELD_FROM_ISR();
}

// SPI Implementation
static void spi_init(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5000000,
        .mode = 0,
        .spics_io_num = PIN_SS,
        .queue_size = 3,
        .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));
}

void cc1101_strobe(uint8_t cmd) {
    spi_transaction_t t = { .length = 8, .tx_data = {cmd}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(spi_handle, &t);
}

void cc1101_write_reg(uint8_t addr, uint8_t value) {
    uint8_t tx_data[2] = { addr, value };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data };
    spi_device_polling_transmit(spi_handle, &t);
}

uint8_t cc1101_read_reg(uint8_t addr) {
    uint8_t tx_data[2] = { (uint8_t)(addr | 0x80), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data, .rx_buffer = rx_data };
    spi_device_polling_transmit(spi_handle, &t);
    return rx_data[1];
}

uint8_t cc1101_read_status(uint8_t addr) {
    uint8_t tx_data[2] = { (uint8_t)(addr | 0xC0), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data, .rx_buffer = rx_data };
    spi_device_polling_transmit(spi_handle, &t);
    return rx_data[1];
}

void cc1101_write_burst(uint8_t addr, const uint8_t *data, uint8_t len) {
    uint8_t header = addr | 0x40;
    spi_transaction_t t = { .length = 8, .tx_data = {header}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(spi_handle, &t);
    spi_transaction_t t_data = { .length = len * 8, .tx_buffer = data };
    spi_device_polling_transmit(spi_handle, &t_data);
}

void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len) {
    uint8_t header = addr | 0xC0;
    spi_transaction_t t = { .length = 8, .tx_data = {header}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(spi_handle, &t);
    spi_transaction_t t_data = { .length = len * 8, .rx_buffer = data };
    spi_device_polling_transmit(spi_handle, &t_data);
}

// Manchester Decoding
static void rmt_to_manchester_decode(const rmt_symbol_word_t *symbols, size_t num_symbols) {
    static uint8_t half_bits[MAX_RMT_SYMBOLS * 2];
    int hb_count = 0;

    for (size_t i = 0; i < num_symbols; i++) {
        int durations[2] = {symbols[i].duration0, symbols[i].duration1};
        int levels[2]    = {symbols[i].level0, symbols[i].level1};
        for (int p = 0; p < 2; p++) {
            int d = durations[p];
            int l = levels[p];
            if (d == 0) continue;
            // Filter short glitches (< 2us = 8 ticks)
            if (d < 8) continue;
            
            int n = (d + 8) / 16; // 4us = 16 ticks
            if (n > 4) n = 4; // Cap
            for (int k = 0; k < n && hb_count < sizeof(half_bits); k++) {
                half_bits[hb_count++] = l;
            }
        }
    }
    if (hb_count < 32) return;

    uint8_t payload[64];
    int payload_len = 0;
    bool synced = false;
    uint8_t current_byte = 0;
    int bit_count = 0;

    int sync_idx = -1;
    uint32_t pattern = 0;
    for (int i = 0; i < hb_count; i++) {
        pattern = (pattern << 1) | half_bits[i];
        // Raw EnOcean Sync Byte 0xAD = 1100110011110011 = 0xCCF3 (in half-bits)
        if ((pattern & 0xFFFF) == 0xCCF3) { 
            sync_idx = i + 1;
            synced = true;
            break;
        }
    }

    if (synced && sync_idx != -1) {
        for (int i = sync_idx; i < hb_count - 1; i += 2) {
            uint8_t hb1 = half_bits[i];
            uint8_t hb2 = half_bits[i+1];
            int decoded_bit = -1;
            // EnOcean Manchester: 1 -> 10, 0 -> 01
            if (hb1 == 1 && hb2 == 0) decoded_bit = 1;
            else if (hb1 == 0 && hb2 == 1) decoded_bit = 0;
            else { i--; continue; } // Try resync
            
            current_byte = (current_byte << 1) | decoded_bit;
            bit_count++;
            if (bit_count == 8) {
                if (payload_len < sizeof(payload)) payload[payload_len++] = current_byte;
                bit_count = 0;
            }
        }
    }
    if (synced && payload_len > 0) {
        uint8_t rssi_raw = cc1101_read_status(0x34);
        int dbm = (rssi_raw >= 128) ? (rssi_raw - 256) / 2 - 74 : rssi_raw / 2 - 74;
        uint8_t opt_data[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, (uint8_t)(-dbm), 0x00 };
        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, payload, payload_len, opt_data, 7);
    }
}

static void rf_rx_task_impl(void *pvParameters) {
    rmt_receive_config_t rec_config = {
        .signal_range_min_ns = 2000,
        .signal_range_max_ns = 40000,
    };
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_ERROR_CHECK(rmt_receive(rx_channel, rmt_rx_buffer, sizeof(rmt_rx_buffer), &rec_config));
        if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            size_t rx_symbols_count = 0;
            for(int i=0; i<MAX_RMT_SYMBOLS; i++) {
                if(rmt_rx_buffer[i].duration0 == 0) break;
                rx_symbols_count++;
            }
            if (rx_symbols_count > 10) {
                rmt_to_manchester_decode(rmt_rx_buffer, rx_symbols_count);
            }
        }
        while (gpio_get_level(PIN_GDO2) == 1) vTaskDelay(pdMS_TO_TICKS(1));
        vTaskDelay(pdMS_TO_TICKS(5)); 
        gpio_intr_enable(PIN_GDO2);
    }
}

void radio_rmt_rx_init(void) {
    rmt_done_sem = xSemaphoreCreateBinary();
    rmt_rx_channel_config_t rx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = MAX_RMT_SYMBOLS,
        .gpio_num = PIN_GDO0,
        .flags.invert_in = false,
        .flags.with_dma = false
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &rx_channel));
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_callback };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(rx_channel));
    
    xTaskCreate(rf_rx_task_impl, "rf_rx_task", 4096, NULL, 5, &rf_task_handle);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_GDO2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 1,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);
    gpio_isr_handler_add(PIN_GDO2, gdo2_cs_isr_handler, NULL);
}

void radio_hal_init(void) {
    spi_init();
    gpio_config_t io_led = { .pin_bit_mask = (1ULL << PIN_LED), .mode = GPIO_MODE_OUTPUT, .pull_up_en = 1 };
    gpio_config(&io_led);
    gpio_set_level(PIN_LED, 1);
    
    cc1101_strobe(CC1101_SRES);
    vTaskDelay(pdMS_TO_TICKS(10));
    for (int i = 0; i < sizeof(erp1_config)/sizeof(cc1101_cfg_t); i++) {
        cc1101_write_reg(erp1_config[i].addr, erp1_config[i].val);
    }
    cc1101_write_burst(0x3E, patable_ook, 2);
    
    gpio_install_isr_service(0);
    radio_rmt_rx_init();
    
    cc1101_strobe(CC1101_SRX);
    ESP_LOGI(TAG, "CC1101 Initialized in Async RX Mode with RMT");
}

static bool check_lbt(void) {
    uint8_t rssi_raw = cc1101_read_status(0x34);
    if (rssi_raw >= 75) return false;
    return true;
}

static const uint8_t manchester_lut[16] = {
    0x55, 0x56, 0x59, 0x5A, 0x65, 0x66, 0x69, 0x6A,
    0x95, 0x96, 0x99, 0x9A, 0xA5, 0xA6, 0xA9, 0xAA
};

void radio_transmit(const uint8_t *data, uint8_t len) {
    if (len > 31) return; 
    uint8_t encoded_len = len * 2;
    uint8_t encoded_data[64];

    for (uint8_t i = 0; i < len; i++) {
        encoded_data[i * 2]     = manchester_lut[data[i] >> 4];
        encoded_data[i * 2 + 1] = manchester_lut[data[i] & 0x0F];
    }

    is_transmitting = true;
    gpio_intr_disable(PIN_GDO2);
    
    cc1101_strobe(CC1101_SIDLE);
    
    // Set Baudrate to 250kbps for raw chip transmission (since 125kbps Manchester = 250kbps chips)
    cc1101_write_reg(0x10, 0x2D); // MDMCFG4: 250kbps
    cc1101_write_reg(0x11, 0x3B); // MDMCFG3
    
    cc1101_write_reg(0x08, 0x00);        // PKTCTRL0: Fixed Packet Length
    cc1101_write_reg(0x06, encoded_len); // PKTLEN
    cc1101_write_reg(0x13, 0x22);        // MDMCFG1: 4 bytes preamble
    cc1101_write_reg(0x04, 0xAA);        // SYNC1
    cc1101_write_reg(0x05, 0xAD);        // SYNC0
    cc1101_write_reg(0x12, 0x32);        // MDMCFG2: ASK/OOK, No HW Manchester, 16/16 sync
    
    for(int i = 0; i < 5; i++) {
        if(check_lbt()) break;
        vTaskDelay(pdMS_TO_TICKS(2 + (esp_random() % 5)));
    }
    
    for(int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE);
        cc1101_strobe(CC1101_SFTX);
        cc1101_write_burst(0x3F, encoded_data, encoded_len);
        cc1101_strobe(CC1101_STX);
        int timeout = 50; 
        while((cc1101_read_status(CC1101_MARCSTATE) & 0x1F) != 0x01 && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            timeout--;
        }
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(10 + (esp_random() % 16)));
    }
    
    // Restore Async RX Mode (125kbps filters)
    cc1101_write_reg(0x10, 0x4C); // Optimized MDMCFG4 for RX
    cc1101_write_reg(0x11, 0x3B); // Optimized MDMCFG3 for RX
    cc1101_write_reg(0x08, 0x32); 
    cc1101_write_reg(0x12, 0x30); 
    cc1101_strobe(CC1101_SFRX);
    cc1101_strobe(CC1101_SRX);
    
    gpio_intr_enable(PIN_GDO0);
    is_transmitting = false;
}
