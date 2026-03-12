#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "TCMSerial.h"

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_NONE);

    // Initialize USB CDC (Serial)
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 2048,
        .rx_buffer_size = 2048,
    };
    usb_serial_jtag_driver_install(&usb_config);

    // Initialize TCM Emulator Library
    TCMSerial_begin(57600);

    uint8_t buf[256];
    while (1) {
        // 1. Read from USB CDC and write to TCMSerial
        int usb_len = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
        if (usb_len > 0) {
            TCMSerial_write_buf(buf, usb_len);
        }

        // 2. Read from TCMSerial and write to USB CDC
        size_t tcm_len = TCMSerial_read_buf(buf, sizeof(buf));
        if (tcm_len > 0) {
            usb_serial_jtag_write_bytes(buf, tcm_len, 0);
        }

        // Slight delay to prevent watchdog issues and excessive CPU usage
        vTaskDelay(1);
    }
}
