import os
with open("src/esp3_proto.c", "r", encoding="utf-8", errors="replace") as f:
    lines = f.readlines()
    for i, line in enumerate(lines):
        if "crc8_table" in line:
            print(f"Line {i}: {line.strip()}")
