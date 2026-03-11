#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp3_proto.h"
#include "radio_hal.h"

static const char *TAG = "MAIN";

void usb_rx_task(void *pvParameters) {
    uint8_t rx_buf[128];
    while (1) {
        int bytes_read = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), portMAX_DELAY);
        if (bytes_read > 0) {
            for (int i = 0; i < bytes_read; i++) {
                esp3_process_byte(rx_buf[i]);
            }
        }
    }
}

void app_main(void) {
    // Disable logging to USB CDC to avoid corrupting ESP3 stream
    esp_log_level_set("*", ESP_LOG_NONE);
    
    // Initialize USB Serial JTAG driver
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };
    usb_serial_jtag_driver_install(&usb_config);

    // Initialize ESP3 parser
    esp3_init();
    
    // Initialize Radio HAL (SPI, GPIO, CC1101, RMT RX Task)
    // radio_hal_init();

    // Start USB RX Task
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 5, NULL);
    
    // Main loop does nothing
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
