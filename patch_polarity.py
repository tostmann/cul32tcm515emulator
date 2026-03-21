import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace push_manchester_bit
old_push = """static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {
    if (bit) { // 1 -> 10 (High, Low)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 1, .duration1 = 32, .level1 = 0 };
    } else {   // 0 -> 01 (Low, High)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 0, .duration1 = 32, .level1 = 1 };
    }
}"""

new_push = """static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {
    if (bit) { // 1 -> 01 (Low, High)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 0, .duration1 = 32, .level1 = 1 };
    } else {   // 0 -> 10 (High, Low)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 1, .duration1 = 32, .level1 = 0 };
    }
}"""

if old_push in content:
    content = content.replace(old_push, new_push)

# Preamble should be 16x 0, Sync should be 1x 1
# Let's just rewrite the preamble/sync part to be safe
content = content.replace("for (int i = 0; i < 16; i++) push_manchester_bit(tx_symbols, &s_idx, 0);", "for (int i = 0; i < 16; i++) push_manchester_bit(tx_symbols, &s_idx, 0);")
content = content.replace("push_manchester_bit(tx_symbols, &s_idx, 1);", "push_manchester_bit(tx_symbols, &s_idx, 1);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
