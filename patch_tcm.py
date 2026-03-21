import os
import re

def patch_esp3_rx_buffer():
    with open('src/TCMSerial.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
        
    old = """// Internal function called by esp3_proto or radio_hal to send data back to the host
void TCMSerial_internal_push(const uint8_t *data, size_t len) {
    if (!buffer_mutex) return;
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (size_t i = 0; i < len; i++) {
        size_t next = (rx_head + 1) % RX_BUFFER_SIZE;
        if (next != rx_tail) {
            rx_buffer[rx_head] = data[i];
            rx_head = next;
        }
    }
    xSemaphoreGive(buffer_mutex);
}"""
    
    new = """// Internal function called by esp3_proto or radio_hal to send data back to the host
void TCMSerial_internal_push(const uint8_t *data, size_t len) {
    if (!buffer_mutex) return;
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (size_t i = 0; i < len; i++) {
        size_t next = (rx_head + 1) % RX_BUFFER_SIZE;
        // Ignore overflow for testing, overwrite!
        rx_buffer[rx_head] = data[i];
        rx_head = next;
        if (rx_head == rx_tail) rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE; // Advance tail to drop oldest
    }
    xSemaphoreGive(buffer_mutex);
}"""
    
    if old in content:
        content = content.replace(old, new)
        with open('src/TCMSerial.c', 'w') as f:
            f.write(content)
        print("Patched RX buffer!")
    else:
        print("Not found in TCMSerial.c")

if __name__ == "__main__":
    patch_esp3_rx_buffer()
