import re

with open('tests/test_rf_link_v2.py', 'r') as f:
    text = f.read()

# Add debug parser
patch = '''elif pt == 0x70:
                    print(f"[{name}] SPI VERIFY: PARTNUM={d[0]:02X}, VERSION={d[1]:02X}")
                elif pt == 0x71:
                    print(f"[{name}] GDO0 HW0 -> {d[0]}")
                elif pt == 0x72:
                    print(f"[{name}] GDO0 HW1 -> {d[0]}")
                elif pt == 0x73:
                    print(f"[{name}] GDO2 HW0 -> {d[0]}")
                elif pt == 0x74:
                    print(f"[{name}] GDO2 HW1 -> {d[0]}")'''

text = text.replace('elif pt == 0x33:\n                    print(f"[{name}] DIAG 0x33: {d.hex()}")', patch + '\n                elif pt == 0x33:\n                    print(f"[{name}] DIAG 0x33: {d.hex()}")')

with open('tests/test_rf_link_v2.py', 'w') as f:
    f.write(text)
