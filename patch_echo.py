import os
import re

def patch_main_echo():
    with open('src/main.c', 'r') as f:
        content = f.read()
        
    old = """        int usb_len = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
        if (usb_len > 0) {
            STREAM_WRITE_BUF(tcm_stream, buf, usb_len);
        }"""
        
    new = """        int usb_len = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
        if (usb_len > 0) {
            usb_serial_jtag_write_bytes(buf, usb_len, 0); // ECHO
            STREAM_WRITE_BUF(tcm_stream, buf, usb_len);
        }"""
        
    if old in content:
        content = content.replace(old, new)
        with open('src/main.c', 'w') as f:
            f.write(content)
        print("Patched echo!")
    else:
        print("Not found in main.c")

if __name__ == "__main__":
    patch_main_echo()
