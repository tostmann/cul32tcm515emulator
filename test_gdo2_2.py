import re

with open('src/radio_hal.c', 'r') as f:
    text = f.read()

patch = '''
        if (xSemaphoreTake(carrier_sense_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI("HAL", "CS Triggered! GDO2=%d", gpio_get_level(PIN_GDO2));
            rmt_receive(rx_channel, rmt_rx_buffer, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t), &cfg);
            if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
                process_rmt_rx(rmt_rx_symbols);
            } else {
                rmt_disable(rx_channel); rmt_enable(rx_channel);
            }
        } else {
            // ESP_LOGI("HAL", "No CS. GDO2=%d", gpio_get_level(PIN_GDO2));
        }'''

# Find the while(1) loop in rf_rx_task_impl
start_idx = text.find('while (1) {')
end_idx = text.find('}', start_idx + 10) + 1
while text.count('}', start_idx, end_idx) < text.count('{', start_idx, end_idx):
    end_idx = text.find('}', end_idx) + 1

new_loop = 'while (1) {' + patch + '\n    }'
text = text[:start_idx] + new_loop + text[end_idx:]

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
