import re

with open('src/radio_hal.c', 'r') as f:
    content = f.read()

# Add FREND0 and MCSM0
patch = '''    cc1101_write_reg(0x1D, 0x92); // AGCCTRL0: Freeze on CS, high filter BW
    
    // Fix missing OOK PA power index configuration and autocalibration
    cc1101_write_reg(0x22, 0x11); // FREND0: Use PA index 1 for '1' bit in OOK
    cc1101_write_reg(0x18, 0x18); // MCSM0: Autocalibrate from IDLE to RX/TX
    cc1101_write_reg(0x19, 0x16); // FOCCFG: No freq offset comp for OOK
    cc1101_write_reg(0x20, 0xFB); // WORCTRL: recommended
    cc1101_write_reg(0x23, 0xE9); // FSCAL3: recommended
    cc1101_write_reg(0x24, 0x2A); // FSCAL2: recommended
    cc1101_write_reg(0x25, 0x00); // FSCAL1: recommended
    cc1101_write_reg(0x26, 0x1F); // FSCAL0: recommended
    
    static const uint8_t patable_ook[] = {0x00, 0xC0};'''

content = content.replace(
    '''    cc1101_write_reg(0x1D, 0x92); // AGCCTRL0: Freeze on CS, high filter BW
    
    static const uint8_t patable_ook[] = {0x00, 0xC0};''',
    patch
)

with open('src/radio_hal.c', 'w') as f:
    f.write(content)
