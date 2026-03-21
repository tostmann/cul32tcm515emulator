import os

def fix_rmt_panic():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Add rmt_enable(tx_channel) in radio_hal_init
    old_tx_init = """    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &tx_encoder);

    gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);"""
    
    new_tx_init = """    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &tx_encoder);
    
    rmt_enable(tx_channel);

    gpio_set_direction(PIN_GDO0, GPIO_MODE_INPUT);"""
    
    content = content.replace(old_tx_init, new_tx_init)

    # Remove the debug return in radio_transmit
    old_tx = """    // Return early to test if preamble crashes
    return;
    // Data bits
    for (int i = 0; i < len; i++) {"""
    new_tx = """    // Data bits
    for (int i = 0; i < len; i++) {"""
    
    content = content.replace(old_tx, new_tx)

    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Fixed RMT panic!")

if __name__ == "__main__":
    fix_rmt_panic()
