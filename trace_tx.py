import os

def trace_tx():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    old_tx = """        cc1101_strobe(CC1101_STX);
        esp_rom_delay_us(100); 

        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));"""
    
    new_tx = """        cc1101_strobe(CC1101_STX);
        esp_rom_delay_us(100); 

        uint8_t d_pre = 0xC1;
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
    
    content = content.replace(old_tx, new_tx)

    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    trace_tx()
