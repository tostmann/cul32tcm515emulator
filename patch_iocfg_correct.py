import sys

with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix the initial configs
content = content.replace("cc1101_write_reg(0x00, 0x0D); // IOCFG0: GDO0 = Serial Data Output (Async)\n    cc1101_write_reg(0x02, 0x0E); // IOCFG2: GDO2 = Carrier Sense", "cc1101_write_reg(0x02, 0x0D); // IOCFG0: GDO0 = Serial Data Output (Async)\n    cc1101_write_reg(0x00, 0x0E); // IOCFG2: GDO2 = Carrier Sense")

# Fix the TX switch
content = content.replace("cc1101_write_reg(0x00, 0x2E); // GDO0 to High-Z (Input for TX)", "cc1101_write_reg(0x02, 0x2E); // GDO0 to High-Z (Input for TX)")

# Fix the RX switch
content = content.replace("cc1101_write_reg(0x00, 0x0D); // GDO0 back to Output for RX", "cc1101_write_reg(0x02, 0x0D); // GDO0 back to Output for RX")

with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
    f.write(content)
