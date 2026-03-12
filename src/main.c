#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "enocean_stream.h"

// Factory functions
extern enocean_stream_t* get_emulator_stream(void);
extern enocean_stream_t* get_uart_stream(int port, int tx_pin, int rx_pin);

// SWITCH HERE
#define USE_EMULATOR 1

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_NONE);
    usb_serial_jtag_driver_config_t usb_config = { .tx_buffer_size = 2048, .rx_buffer_size = 2048 };
    usb_serial_jtag_driver_install(&usb_config);

    enocean_stream_t *tcm_stream;

#if USE_EMULATOR
    tcm_stream = get_emulator_stream();
#else
    // Example for real TCM: UART 1, GPIO 4 TX, GPIO 5 RX
    tcm_stream = get_uart_stream(1, 4, 5);
#endif

    STREAM_INIT(tcm_stream, 57600);

    uint8_t buf[256];
    while (1) {
        int usb_len = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
        if (usb_len > 0) {
            STREAM_WRITE_BUF(tcm_stream, buf, usb_len);
        }

        size_t tcm_len = STREAM_READ_BUF(tcm_stream, buf, sizeof(buf));
        if (tcm_len > 0) {
            usb_serial_jtag_write_bytes(buf, tcm_len, 0);
        }

        vTaskDelay(1);
    }
}
