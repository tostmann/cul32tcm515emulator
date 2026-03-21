import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the delay
content = content.replace("cc1101_strobe(CC1101_STX);\n        esp_rom_delay_us(250);", "cc1101_strobe(CC1101_STX);")

# Add the 500us LOW pulse at the start of tx_symbols
# Find `size_t s_idx = 0;` and insert after
content = content.replace("size_t s_idx = 0;\n    // Preamble:", "size_t s_idx = 0;\n    // PLL Settling Time (500us LOW)\n    tx_symbols[s_idx++] = (rmt_symbol_word_t){ .duration0 = 4000, .level0 = 0, .duration1 = 4000, .level1 = 0 };\n    // Preamble:")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
