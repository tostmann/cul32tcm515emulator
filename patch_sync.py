import os

def patch_esp3_sync_diag():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')

    old_sync = """        case STATE_SYNC:
            if (byte == ESP3_SYNC_BYTE) {
                header_idx = 0;
                rx_state = STATE_HEADER;
            }
            break;"""
    new_sync = """        case STATE_SYNC:
            if (byte == ESP3_SYNC_BYTE) {
                header_idx = 0;
                rx_state = STATE_HEADER;
                esp3_send_response(0x99); // SYNC HIT
            }
            break;"""
    content = content.replace(old_sync, new_sync)

    with open('src/esp3_proto.c', 'w') as f:
        f.write(content)
    print("Patched SYNC Diag!")

if __name__ == "__main__":
    patch_esp3_sync_diag()
