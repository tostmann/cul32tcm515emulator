import os
import re

def patch_esp3_crcd():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
        
    old = """                uint8_t expected_crc = calc_buffer_crc(payload_static_buf, total_len);
                uint8_t diag_crc[2] = {expected_crc, byte};
                esp3_send_packet(0x76, diag_crc, 2, NULL, 0); // DIAG CRC
                if (byte == expected_crc) {"""
                
    new = """                uint8_t expected_crc = calc_buffer_crc(payload_static_buf, total_len);
                if (byte == expected_crc) {"""
                
    if old in content:
        content = content.replace(old, new)
        with open('src/esp3_proto.c', 'w') as f:
            f.write(content)
        print("Removed CRC diag!")
    else:
        print("Not found in esp3_proto.c")

if __name__ == "__main__":
    patch_esp3_crcd()
