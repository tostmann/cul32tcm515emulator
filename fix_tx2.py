import re

with open('src/radio_hal.c', 'r') as f:
    text = f.read()

text = text.replace('static rmt_encoder_handle_t tx_encoder = NULL;\nstatic rmt_channel_handle_t tx_channel = NULL;\nstatic rmt_encoder_handle_t tx_encoder = NULL;', 'static rmt_channel_handle_t tx_channel = NULL;\nstatic rmt_encoder_handle_t tx_encoder = NULL;')

# also verify include
if '#include "driver/rmt_encoder.h"' not in text:
    text = text.replace('#include "driver/rmt_tx.h"', '#include "driver/rmt_tx.h"\n#include "driver/rmt_encoder.h"')

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
