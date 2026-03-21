import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
in_push_bit = False
for line in lines:
    if "static void erp1_push_bit(erp1_decoder_t *dec, uint8_t bit)" in line:
        in_push_bit = True
        new_lines.append("static void erp1_push_bit(erp1_decoder_t *dec, uint8_t raw_bit) {\n")
        new_lines.append("    uint8_t bit = !raw_bit;\n")
        continue
    if in_push_bit:
        if line.strip() == "}":
            in_push_bit = False
            new_lines.append("}\n")
        else:
            new_lines.append(line)
        continue
    new_lines.append(line)

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print("erp1_push_bit patched.")
