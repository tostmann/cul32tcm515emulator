import os
import re

def patch_esp3():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
        
    old = '''                    if (current_packet.packet_type == ESP3_TYPE_RADIO_ERP1) {
                        radio_transmit(current_packet.data, current_packet.data_len);
                        esp3_send_response(RET_OK);
                    }'''
                    
    new = '''                    if (current_packet.packet_type == ESP3_TYPE_RADIO_ERP1) {
                        uint8_t pre_tx = 0xAA;
                        esp3_send_packet(0x75, &pre_tx, 1, NULL, 0); // DIAG BEFORE TX
                        radio_transmit(current_packet.data, current_packet.data_len);
                        uint8_t post_tx = 0xBB;
                        esp3_send_packet(0x75, &post_tx, 1, NULL, 0); // DIAG AFTER TX
                        esp3_send_response(RET_OK);
                    }'''
    
    if old in content:
        content = content.replace(old, new)
        with open('src/esp3_proto.c', 'w') as f:
            f.write(content)
        print("Patched!")
    else:
        print("Not found in esp3_proto.c")

if __name__ == "__main__":
    patch_esp3()
