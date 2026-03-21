import os
import re

def patch_esp3_reset_2():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
        
    old = """        case STATE_CRC8D:
            {
                uint16_t total_len = current_packet.data_len + current_packet.opt_len;
                uint8_t expected_crc = calc_buffer_crc(payload_static_buf, total_len);
                if (byte == expected_crc) {"""
                
    new = """        case STATE_CRC8D:
            {
                uint16_t total_len = current_packet.data_len + current_packet.opt_len;
                uint8_t expected_crc = calc_buffer_crc(payload_static_buf, total_len);
                esp3_send_response(0x42); // ALWAYS RESPOND
                if (byte == expected_crc) {"""
                
    if old in content:
        content = content.replace(old, new)
        with open('src/esp3_proto.c', 'w') as f:
            f.write(content)
        print("Patched CRC DIAG!")
    else:
        print("Not found in esp3_proto.c")

if __name__ == "__main__":
    patch_esp3_reset_2()
