#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp3_proto.h"
#include "radio_hal.h"
#include "enocean_nvs.h"

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
        int tcm_avail = TCMSerial_available();
        if (tcm_avail > 0) {
            if (tcm_avail > sizeof(buf)) tcm_avail = sizeof(buf);
            for (int i = 0; i < tcm_avail; i++) {
                buf[i] = (uint8_t)TCMSerial_read();
            }
            usb_serial_jtag_write_bytes(buf, tcm_avail, 0);
        }

        // Slight delay to prevent watchdog issues and excessive CPU usage in the tight loop
        vTaskDelay(1);
    }
}
