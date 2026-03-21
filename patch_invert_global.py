import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Swap Manchester mapping to test the other polarity globally
old_push = """static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {
    if (bit) { // 1 -> 01 (Low, High)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 0, .duration1 = 32, .level1 = 1 };
    } else {   // 0 -> 10 (High, Low)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 1, .duration1 = 32, .level1 = 0 };
    }
}"""

new_push = """static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {
    if (bit) { // 1 -> 10 (High, Low)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 1, .duration1 = 32, .level1 = 0 };
    } else {   // 0 -> 01 (Low, High)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 0, .duration1 = 32, .level1 = 1 };
    }
}"""

content = content.replace(old_push, new_push)

# Make Preamble 4x 0 and Sync 1x 1
content = content.replace("for (int i = 0; i < 4; i++) push_manchester_bit(tx_symbols, &s_idx, 1);\n    // Sync bit: 1 bit of EnOcean '0' (Low-High)\n    push_manchester_bit(tx_symbols, &s_idx, 0);", "for (int i = 0; i < 4; i++) push_manchester_bit(tx_symbols, &s_idx, 0);\n    // Sync bit: 1 bit of EnOcean '1' (Low-High)\n    push_manchester_bit(tx_symbols, &s_idx, 1);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
