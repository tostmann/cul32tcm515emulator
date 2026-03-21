import re

with open('src/esp3_proto.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the ALWAYS RESPOND
content = re.sub(r'\s*esp3_send_response\(0x42\); // ALWAYS RESPOND\n', '', content)
# Remove DIAG BEFORE TX
content = re.sub(r'\s*// DIAG BEFORE TX\n', '', content)
content = re.sub(r'\s*uint8_t pre_tx = 0xAA;\n\s*esp3_send_packet\(0x75, &pre_tx, 1, NULL, 0\);\n', '', content)
# Remove DIAG AFTER TX
content = re.sub(r'\s*// DIAG AFTER TX\n', '', content)
content = re.sub(r'\s*uint8_t post_tx = 0xBB;\n\s*esp3_send_packet\(0x75, &post_tx, 1, NULL, 0\);\n', '', content)

with open('src/esp3_proto.c', 'w', encoding='utf-8') as f:
    f.write(content)

print("esp3_proto.c cleaned up via Python regex.")
