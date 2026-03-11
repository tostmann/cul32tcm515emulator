#include "radio_hal.h"
#include "cc1101_regs.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RADIO_HAL";
static spi_device_handle_t spi_handle;

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
        .clock_speed_hz = 5000000, // 5 MHz
        .mode = 0,                 // SPI mode 0
        .spics_io_num = PIN_SS,
        .queue_size = 3,
        .flags = 0
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));
}

static void gpio_init(void) {
    gpio_config_t io_out = {
        .pin_bit_mask = (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_out);
    gpio_set_level(PIN_LED, 1); // Turn off

    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_GDO0) | (1ULL << PIN_GDO2) | (1ULL << PIN_SWITCH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE 
    };
    gpio_config(&io_in);
}

void cc1101_strobe(uint8_t cmd) {
    spi_transaction_t t = {
        .length = 8,
        .tx_data = {cmd},
        .flags = SPI_TRANS_USE_TXDATA
    };
    spi_device_polling_transmit(spi_handle, &t);
}

void cc1101_write_reg(uint8_t addr, uint8_t value) {
    uint8_t tx_data[2] = { addr, value };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data
    };
    spi_device_polling_transmit(spi_handle, &t);
}

uint8_t cc1101_read_reg(uint8_t addr) {
    uint8_t tx_data[2] = { (uint8_t)(addr | 0x80), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };
    spi_device_polling_transmit(spi_handle, &t);
    return rx_data[1];
}

uint8_t cc1101_read_status(uint8_t addr) {
    uint8_t tx_data[2] = { (uint8_t)(addr | 0xC0), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };
    spi_device_polling_transmit(spi_handle, &t);
    return rx_data[1];
}

void cc1101_write_burst(uint8_t addr, const uint8_t *data, uint8_t len) {
    uint8_t header = addr | 0x40;
    spi_transaction_t t = {
        .length = 8,
        .tx_data = {header},
        .flags = SPI_TRANS_USE_TXDATA
    };
    spi_device_polling_transmit(spi_handle, &t);

    spi_transaction_t t_data = {
        .length = len * 8,
        .tx_buffer = data
    };
    spi_device_polling_transmit(spi_handle, &t_data);
}

void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len) {
    uint8_t header = addr | 0xC0;
    spi_transaction_t t = {
        .length = 8,
        .tx_data = {header},
        .flags = SPI_TRANS_USE_TXDATA
    };
    spi_device_polling_transmit(spi_handle, &t);

    spi_transaction_t t_data = {
        .length = len * 8,
        .rx_buffer = data
    };
    spi_device_polling_transmit(spi_handle, &t_data);
}

void radio_hal_init(void) {
    gpio_init();
    spi_init();

    cc1101_strobe(CC1101_SRES);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t partnum = cc1101_read_status(CC1101_PARTNUM);
    uint8_t version = cc1101_read_status(CC1101_VERSION);
    ESP_LOGI(TAG, "CC1101 Found - Part: 0x%02X, Version: 0x%02X", partnum, version);

    for (int i = 0; i < sizeof(erp1_config)/sizeof(cc1101_cfg_t); i++) {
        cc1101_write_reg(erp1_config[i].addr, erp1_config[i].val);
    }
    
    cc1101_write_burst(0x3E, patable_ook, 2);

    cc1101_strobe(CC1101_SRX);
    ESP_LOGI(TAG, "CC1101 Initialized and in RX Mode");
}

void radio_hal_enable_rx_isr(void (*isr_callback)(void*), void* arg) {
    gpio_install_isr_service(0);
    gpio_set_intr_type(PIN_GDO0, GPIO_INTR_NEGEDGE); 
    gpio_isr_handler_add(PIN_GDO0, isr_callback, arg);
}
