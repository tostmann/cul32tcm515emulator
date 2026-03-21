import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if line.startswith("static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {"):
        skip = True
        new_lines.append("""static void push_manchester_bit(rmt_symbol_word_t *buf, size_t *idx, uint8_t bit) {
    if (bit) { // 1 -> 01 (Low, High)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 0, .duration1 = 32, .level1 = 1 };
    } else {   // 0 -> 10 (High, Low)
        buf[(*idx)++] = (rmt_symbol_word_t){ .duration0 = 32, .level0 = 1, .duration1 = 32, .level1 = 0 };
    }
}
""")
        continue
    if skip:
        if line.startswith("void radio_transmit(const uint8_t *data, uint8_t len) {"):
            skip = False
            # We don't append it yet, we let the normal code process it
        else:
            continue
    new_lines.append(line)

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

