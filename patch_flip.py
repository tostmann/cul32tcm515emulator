import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Flip push_manchester_bit polarity back
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

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
