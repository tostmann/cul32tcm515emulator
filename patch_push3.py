import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if line.startswith("static void erp1_push_bit(erp1_decoder_t *dec, uint8_t bit) {"):
        skip = True
        new_lines.append("""static void erp1_push_bit(erp1_decoder_t *dec, uint8_t raw_bit) {
    uint8_t bit = !raw_bit;
    if (dec->state == ERP1_STATE_SYNC) {
        if (bit == 0) {
            dec->preamble_bits++;
        } else if (bit == 1) {
            if (dec->preamble_bits >= ERP1_MIN_PREAMBLE) {
                dec->state = ERP1_STATE_DATA;
                dec->byte_idx = 0;
                dec->bit_idx = 0;
                memset(dec->buffer, 0, sizeof(dec->buffer));
            } else {
                erp1_decoder_reset(dec);
            }
        }
    } else if (dec->state == ERP1_STATE_DATA) {
        if (dec->byte_idx < ERP1_MAX_PAYLOAD) {
            if (bit) dec->buffer[dec->byte_idx] |= (1 << (7 - dec->bit_idx));
            dec->bit_idx++;
            if (dec->bit_idx >= 8) {
                dec->bit_idx = 0;
                dec->byte_idx++;
            }
        } else {
            erp1_decoder_reset(dec);
        }
    }
}
""")
        continue
    if skip:
        if line.startswith("static void erp1_decode_pulse"):
            skip = False
            new_lines.append(line)
        continue
    new_lines.append(line)

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
