import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Disable RX channel initialization
content = content.replace("rmt_new_rx_channel(&rcfg, &rx_channel);", "//rmt_new_rx_channel(&rcfg, &rx_channel);")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
