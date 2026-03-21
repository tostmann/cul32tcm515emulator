import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Switch GDO0 to High-Z before TX loop
content = content.replace("is_transmitting = true;", "is_transmitting = true;\n    cc1101_write_reg(0x00, 0x2E); // GDO0 to High-Z (Input for TX)")

# Switch GDO0 back to Output after TX loop
content = content.replace("cc1101_strobe(CC1101_SIDLE);\n    cc1101_strobe(CC1101_SRX);\n    //gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);\n    is_transmitting = false;", "cc1101_strobe(CC1101_SIDLE);\n    cc1101_write_reg(0x00, 0x0D); // GDO0 back to Output for RX\n    cc1101_strobe(CC1101_SRX);\n    //gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);\n    is_transmitting = false;")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
