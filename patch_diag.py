import os
import re

def patch_esp3_diagnose():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
        
    old = """void esp3_process_byte(uint8_t byte) {"""
    new = """void esp3_process_byte(uint8_t byte) {
    uint8_t diag[2] = {0x00, byte};
    esp3_send_packet(0x80, diag, 2, NULL, 0);"""
    
    if old in content:
        content = content.replace(old, new)
        with open('src/esp3_proto.c', 'w') as f:
            f.write(content)
        print("Patched diag 0x80!")
    else:
        print("Not found in esp3_proto.c")

if __name__ == "__main__":
    patch_esp3_diagnose()
