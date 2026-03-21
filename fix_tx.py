import re

with open('src/radio_hal.c', 'r') as f:
    text = f.read()

# Add copy encoder initialization
encoder_init = '''
    // Create copy encoder for TX
    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &tx_encoder);
'''
text = text.replace('rmt_new_tx_channel(&tcfg, &tx_channel);', 'rmt_new_tx_channel(&tcfg, &tx_channel);\n' + encoder_init)

# Replace the transmit call
text = text.replace('rmt_transmit(tx_channel, NULL, tx_symbols, s_idx, &tx_cfg);', 'rmt_transmit(tx_channel, tx_encoder, tx_symbols, s_idx * sizeof(rmt_symbol_word_t), &tx_cfg);')

# Add tx_encoder global var
if 'rmt_encoder_t *tx_encoder = NULL;' not in text:
    text = text.replace('static rmt_channel_handle_t rx_channel = NULL;', 'static rmt_channel_handle_t rx_channel = NULL;\nstatic rmt_encoder_handle_t tx_encoder = NULL;')

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
