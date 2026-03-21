import re

with open('src/radio_hal.c', 'r') as f:
    text = f.read()

# Replace ESP_LOGI with direct esp3 packets for diag
patch = """
    // --- HARDWARE VERIFICATION TEST ---
    uint8_t partnum = cc1101_read_status(0x30);
    uint8_t version = cc1101_read_status(0x31);
    uint8_t d0[2] = { partnum, version };
    esp3_send_packet(0x70, d0, 2, NULL, 0); // DIAG 0x70
    
    gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_GDO2, GPIO_MODE_INPUT);
    
    cc1101_write_reg(0x00, 0x2F); vTaskDelay(2); // HW 0
    uint8_t d1[1] = { gpio_get_level(PIN_GDO0) };
    esp3_send_packet(0x71, d1, 1, NULL, 0); // DIAG 0x71
    
    cc1101_write_reg(0x00, 0x6F); vTaskDelay(2); // HW 1
    uint8_t d2[1] = { gpio_get_level(PIN_GDO0) };
    esp3_send_packet(0x72, d2, 1, NULL, 0); // DIAG 0x72
    
    cc1101_write_reg(0x02, 0x2F); vTaskDelay(2); // HW 0
    uint8_t d3[1] = { gpio_get_level(PIN_GDO2) };
    esp3_send_packet(0x73, d3, 1, NULL, 0); // DIAG 0x73
    
    cc1101_write_reg(0x02, 0x6F); vTaskDelay(2); // HW 1
    uint8_t d4[1] = { gpio_get_level(PIN_GDO2) };
    esp3_send_packet(0x74, d4, 1, NULL, 0); // DIAG 0x74
    // --- END VERIFICATION ---
"""

# Find the block and replace it
start = text.find('// --- HARDWARE VERIFICATION TEST ---')
end = text.find('// --- END VERIFICATION ---') + len('// --- END VERIFICATION ---')
text = text[:start] + patch + text[end:]

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
