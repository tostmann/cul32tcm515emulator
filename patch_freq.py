import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("cc1101_write_reg(0x0D, 0x21); cc1101_write_reg(0x0E, 0x65); cc1101_write_reg(0x0F, 0x6A);", "cc1101_write_reg(0x0D, 0x21); cc1101_write_reg(0x0E, 0x64); cc1101_write_reg(0x0F, 0x90);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
