import re
with open('src/radio_hal.c', 'r') as f:
    text = f.read()

# Replace ESP_LOGI with esp3_send_packet
text = text.replace(
    'ESP_LOGI("HAL", "CS Trigger! GDO2=%d", gpio_get_level(PIN_GDO2));',
    '''uint8_t d1[1] = { gpio_get_level(PIN_GDO2) };
            esp3_send_packet(0x33, d1, 1, NULL, 0);'''
)

text = text.replace(
    'ESP_LOGI("HAL", "No CS. GDO2=%d", gpio_get_level(PIN_GDO2));',
    '''uint8_t d2[1] = { gpio_get_level(PIN_GDO2) + 10 }; // +10 to distinguish
            esp3_send_packet(0x33, d2, 1, NULL, 0);'''
)

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
