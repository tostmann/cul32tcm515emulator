import os

def fix_rx_disable():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove the debug stuff we added
    old_tx = """    uint8_t d1 = 0xD1;
    esp3_send_packet(0x86, &d1, 1, NULL, 0); // Before disable
    vTaskDelay(pdMS_TO_TICKS(10));
    
    rmt_disable(rx_channel);
    
    uint8_t d2 = 0xD2;
    esp3_send_packet(0x87, &d2, 1, NULL, 0); // After disable
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);
    
    uint8_t d3 = 0xD3;
    esp3_send_packet(0x88, &d3, 1, NULL, 0); // After GPIO
    vTaskDelay(pdMS_TO_TICKS(10));"""
    
    new_tx = """    // rmt_disable(rx_channel); // CRASHES IF RECEIVE IS PENDING!
    
    gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);"""
    
    content = content.replace(old_tx, new_tx)

    # And at the end of radio_transmit
    old_end = """    rmt_enable(rx_channel);
    is_transmitting = false;
    
    uint8_t d_exit = 0xBB;
    esp3_send_packet(0x82, &d_exit, 1, NULL, 0);"""
    
    new_end = """    // rmt_enable(rx_channel);
    is_transmitting = false;"""
    
    content = content.replace(old_end, new_end)
    
    # Also remove the trace elements inside the loop
    old_loop = """        uint8_t d_pre = 0xC1;
        esp3_send_packet(0x83, &d_pre, 1, NULL, 0); // before rmt_transmit
        vTaskDelay(pdMS_TO_TICKS(10)); // flush

        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        
        uint8_t d_mid = 0xC2;
        esp3_send_packet(0x84, &d_mid, 1, NULL, 0); // after rmt_transmit
        vTaskDelay(pdMS_TO_TICKS(10)); // flush

        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        
        uint8_t d_post = 0xC3;
        esp3_send_packet(0x85, &d_post, 1, NULL, 0); // after wait
        vTaskDelay(pdMS_TO_TICKS(10)); // flush
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));"""
        
    new_loop = """        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));"""
        
    content = content.replace(old_loop, new_loop)

    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Fixed RX disable crash!")

if __name__ == "__main__":
    fix_rx_disable()
