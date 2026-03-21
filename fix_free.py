import os

def fix_the_free():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # REMOVE free(tx_symbols)
    old_code = """    rmt_enable(rx_channel);
    is_transmitting = false;
    free(tx_symbols);
}"""
    
    new_code = """    // rmt_enable(rx_channel);
    is_transmitting = false;
    // no more free!
}"""
    
    content = content.replace(old_code, new_code)
    
    # We also uncomment rmt_transmit because that wasn't the issue!
    old_tx = """        // rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        // rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        // rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);"""
        
    new_tx = """        rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
        rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);
        rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);"""
        
    content = content.replace(old_tx, new_tx)

    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Removed free(tx_symbols)!")

if __name__ == "__main__":
    fix_the_free()
