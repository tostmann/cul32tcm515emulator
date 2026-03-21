import os

def check_crash_point():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # We will replace the start of radio_transmit with tracing
    old_tx = """    is_transmitting = true;
    rmt_disable(rx_channel);

    gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);
    for (int sub = 0; sub < 3; sub++) {"""
    
    new_tx = """    is_transmitting = true;
    
    uint8_t d1 = 0xD1;
    esp3_send_packet(0x86, &d1, 1, NULL, 0); // Before disable
    vTaskDelay(pdMS_TO_TICKS(10));
    
    rmt_disable(rx_channel);
    
    uint8_t d2 = 0xD2;
    esp3_send_packet(0x87, &d2, 1, NULL, 0); // After disable
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);
    
    uint8_t d3 = 0xD3;
    esp3_send_packet(0x88, &d3, 1, NULL, 0); // After GPIO
    vTaskDelay(pdMS_TO_TICKS(10));
    
    for (int sub = 0; sub < 3; sub++) {"""
    
    content = content.replace(old_tx, new_tx)

    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    check_crash_point()
