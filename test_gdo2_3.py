import re
with open('src/radio_hal.c', 'r') as f:
    text = f.read()

# find where the `if (xSemaphoreTake` ends
# it's just before the end of the while loop.
patch = '''        if (xSemaphoreTake(carrier_sense_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI("HAL", "CS Triggered! GDO2=%d", gpio_get_level(PIN_GDO2));
            if (is_transmitting) { continue; }
            memset(rmt_rx_buffer, 0, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t));
            if (rmt_receive(rx_channel, rmt_rx_buffer, MAX_RMT_SYMBOLS, &cfg) == ESP_OK) {
                if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                    size_t cnt = 0; while (cnt < MAX_RMT_SYMBOLS && rmt_rx_buffer[cnt].duration0 != 0) cnt++;
                    for (size_t i = 0; i < cnt; i++) {
                        erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level0, rmt_rx_buffer[i].duration0, false);
                        if (rmt_rx_buffer[i].duration1 > 0) {
                            erp1_decode_pulse(&global_decoder, rmt_rx_buffer[i].level1, rmt_rx_buffer[i].duration1, false);
                        }
                    }
                    erp1_decode_pulse(&global_decoder, 0, 0, true); 
                } else {
                    rmt_disable(rx_channel); rmt_enable(rx_channel);
                }
            }
            xSemaphoreTake(carrier_sense_sem, 0); 
        } else {
            ESP_LOGI("HAL", "No CS. GDO2=%d", gpio_get_level(PIN_GDO2));
        }'''

# Replace the whole body of the while loop
start_idx = text.find('while (1) {') + len('while (1) {\n')
end_idx = text.find('    }', start_idx)
text = text[:start_idx] + patch + '\n' + text[end_idx:]

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
