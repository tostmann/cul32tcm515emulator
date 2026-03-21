import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("for (int i = 0; i < 8; i++) push_manchester_bit(tx_symbols, &s_idx, 1);", "for (int i = 0; i < 16; i++) push_manchester_bit(tx_symbols, &s_idx, 1);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
