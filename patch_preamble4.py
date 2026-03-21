import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Change Preamble bits and Sync bit!
content = content.replace("for (int i = 0; i < 4; i++) push_manchester_bit(tx_symbols, &s_idx, 0);\n    // Sync bit: 1 bit of EnOcean '1' (High-Low)\n    push_manchester_bit(tx_symbols, &s_idx, 1);", "for (int i = 0; i < 4; i++) push_manchester_bit(tx_symbols, &s_idx, 1);\n    // Sync bit: 1 bit of EnOcean '0' (Low-High)\n    push_manchester_bit(tx_symbols, &s_idx, 0);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
