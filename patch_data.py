import os

def patch_data_diag():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')

    old_data = """        case STATE_DATA:
            payload_static_buf[payload_idx++] = byte;
            if (payload_idx == (current_packet.data_len + current_packet.opt_len)) {
                rx_state = STATE_CRC8D;
            }
            break;"""
    new_data = """        case STATE_DATA:
            payload_static_buf[payload_idx++] = byte;
            if (payload_idx == (current_packet.data_len + current_packet.opt_len)) {
                rx_state = STATE_CRC8D;
                esp3_send_response(0xdd); // REACHED END OF DATA
            }
            break;"""
    content = content.replace(old_data, new_data)

    with open('src/esp3_proto.c', 'w') as f:
        f.write(content)
    print("Patched DATA Diag!")

if __name__ == "__main__":
    patch_data_diag()
