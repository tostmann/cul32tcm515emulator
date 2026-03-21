import os

def patch_esp3_buffer_diag():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
        
    old = """void esp3_process_byte(uint8_t byte) {"""
    
    new = """void esp3_process_byte(uint8_t byte) {
    if (rx_state == STATE_SYNC && byte == ESP3_SYNC_BYTE) {
         esp3_send_response(0x99); // DIAG: Got Sync!
    }"""
    
    if old in content:
        content = content.replace(old, new)
        with open('src/esp3_proto.c', 'w') as f:
            f.write(content)
        print("Patched diag sync!")
    else:
        print("Not found in esp3_proto.c")

if __name__ == "__main__":
    patch_esp3_buffer_diag()
