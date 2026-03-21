import re

with open('src/esp3_proto.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(r'\s*esp3_send_response\(0x99\);.*?\n', '\n', content)
content = re.sub(r'\s*esp3_send_response\(0x11\);.*?\n', '\n', content)
content = re.sub(r'\s*esp3_send_response\(0xdd\);.*?\n', '\n', content)

with open('src/esp3_proto.c', 'w', encoding='utf-8') as f:
    f.write(content)

print("esp3_proto.c cleaned up more.")
