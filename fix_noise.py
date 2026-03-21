import re
with open('src/radio_hal.c', 'r') as f:
    text = f.read()

# Fix AGC
text = text.replace('cc1101_write_reg(0x1B, 0x07); // AGCCTRL2: Max target amplitude', 'cc1101_write_reg(0x1B, 0x03); // AGCCTRL2')
text = text.replace('cc1101_write_reg(0x1C, 0x00); // AGCCTRL1: Max relative CS sensitivity', 'cc1101_write_reg(0x1C, 0x30); // AGCCTRL1: +14dB relative CS threshold')

# Remove spam
text = text.replace('''            uint8_t d1[1] = { gpio_get_level(PIN_GDO2) };
            esp3_send_packet(0x33, d1, 1, NULL, 0);''', '')

text = text.replace('''        } else {
            uint8_t d2[1] = { gpio_get_level(PIN_GDO2) + 10 }; // +10 to distinguish
            esp3_send_packet(0x33, d2, 1, NULL, 0);
 
        }''', '        }')

with open('src/radio_hal.c', 'w') as f:
    f.write(text)
