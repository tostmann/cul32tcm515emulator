import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if line.startswith("void radio_transmit(const uint8_t *data, uint8_t len) {"):
        skip = True
        new_lines.append("""void radio_transmit(const uint8_t *data, uint8_t len) {
    if (!data || len == 0 || len > 32) return;

    if (g_radio_loopback_enabled) {
        uint8_t opt[7] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x30, 0x00 }; 
        esp3_send_packet(ESP3_TYPE_RADIO_ERP1, data, len, opt, 7);
    }

    static rmt_symbol_word_t tx_symbols[1024];

    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) checksum += data[i];

    size_t s_idx = 0;
    // Preamble: 4 bits of '0' (EnOcean standard)
    for (int i = 0; i < 4; i++) push_manchester_bit(tx_symbols, &s_idx, 0);
    // Sync bit: 1 bit of '1'
    push_manchester_bit(tx_symbols, &s_idx, 1);
    
    // Data bits
    for (int i = 0; i < len; i++) {
        for (int b = 7; b >= 0; b--) {
            push_manchester_bit(tx_symbols, &s_idx, (data[i] >> b) & 1);
        }
    }
    // Checksum
    for (int b = 7; b >= 0; b--) {
        push_manchester_bit(tx_symbols, &s_idx, (checksum >> b) & 1);
    }
    
    tx_symbols[s_idx++] = (rmt_symbol_word_t){ .duration0 = 100, .level0 = 0, .duration1 = 0, .level1 = 0 };

    is_transmitting = true;
    
    gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);
    
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
    is_transmitting = false;
}
""")
        continue
    if skip:
        if line.startswith("static inline int hex2int(char c) {"):
            skip = False
            new_lines.append(line)
        continue
    new_lines.append(line)

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
