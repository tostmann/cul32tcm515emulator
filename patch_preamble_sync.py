import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix Preamble and Sync back to 10 (0) and 01 (1)
content = content.replace("for (int i = 0; i < 16; i++) push_manchester_bit(tx_symbols, &s_idx, 1);\n    // Sync bit: 1 bit of EnOcean '0' (Low-High)\n    push_manchester_bit(tx_symbols, &s_idx, 0);", "for (int i = 0; i < 4; i++) push_manchester_bit(tx_symbols, &s_idx, 0);\n    // Sync bit: 1 bit of EnOcean '1' (Low-High)\n    push_manchester_bit(tx_symbols, &s_idx, 1);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
