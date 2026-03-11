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
SemaphoreHandle_t rf_rx_semaphore = NULL;
volatile bool is_transmitting = false;

// RMT Configuration
#define RMT_RESOLUTION_HZ   1000000 // 1 MHz -> 1 µs
#define MAX_RMT_SYMBOLS     1024

static rmt_channel_handle_t rx_channel = NULL;
static TaskHandle_t rf_task_handle = NULL;
static SemaphoreHandle_t rmt_done_sem = NULL;
static rmt_symbol_word_t *rmt_rx_buffer = NULL;
static SemaphoreHandle_t carrier_sense_sem = NULL;

// ISR / Callbacks
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

// SPI Implementation
static void spi_init(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5000000,
        .mode = 0,
        .spics_io_num = PIN_SS,
        .queue_size = 7,
        .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));
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
    if (len > 128) return; 
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    uint8_t buf[129];
    buf[0] = addr | 0x40;
    memcpy(&buf[1], data, len);
    spi_transaction_t t = { .length = (len + 1) * 8, .tx_buffer = buf };
    spi_device_polling_transmit(spi_handle, &t);
    xSemaphoreGive(spi_mutex);
}

void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len) {
    if (len > 128) return;
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    uint8_t tx_buf[129] = { (uint8_t)(addr | 0xC0) };
    uint8_t rx_buf[129] = { 0 };
    spi_transaction_t t = { .length = (len + 1) * 8, .tx_buffer = tx_buf, .rx_buffer = rx_buf };
    spi_device_polling_transmit(spi_handle, &t);
    memcpy(data, &rx_buf[1], len);
    xSemaphoreGive(spi_mutex);
}

// Manchester Decoding
// 1 -> 10 (High Low)
// 0 -> 01 (Low High)
// Sync Word: 0xA55A
static void rmt_to_manchester_decode(const rmt_symbol_word_t *symbols, size_t num_symbols) {
    uint16_t sync_shift_reg = 0;
    bool sync_found = false;
    uint8_t payload[64];
    int payload_len = 0;
    uint8_t current_byte = 0;
    int bit_count = 0;
    int chip_state = 0; // 0: Expect first chip of pair, 1: Expect second chip

    for (size_t i = 0; i < num_symbols; i++) {
        uint32_t durations[2] = {symbols[i].duration0, symbols[i].duration1};
        uint8_t levels[2]    = {symbols[i].level0, symbols[i].level1};
        
        for (int p = 0; p < 2; p++) {
            uint32_t d = durations[p];
            if (d == 0) continue;
            
            int chip_count = 0;
            if (d >= 2 && d <= 6) chip_count = 1;
            else if (d > 6 && d <= 11) chip_count = 2;
            else {
                sync_shift_reg = 0;
                sync_found = false;
                continue;
            }

            for (int c = 0; c < chip_count; c++) {
                sync_shift_reg = (sync_shift_reg << 1) | levels[p];
                
                if (!sync_found) {
                    if (sync_shift_reg == 0xA55A) {
                        sync_found = true;
                        chip_state = 0;
                        bit_count = 0;
                        payload_len = 0;
                    }
                } else {
                    // We are in payload. Process chips in pairs.
                    static uint8_t first_chip;
                    if (chip_state == 0) {
                        first_chip = levels[p];
                        chip_state = 1;
                    } else {
                        uint8_t second_chip = levels[p];
                        int bit = -1;
                        if (first_chip == 1 && second_chip == 0) bit = 1;
                        else if (first_chip == 0 && second_chip == 1) bit = 0;
                        
                        if (bit != -1) {
                            current_byte = (current_byte << 1) | bit;
                            bit_count++;
                            if (bit_count == 8) {
                                if (payload_len < sizeof(payload)) payload[payload_len++] = current_byte;
                                bit_count = 0;
                            }
                        } else {
                            // Manchester error -> End of packet
                            sync_found = false;
                            goto packet_end;
                        }
                        chip_state = 0;
                    }
                }
            }
        }
    }

packet_end:
    if (payload_len > 0) {
        uint8_t opt_data[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00 };
        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, payload, payload_len, opt_data, 7);
    }
}

static void rf_rx_task_impl(void *pvParameters) {
    rmt_receive_config_t rec_config = {
        .signal_range_min_ns = 1000,
        .signal_range_max_ns = 60000,
    };
    rmt_enable(rx_channel);
    while (1) {
        if (xSemaphoreTake(carrier_sense_sem, portMAX_DELAY) == pdTRUE) {
            if (is_transmitting) {
                gpio_intr_enable(PIN_GDO2);
                continue;
            }
            
            memset(rmt_rx_buffer, 0, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t));
            if (rmt_receive(rx_channel, rmt_rx_buffer, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t), &rec_config) == ESP_OK) {
                if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
                    size_t rx_symbols_count = 0;
                    for(int i=0; i<MAX_RMT_SYMBOLS; i++) {
                        if(rmt_rx_buffer[i].duration0 == 0) break;
                        rx_symbols_count++;
                    }
                    if (rx_symbols_count > 20) {
                        rmt_to_manchester_decode(rmt_rx_buffer, rx_symbols_count);
                    }
                }
            }
            
            int wait_tout = 100;
            while (gpio_get_level(PIN_GDO2) == 1 && wait_tout > 0) {
                vTaskDelay(pdMS_TO_TICKS(1));
                wait_tout--;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            xSemaphoreTake(carrier_sense_sem, 0); 
            gpio_intr_enable(PIN_GDO2);
        }
    }
}

void radio_rmt_rx_init(void) {
    if (carrier_sense_sem == NULL) carrier_sense_sem = xSemaphoreCreateBinary();
    if (rmt_done_sem == NULL) rmt_done_sem = xSemaphoreCreateBinary();
    
    if (rmt_rx_buffer == NULL) {
        rmt_rx_buffer = (rmt_symbol_word_t *)heap_caps_calloc(
            MAX_RMT_SYMBOLS, 
            sizeof(rmt_symbol_word_t), 
            MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
        );
    }
    
    if (rmt_rx_buffer == NULL || rmt_done_sem == NULL || carrier_sense_sem == NULL) {
        return; 
    }

    rmt_rx_channel_config_t rx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 48, 
        .gpio_num = PIN_GDO0,
        .flags.invert_in = false,
        .flags.with_dma = true
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &rx_channel));
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_callback };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, NULL));
    
    xTaskCreate(rf_rx_task_impl, "rf_rx_task", 8192, NULL, 5, &rf_task_handle);
    
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
    if (spi_mutex == NULL) {
        spi_mutex = xSemaphoreCreateMutex();
    }
    if (spi_mutex == NULL) {
        return; 
    }
    
    spi_init();
    gpio_config_t io_led = { .pin_bit_mask = (1ULL << PIN_LED), .mode = GPIO_MODE_OUTPUT, .pull_up_en = 1 };
    gpio_config(&io_led);
    gpio_set_level(PIN_LED, 1);
    
    cc1101_strobe(CC1101_SRES);
    vTaskDelay(pdMS_TO_TICKS(10));
    for (int i = 0; i < sizeof(erp1_config)/sizeof(cc1101_cfg_t); i++) {
        cc1101_write_reg(erp1_config[i].addr, erp1_config[i].val);
    }
    
    // Expert overrides for TCM515 compatibility
    cc1101_write_reg(0x0D, 0x21); // FREQ2
    cc1101_write_reg(0x0E, 0x65); // FREQ1
    cc1101_write_reg(0x0F, 0x6A); // FREQ0 (868.299 MHz)
    cc1101_write_reg(0x10, 0x2D); // MDMCFG4: 541kHz BW
    cc1101_write_reg(0x11, 0x3B); // MDMCFG3: 250kbps
    cc1101_write_reg(0x12, 0x30); // MDMCFG2: OOK, No HW Sync
    
    cc1101_write_burst(0x3E, patable_ook, 2);
    
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { }
    
    radio_rmt_rx_init();
    
    cc1101_strobe(CC1101_SRX);
    ESP_LOGI(TAG, "CC1101 Initialized in 250kbps OOK mode");
}

static bool check_lbt(void) {
    uint8_t rssi_raw = cc1101_read_status(0x34);
    if (rssi_raw >= 75) return false;
    return true;
}

static uint16_t encode_manchester_byte(uint8_t b) {
    uint16_t result = 0;
    for (int i = 7; i >= 0; i--) {
        if ((b >> i) & 1) result = (result << 2) | 0x02; // 1 -> 10
        else result = (result << 2) | 0x01; // 0 -> 01
    }
    return result;
}

void radio_transmit(const uint8_t *data, uint8_t len) {
    if (len > 31) return; 
    
    uint8_t tx_buf[128];
    uint8_t idx = 0;
    
    // Preamble 2 bytes (0xAA = 10101010 chips)
    tx_buf[idx++] = 0xAA;
    tx_buf[idx++] = 0xAA;
    
    // Sync Word (0xA55A chips)
    tx_buf[idx++] = 0xA5;
    tx_buf[idx++] = 0x5A;
    
    for (uint8_t i = 0; i < len; i++) {
        uint16_t encoded = encode_manchester_byte(data[i]);
        tx_buf[idx++] = (encoded >> 8) & 0xFF;
        tx_buf[idx++] = encoded & 0xFF;
    }

    is_transmitting = true;
    gpio_intr_disable(PIN_GDO2);
    
    cc1101_strobe(CC1101_SIDLE);
    
    // Config for TX: 250kbps OOK, Fixed length, No preamble/sync in HW
    cc1101_write_reg(0x10, 0x2D); 
    cc1101_write_reg(0x11, 0x3B); 
    cc1101_write_reg(0x08, 0x00);        // Fixed length
    cc1101_write_reg(0x06, idx);         // Packet length
    cc1101_write_reg(0x12, 0x30);        // OOK, No HW Sync
    
    for(int i = 0; i < 5; i++) {
        if(check_lbt()) break;
        vTaskDelay(pdMS_TO_TICKS(2 + (esp_random() % 5)));
    }
    
    for(int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE);
        cc1101_strobe(CC1101_SFTX);
        cc1101_write_burst(0x3F, tx_buf, idx);
        cc1101_strobe(CC1101_STX);
        int timeout = 50; 
        while((cc1101_read_status(CC1101_MARCSTATE) & 0x1F) != 0x01 && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            timeout--;
        }
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(10 + (esp_random() % 16)));
    }
    
    // Restore Async RX Mode
    cc1101_write_reg(0x10, 0x2D); 
    cc1101_write_reg(0x08, 0x32); 
    cc1101_write_reg(0x12, 0x30); 
    cc1101_strobe(CC1101_SFRX);
    cc1101_strobe(CC1101_SRX);
    
    gpio_intr_enable(PIN_GDO2);
    is_transmitting = false;
}
