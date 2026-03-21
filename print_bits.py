import sys

def print_bits(hex_str):
    b = bytes.fromhex(hex_str)
    res = ""
    for byte in b:
        for i in range(7, -1, -1):
            bit = (byte >> i) & 1
            res += "1" if bit else "0"
    print(res)

print_bits("A50000000801234567007D")
