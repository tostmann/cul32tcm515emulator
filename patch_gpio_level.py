import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("cc1101_strobe(CC1101_STX);", "gpio_set_level(PIN_GDO0, 0); // Force idle low\n        cc1101_strobe(CC1101_STX);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
