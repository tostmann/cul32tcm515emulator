import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("tx_symbols[s_idx++] = (rmt_symbol_word_t){ .duration0 = 100, .level0 = 0, .duration1 = 100, .level1 = 0 };", "tx_symbols[s_idx++] = (rmt_symbol_word_t){ .duration0 = 400, .level0 = 0, .duration1 = 400, .level1 = 0 };")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
