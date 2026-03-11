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

void rf_rx_task(void *pvParameters) {
    uint8_t fifo_buf[64];
    while (1) {
        if (xSemaphoreTake(rf_rx_semaphore, portMAX_DELAY) == pdTRUE) {
            // Read MARCSTATE to ensure we are still in RX or just finished
            // In ERP1 Packet Mode, CC1101 should have a full packet in FIFO
            uint8_t rxbytes = cc1101_read_status(0x3B) & 0x7F; // RXBYTES
            
            if (rxbytes > 0 && rxbytes <= 64) {
                cc1101_read_burst(0x3F, fifo_buf, rxbytes);
                
                // Construct ESP3 Optional Data (7 bytes for TCM515)
                // SubTelNum(1), DestID(4), dBm(1), SecurityLevel(1)
                uint8_t rssi_raw = cc1101_read_status(0x34);
                int dbm = (rssi_raw >= 128) ? (rssi_raw - 256) / 2 - 74 : rssi_raw / 2 - 74;
                
                uint8_t opt_data[7] = {
                    0x01,       // SubTelNum
                    0xFF, 0xFF, 0xFF, 0xFF, // Destination ID (Broadcast)
                    (uint8_t)(-dbm), // dBm (absolute value)
                    0x00        // Security Level
                };
                
                esp3_send_packet(ESP3_TYPE_RADIO_ERP1, fifo_buf, rxbytes, opt_data, 7);
            }
            
            // Re-arm RX
            cc1101_strobe(0x36); // SIDLE
            cc1101_strobe(0x3A); // SFRX
            cc1101_strobe(0x34); // SRX
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
    
    // Initialize Radio HAL (SPI, GPIO, CC1101)
    radio_hal_init();

    // Start Tasks
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 5, NULL);
    xTaskCreate(rf_rx_task, "rf_rx", 4096, NULL, 6, NULL);
    
    // Main loop does nothing
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
