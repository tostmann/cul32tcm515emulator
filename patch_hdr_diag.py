import os

def patch_esp3_diag_states():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')

    # Add state debug prints or ESP responses
    new_content = content
    # We can just send a response when we reach each state, but that might mess up the buffer or be too much.
    # Let's add a response on Header CRC fail, Header CRC pass, etc.
    
    # Header CRC check
    old_hdr_crc = """            if (byte == expected_crc) {
                rx_state = STATE_DATA;"""
    new_hdr_crc = """            if (byte == expected_crc) {
                esp3_send_response(0x11); // HDR CRC OK
                rx_state = STATE_DATA;"""
    new_content = new_content.replace(old_hdr_crc, new_hdr_crc)
    
    old_hdr_fail = """            } else {
                rx_state = STATE_SYNC; // CRC failed
            }
            break;"""
    new_hdr_fail = """            } else {
                uint8_t diag[2] = {expected_crc, byte};
                esp3_send_packet(0x88, diag, 2, NULL, 0); // HDR CRC FAIL
                rx_state = STATE_SYNC; // CRC failed
            }
            break;"""
    new_content = new_content.replace(old_hdr_fail, new_hdr_fail)

    with open('src/esp3_proto.c', 'w') as f:
        f.write(new_content)
    print("Patched Header CRC Diag!")

if __name__ == "__main__":
    patch_esp3_diag_states()
