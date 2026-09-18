/*
    src/crypto_test.c — Test suite for fds_ext_crypto.h (ChaCha20)
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>


#define FDS_EXT_BYTES_IMPL
#include "fds_ext_bytes.h"

#define FDS_EXT_CRYPTO_IMPL
#include "fds_ext_crypto.h"

int main(void) {
    printf("[INFO] Starting ChaCha20 crypto test...\n");

    // 1. Define a 256-bit key and 96-bit nonce
    uint8_t key[32];
    for (int i = 0; i < 32; ++i) {
        key[i] = (uint8_t)(i * 3 + 7);
    }

    uint8_t nonce[12];
    for (int i = 0; i < 12; ++i) {
        nonce[i] = (uint8_t)(i + 0x42);
    }

    // 2. Prepare sample plaintext
    const char *original_text = "FDS Low-Level Framework: Secure binary transmission test via ChaCha20!";
    FdsBytesView input = fds_bv_from_cstr(original_text);

    printf("[INFO] Original payload size: %zu bytes\n", input.size);

    // 3. Encrypt payload into a builder container
    FdsBytesBuilder cipher_builder = fds_bb_create(0);
    bool enc_success = fds_ext_encrypt_chacha20(input, key, nonce, &cipher_builder);
    assert(enc_success);
    
    printf("[INFO] Encrypted container size (Header + Ciphertext): %zu bytes\n", cipher_builder.size);
    
    // 4. Decrypt container back into a plaintext builder
    FdsBytesView cipher_view = fds_bv(cipher_builder.data, cipher_builder.size);
    printf("\n%s\n", cipher_view.data);
    FdsBytesBuilder plain_builder = fds_bb_create(0);
    bool dec_success = fds_ext_decrypt_chacha20(cipher_view, key, &plain_builder);
    assert(dec_success);

    // Ensure null termination for string comparison/printing
    fds_bb_append_byte(&plain_builder, 0x00);
    printf("[INFO] Decrypted text: %s\n", (char *)plain_builder.data);

    // 5. Verify integrity (exact byte match)
    assert(plain_builder.size - 1 == input.size);
    assert(memcmp(plain_builder.data, input.data, input.size) == 0);
    printf("[INFO] SUCCESS: Decrypted data matches original input bit-for-bit!\n");

    // 6. Test negative case: Decryption with a wrong key should fail safely (rollback)
    uint8_t wrong_key[32];
    memset(wrong_key, 0xFF, sizeof(wrong_key));

    size_t prev_size = plain_builder.size;
    bool wrong_dec_success = fds_ext_decrypt_chacha20(cipher_view, wrong_key, &plain_builder);
    
    assert(!wrong_dec_success);
    assert(plain_builder.size == prev_size); // Check transaction rollback
    printf("[INFO] SUCCESS: Wrong key detected and rejected safely with builder rollback!\n");

    // Cleanup memory builders
    fds_bb_destroy(&cipher_builder);
    fds_bb_destroy(&plain_builder);

    printf("[INFO] All crypto tests passed successfully!\n");
    return 0;
}