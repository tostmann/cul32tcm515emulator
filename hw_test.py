import re

with open('src/radio_hal.c', 'r') as f:
    text = f.read()

patch = """
    cc1101_strobe(CC1101_SRES); vTaskDelay(10);
    
    // --- HARDWARE VERIFICATION TEST ---
    uint8_t partnum = cc1101_read_status(0x30);
    uint8_t version = cc1101_read_status(0x31);
    ESP_LOGI("HW_TEST", "SPI Link: PARTNUM=0x%02X, VERSION=0x%02X", partnum, version);
    
    gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_GDO2, GPIO_MODE_INPUT);
    
    cc1101_write_reg(0x00, 0x2F); vTaskDelay(2); // 0x2F = HW to 0
    ESP_LOGI("HW_TEST", "GDO0 (PIN %d) set to HW 0 -> reads %d", PIN_GDO0, gpio_get_level(PIN_GDO0));
    cc1101_write_reg(0x00, 0x6F); vTaskDelay(2); // 0x6F = HW to 1
    ESP_LOGI("HW_TEST", "GDO0 (PIN %d) set to HW 1 -> reads %d", PIN_GDO0, gpio_get_level(PIN_GDO0));
    
    cc1101_write_reg(0x02, 0x2F); vTaskDelay(2); // 0x2F = HW to 0
    ESP_LOGI("HW_TEST", "GDO2 (PIN %d) set to HW 0 -> reads %d", PIN_GDO2, gpio_get_level(PIN_GDO2));
    cc1101_write_reg(0x02, 0x6F); vTaskDelay(2); // 0x6F = HW to 1
    ESP_LOGI("HW_TEST", "GDO2 (PIN %d) set to HW 1 -> reads %d", PIN_GDO2, gpio_get_level(PIN_GDO2));
    // --- END VERIFICATION ---
"""

text = text.replace("cc1101_strobe(CC1101_SRES); vTaskDelay(10);", patch)

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
