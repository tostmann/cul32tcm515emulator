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
    esp_log_level_set("*", ESP_LOG_NONE);
    
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024,
    };
    usb_serial_jtag_driver_install(&usb_config);

    esp3_init();
    radio_hal_init();

    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 5, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
