import re

with open('src/radio_hal.c', 'r') as f:
    content = f.read()

# Fix init
content = content.replace(
    'rmt_new_tx_channel(&tcfg, &tx_channel);',
    'rmt_new_tx_channel(&tcfg, &tx_channel);\n    gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);'
)

# Fix transmit
tx_block_old = '''        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, NULL, tx_symbols, s_idx, &tx_cfg);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));'''

tx_block_new = '''        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        gpio_set_direction(PIN_GDO0, GPIO_MODE_OUTPUT);
        rmt_transmit(tx_channel, NULL, tx_symbols, s_idx, &tx_cfg);
        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
        gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);
        
        if (sub < 2) vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 15)));'''

content = content.replace(tx_block_old, tx_block_new)

with open('src/radio_hal.c', 'w') as f:
    f.write(content)
