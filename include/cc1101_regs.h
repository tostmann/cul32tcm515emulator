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
    {0x00, 0x06}, // IOCFG2: GDO2 - 0x06 = Asserts when sync word has been sent / received
    {0x02, 0x01}, // IOCFG0: GDO0 - 0x01 = Associated with RX FIFO (interrupt on packet rx)
    {0x03, 0x47}, // FIFOTHR: RX FIFO Threshold
    {0x04, 0xA5}, // SYNC1: EnOcean Preamble/Sync (Muss an ERP1 Sync-Word angepasst werden)
    {0x05, 0x5A}, // SYNC0: 
    {0x06, 0x15}, // PKTLEN: Max 21 Bytes for ERP1
    {0x07, 0x04}, // PKTCTRL1: Append status (RSSI, LQI)
    {0x08, 0x00}, // PKTCTRL0: Fixed packet length mode, no CRC (EnOcean uses checksum)
    {0x0D, 0x21}, // FREQ2: 868.3 MHz (26 MHz XOSC)
    {0x0E, 0x65}, // FREQ1: 
    {0x0F, 0x6A}, // FREQ0: 
    {0x10, 0x2B}, // MDMCFG4: RX Filter BW ~540kHz (needed for 125kbps OOK)
    {0x11, 0x22}, // MDMCFG3: Data rate 125 kbps
    {0x12, 0x3A}, // MDMCFG2: ASK/OOK (0x30) + Manchester Enable (0x08) + 16/16 Sync (0x02) = 0x3A
    {0x13, 0x22}, // MDMCFG1: Forward Error Correction disabled
    {0x14, 0xF8}, // MDMCFG0: Channel spacing
    {0x15, 0x47}, // DEVIATN: (Ignored in OOK)
    {0x16, 0x07}, // MCSM2: RX_TIME
    {0x17, 0x3F}, // MCSM1: CCA mode (Clear Channel Assessment) für LBT (Listen Before Talk)
    {0x18, 0x18}, // MCSM0: Auto-calibrate when going from IDLE to RX/TX
    {0x19, 0x1D}, // FOCCFG: Frequency Offset Compensation
    {0x1A, 0x1C}, // BSCFG: Bit Synchronization Configuration
    {0x1B, 0x00}, // AGCCTRL2: AGC Control (Crucial for OOK)
    {0x1C, 0x00}, // AGCCTRL1: AGC Control
    {0x1D, 0x92}, // AGCCTRL0: AGC Control
    {0x21, 0xB6}, // FREND1: Front End RX
    {0x22, 0x11}, // FREND0: Front End TX (OOK needs PATABLE[0] for '0' and PATABLE[1] for '1')
    {0x23, 0xEA}, // FSCAL3: Frequency Synthesizer Calibration
    {0x24, 0x2A}, // FSCAL2: 
    {0x25, 0x00}, // FSCAL1: 
    {0x26, 0x1F}, // FSCAL0: 
};

/* PATABLE for OOK: Index 0 is 'off', Index 1 is 'on' (+10dBm z.B.) */
static const uint8_t patable_ook[] = {0x00, 0xC0}; 

#endif // CC1101_REGS_H
