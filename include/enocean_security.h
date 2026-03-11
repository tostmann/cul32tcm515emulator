#ifndef ENOCEAN_SECURITY_H
#define ENOCEAN_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Context for a secure device (store this in NVS/Flash)
typedef struct {
    uint32_t sender_id;
    uint8_t key[16];        // 128-bit AES Key
    uint32_t rlc;           // Current Rolling Code
    uint8_t rlc_size;       // 2 (16-bit) or 3 (24-bit) or 4 (32-bit)
} enocean_sec_device_t;

/**
 * @brief Verifies the AES-128 CMAC of an EnOcean telegram.
 * @return true if MAC is valid, false otherwise.
 */
bool enocean_sec_verify_mac(const enocean_sec_device_t *device, 
                            const uint8_t *payload, size_t payload_len, 
                            uint32_t received_rlc, 
                            const uint8_t *received_mac, size_t mac_len);

/**
 * @brief Decrypts payload using AES-128 CTR (Standard for VAES).
 */
bool enocean_sec_decrypt_ctr(const enocean_sec_device_t *device, 
                             uint8_t *payload, size_t payload_len, 
                             uint32_t rlc);

#endif // ENOCEAN_SECURITY_H
