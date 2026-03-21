import os

def check_rmt_transmit():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    old_loop = """        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));"""
        
    new_loop = """        // rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        // rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        // rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));"""
        
    content = content.replace(old_loop, new_loop)

    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    check_rmt_transmit()
