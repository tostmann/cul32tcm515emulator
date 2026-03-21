import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# I will comment out gpio_set_direction just to test if RMT output is the issue
content = content.replace("gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);", "//gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);")
content = content.replace("gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);", "//gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)

