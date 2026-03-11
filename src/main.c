#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "radio_hal.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "TCM-Emu-Gemini Starting...");

    // Initialize Radio HAL (includes LED and SPI/CC1101 init)
    radio_hal_init();

    while (1) {
        static bool led_state = false;
        
        // Toggle LED
        gpio_set_level(PIN_LED, led_state);
        led_state = !led_state;

        vTaskDelay(pdMS_TO_TICKS(1000));

        // CC1101 Status auslesen (0x35 = MARCSTATE)
        uint8_t marcstate = cc1101_read_status(0x35);
        ESP_LOGI(TAG, "CC1101 MARCSTATE: 0x%02X", marcstate);
    }
}
