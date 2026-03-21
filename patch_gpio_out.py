import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("//gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);", "gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
