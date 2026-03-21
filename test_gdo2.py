import time

def monitor_gdo2():
    pass

with open('src/radio_hal.c', 'r') as f:
    text = f.read()

import re
patch = '''
        if (xSemaphoreTake(carrier_sense_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI("HAL", "CS Rising Edge! GDO2=%d", gpio_get_level(PIN_GDO2));
            rmt_receive_config_t cfg = { .signal_range_min_ns = 125, .signal_range_max_ns = 250000 };
            rmt_receive(rx_channel, rmt_rx_buffer, MAX_RMT_SYMBOLS * sizeof(rmt_symbol_word_t), &cfg);
            if (xSemaphoreTake(rmt_done_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
                ESP_LOGI("HAL", "RMT Received %d symbols", rmt_rx_symbols);
                process_rmt_rx(rmt_rx_symbols);
            } else {
                rmt_disable(rx_channel);
                rmt_enable(rx_channel);
            }
        } else {
            ESP_LOGI("HAL", "CS Timeout. GDO2=%d", gpio_get_level(PIN_GDO2));
        }
'''
text = re.sub(r'if \(xSemaphoreTake\(carrier_sense_sem, portMAX_DELAY\) == pdTRUE\) \{[\s\S]*?\} else \{[\s\S]*?rmt_enable\(rx_channel\);\n\s*\}\n\s*\}', patch, text)

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
