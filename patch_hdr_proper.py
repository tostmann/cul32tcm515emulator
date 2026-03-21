import os

def patch_hdr_diag_proper():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')

    old_hdr_crc = """        case STATE_CRC8H:
            if (byte == calc_buffer_crc(header_buf, 4)) {"""
    new_hdr_crc = """        case STATE_CRC8H:
            {
                uint8_t expected_crc = calc_buffer_crc(header_buf, 4);
                if (byte == expected_crc) {
                    esp3_send_response(0x11); // HDR CRC OK"""
    content = content.replace(old_hdr_crc, new_hdr_crc)
    
    old_hdr_fail = """            } else {
                rx_state = STATE_SYNC;
            }
            break;"""
    new_hdr_fail = """            } else {
                uint8_t diag[2] = {expected_crc, byte};
                esp3_send_packet(0x88, diag, 2, NULL, 0); // HDR CRC FAIL
                rx_state = STATE_SYNC;
            }
            } // END SCOPE
            break;"""
    content = content.replace(old_hdr_fail, new_hdr_fail)

    with open('src/esp3_proto.c', 'w') as f:
        f.write(content)
    print("Patched correctly!")

if __name__ == "__main__":
    patch_hdr_diag_proper()
