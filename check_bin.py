import sys

with open('src/esp3_proto.c', 'rb') as f:
    data = f.read()

for i, b in enumerate(data):
    if b > 127 or b < 9 and b != 9 and b != 10 and b != 13:
        print(f"Non-ascii at index {i}: {hex(b)}")
