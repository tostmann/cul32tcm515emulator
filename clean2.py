import sys

with open('src/esp3_proto.c', 'r', encoding='ascii') as f:
    content = f.read()

content = content.replace("                esp3_send_response(0x42); // ALWAYS RESPOND\n", "")
content = content.replace("                         // DIAG BEFORE TX\n", "")
content = content.replace("                         // DIAG AFTER TX\n", "")

with open('src/esp3_proto.c', 'w', encoding='ascii') as f:
    f.write(content)

print("esp3_proto.c cleaned up.")
