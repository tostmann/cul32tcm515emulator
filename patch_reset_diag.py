import os

def patch_esp3_reset():
    with open('src/esp3_proto.c', 'r') as f:
        content = f.read()
        
    old = """void esp3_process_byte(uint8_t byte) {
    uint8_t diag[2] = {0x00, byte};
    esp3_send_packet(0x80, diag, 2, NULL, 0);"""
    
    new = """void esp3_process_byte(uint8_t byte) {"""
    
    if old in content:
        content = content.replace(old, new)
        with open('src/esp3_proto.c', 'w') as f:
            f.write(content)
        print("Removed diag 0x80!")
    else:
        print("Not found in esp3_proto.c")

if __name__ == "__main__":
    patch_esp3_reset()
