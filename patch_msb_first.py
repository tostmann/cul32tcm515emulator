import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Revert to MSB first
content = content.replace("for (int b = 0; b <= 7; b++) {\n            push_manchester_bit(tx_symbols, &s_idx, (data[i] >> b) & 1);", "for (int b = 7; b >= 0; b--) {\n            push_manchester_bit(tx_symbols, &s_idx, (data[i] >> b) & 1);")

content = content.replace("for (int b = 0; b <= 7; b++) {\n        push_manchester_bit(tx_symbols, &s_idx, (checksum >> b) & 1);", "for (int b = 7; b >= 0; b--) {\n        push_manchester_bit(tx_symbols, &s_idx, (checksum >> b) & 1);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
