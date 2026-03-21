import re

with open('tests/test_rf_link_v2.py', 'r') as f:
    text = f.read()

text = text.replace(
    'elif pt == 0x33:\n                    pass # Silently drop diagnostic',
    'elif pt == 0x33:\n                    print(f"[{name}] DIAG 0x33: {d.hex()}")'
)

with open('tests/test_rf_link_v2.py', 'w') as f:
    f.write(text)
