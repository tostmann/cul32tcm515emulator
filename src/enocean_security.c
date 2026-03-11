#include "enocean_security.h"
#include "mbedtls/cmac.h"
#include "mbedtls/aes.h"
#include <string.h>

bool enocean_sec_verify_mac_raw(const enocean_sec_device_t *device, 
                               const uint8_t *mac_input, size_t mi_len, 
                               const uint8_t *received_mac, size_t mac_len) {
    if (!device || !mac_input || !received_mac) return false;

    const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
    uint8_t mac_output[16] = {0};

    int ret = mbedtls_cipher_cmac(cipher_info, device->key, 128, mac_input, mi_len, mac_output);
    if (ret != 0) return false;

    return (memcmp(mac_output, received_mac, mac_len) == 0);
}

// AES-128 CTR Decryption (Variable AES)
bool enocean_sec_decrypt_ctr(const enocean_sec_device_t *device, 
                             uint8_t *payload, size_t payload_len, 
                             uint32_t rlc) {
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);

    if (mbedtls_aes_setkey_enc(&aes_ctx, device->key, 128) != 0) {
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    uint8_t nonce_counter[16] = {0};
    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    nonce_counter[0] = (device->sender_id >> 24) & 0xFF;
    nonce_counter[1] = (device->sender_id >> 16) & 0xFF;
    nonce_counter[2] = (device->sender_id >> 8) & 0xFF;
    nonce_counter[3] = device->sender_id & 0xFF;

    nonce_counter[4] = (rlc >> 24) & 0xFF;
    nonce_counter[5] = (rlc >> 16) & 0xFF;
    nonce_counter[6] = (rlc >> 8) & 0xFF;
    nonce_counter[7] = rlc & 0xFF;

    mbedtls_aes_crypt_ctr(&aes_ctx, payload_len, &nc_off, nonce_counter, stream_block, payload, payload);
    
    mbedtls_aes_free(&aes_ctx);
    return true;
}
