import os

def patch_main_flush():
    with open('src/main.c', 'r') as f:
        content = f.read()
        
    old = """        int usb_len = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
        if (usb_len > 0) {
            STREAM_WRITE_BUF(tcm_stream, buf, usb_len);
        }"""
        
    new = """        int usb_len = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
        if (usb_len > 0) {
            STREAM_WRITE_BUF(tcm_stream, buf, usb_len);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1)); // Wait
        }"""
        
    if old in content:
        content = content.replace(old, new)
        with open('src/main.c', 'w') as f:
            f.write(content)
        print("Patched main flush loop!")
    else:
        print("Not found in main.c")

if __name__ == "__main__":
    patch_main_flush()
