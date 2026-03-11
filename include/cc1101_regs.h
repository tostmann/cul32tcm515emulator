#ifndef CC1101_REGS_H
#define CC1101_REGS_H

#include <stdint.h>

/* CC1101 Command Strobes */
#define CC1101_SRES      0x30        // Reset chip
#define CC1101_SRX       0x34        // Enable RX
#define CC1101_STX       0x35        // Enable TX
#define CC1101_SIDLE     0x36        // Exit RX / TX, turn off freq synthesizer
#define CC1101_SFRX      0x3A        // Flush the RX FIFO buffer
#define CC1101_SFTX      0x3B        // Flush the TX FIFO buffer

/* CC1101 Status Registers (Burst access) */
#define CC1101_PARTNUM   0x30
#define CC1101_VERSION   0x31
#define CC1101_MARCSTATE 0x35

/* CC1101 Configuration Register Array */
typedef struct {
    uint8_t addr;
    uint8_t val;
} cc1101_cfg_t;

/* 
 * Optimierte Parameter für EnOcean ERP1
 * 868.3 MHz, 125 kbps, ASK/OOK, Manchester aktiviert 
 */
static const cc1101_cfg_t erp1_config[] = {
    {0x00, 0x0E}, // IOCFG2: GDO2 - 0x0E = Carrier Sense (CS)
    {0x02, 0x0D}, // IOCFG0: GDO0 - 0x0D = Serial Data Output (Async)
    {0x03, 0x47}, // FIFOTHR: 
    {0x08, 0x32}, // PKTCTRL0: Asynchronous Serial Mode
    {0x0D, 0x21}, // FREQ2: 868.3 MHz
    {0x0E, 0x65}, // FREQ1: 
    {0x0F, 0x6A}, // FREQ0: 
    {0x10, 0xCB}, // MDMCFG4: optimized for 125kbps OOK
    {0x11, 0x83}, // MDMCFG3: 
    {0x12, 0x30}, // MDMCFG2: ASK/OOK (0x30), Asynchronous Mode, No Sync Word
    {0x17, 0x30}, // MCSM1: IDLE after TX, IDLE after RX
    {0x18, 0x18}, // MCSM0: FS Autocal
    {0x1B, 0x03}, // AGCCTRL2: 
    {0x1C, 0x00}, // AGCCTRL1: 
    {0x1D, 0x91}, // AGCCTRL0: 
    {0x21, 0xB6}, // FREND1
    {0x22, 0x11}, // FREND0
    {0x23, 0xEA}, // FSCAL3
    {0x24, 0x2A}, // FSCAL2
    {0x25, 0x00}, // FSCAL1
    {0x26, 0x1F}, // FSCAL0
};

/* PATABLE for OOK: Index 0 is 'off', Index 1 is 'on' (+10dBm z.B.) */
static const uint8_t patable_ook[] = {0x00, 0xC0}; 

#endif // CC1101_REGS_H
