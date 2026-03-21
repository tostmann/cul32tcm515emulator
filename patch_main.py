import os

def patch_main():
    with open('src/main.c', 'r') as f:
        content = f.read()
        
    old = """        size_t tcm_len = STREAM_READ_BUF(tcm_stream, buf, sizeof(buf));
        if (tcm_len > 0) {
            usb_serial_jtag_write_bytes(buf, tcm_len, 0);
        }

        vTaskDelay(1);"""
        
    new = """        size_t tcm_len = STREAM_READ_BUF(tcm_stream, buf, sizeof(buf));
        if (tcm_len > 0) {
            usb_serial_jtag_write_bytes(buf, tcm_len, 0);
            usb_serial_jtag_write_bytes(NULL, 0, 0); // Flush or trigger push
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        vTaskDelay(pdMS_TO_TICKS(1));"""
        
    if old in content:
        content = content.replace(old, new)
        with open('src/main.c', 'w') as f:
            f.write(content)
        print("Patched main!")
    else:
        print("Not found in main.c")

if __name__ == "__main__":
    patch_main()
