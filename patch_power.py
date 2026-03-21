import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("static const uint8_t patable_ook[] = {0x00, 0x50};", "static const uint8_t patable_ook[] = {0x00, 0xC0};")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
