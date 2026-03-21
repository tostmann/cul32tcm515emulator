import os

def patch_radio_hal():
    file_path = 'src/radio_hal.c'
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # We use regex to be safe against whitespace
    import re
    
    old_code_pattern = r'    for \(int sub = 0; sub < 3; sub\+\+\) \{\s*cc1101_strobe\(CC1101_SIDLE\);\s*cc1101_strobe\(CC1101_STX\);\s*esp_rom_delay_us\(100\);\s*rmt_transmit_config_t tx_cfg = \{ \.loop_count = 0 \};\s*gpio_set_direction\(PIN_GDO0, GPIO_MODE_OUTPUT\);\s*rmt_transmit\(tx_channel, tx_encoder, tx_symbols, s_idx \* sizeof\(rmt_symbol_word_t\), &tx_cfg\);\s*rmt_tx_wait_all_done\(tx_channel, portMAX_DELAY\);\s*gpio_set_direction\(PIN_GDO0, GPIO_MODE_INPUT\);\s*if \(sub < 2\) vTaskDelay\(pdMS_TO_TICKS\(20 \+ \(esp_random\(\) % 15\)\)\);\s*\}\s*cc1101_strobe\(CC1101_SIDLE\);\s*cc1101_strobe\(CC1101_SRX\);\s*rmt_enable\(rx_channel\);'

    new_code = """    gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);
    for (int sub = 0; sub < 3; sub++) {
        cc1101_strobe(CC1101_SIDLE);
        cc1101_strobe(CC1101_STX);
        esp_rom_delay_us(100); 

        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));
    }

    cc1101_strobe(CC1101_SIDLE);
    cc1101_strobe(CC1101_SRX);
    gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);
    rmt_enable(rx_channel);"""

    content = re.sub(old_code_pattern, new_code, content)
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    patch_radio_hal()
