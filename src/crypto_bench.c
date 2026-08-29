#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define FDS_EXT_BYTES_IMPL
#include "fds_ext_bytes.h"

#define FDS_EXT_CRYPTO_IMPL
#include "fds_ext_crypto.h"

#define DATA_SIZE ((size_t)128 * 1024 * 1024)
#define ITERATIONS 100


/* ============================================================================
   PRNG
   ============================================================================ */

static uint32_t rng_state = 0xA5B35791u;

static uint32_t rng_u32(void)
{
    uint32_t x = rng_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    rng_state = x;
    return x;
}

static void fill_random(uint8_t *data, size_t size)
{
    size_t i = 0;

    while (i + 4 <= size) {
        uint32_t v = rng_u32();

        data[i + 0] = (uint8_t)(v);
        data[i + 1] = (uint8_t)(v >> 8);
        data[i + 2] = (uint8_t)(v >> 16);
        data[i + 3] = (uint8_t)(v >> 24);

        i += 4;
    }

    while (i < size)
        data[i++] = (uint8_t)rng_u32();
}


/* ============================================================================
   High-resolution timer
   ============================================================================ */

static double now_seconds(void)
{
    static LARGE_INTEGER frequency;
    static bool initialized = false;

    LARGE_INTEGER counter;

    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = true;
    }

    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart /
           (double)frequency.QuadPart;
}


/* ============================================================================
   Main
   ============================================================================ */

int main(void)
{
    printf("============================================================\n");
    printf("FDS ChaCha20 Benchmark\n");
    printf("============================================================\n");

    printf("[INFO] Payload: %.2f MiB\n",
           (double)DATA_SIZE / (1024.0 * 1024.0));

    printf("[INFO] Iterations: %d\n", ITERATIONS);


    /* ------------------------------------------------------------------------
       Allocate original data
       ------------------------------------------------------------------------ */

    uint8_t *original = malloc(DATA_SIZE);

    if (!original) {
        fprintf(stderr, "[ERROR] Failed to allocate original buffer\n");
        return 1;
    }

    fill_random(original, DATA_SIZE);


    /* ------------------------------------------------------------------------
       Key / nonce
       ------------------------------------------------------------------------ */

    uint8_t key[32];
    uint8_t nonce[12];

    for (size_t i = 0; i < sizeof(key); ++i)
        key[i] = (uint8_t)rng_u32();

    for (size_t i = 0; i < sizeof(nonce); ++i)
        nonce[i] = (uint8_t)rng_u32();


    /* ------------------------------------------------------------------------
       Encrypt once to determine container size
       ------------------------------------------------------------------------ */

    FdsBytesBuilder encrypted = fds_bb_create(0);

    FdsBytesView input =
        fds_bv(original, DATA_SIZE);

    if (!fds_ext_encrypt_chacha20(
            input,
            key,
            nonce,
            &encrypted))
    {
        fprintf(stderr, "[ERROR] Initial encryption failed\n");

        free(original);
        fds_bb_destroy(&encrypted);

        return 1;
    }

    printf("[INFO] Container size: %zu bytes\n", encrypted.size);


    /* ------------------------------------------------------------------------
       Warm-up
       ------------------------------------------------------------------------ */

    FdsBytesBuilder warmup =
        fds_bb_create(0);

    for (int i = 0; i < 5; ++i) {
        warmup.size = 0;

        if (!fds_ext_encrypt_chacha20(
                input,
                key,
                nonce,
                &warmup))
        {
            fprintf(stderr, "[ERROR] Warm-up encryption failed\n");
            return 1;
        }
    }

    fds_bb_destroy(&warmup);


    /* =========================================================================
       ENCRYPT BENCHMARK
       ========================================================================= */

    FdsBytesBuilder enc_bench =
        fds_bb_create(DATA_SIZE + 64);

    double encrypt_start = now_seconds();

    for (int i = 0; i < ITERATIONS; ++i) {
        enc_bench.size = 0;

        if (!fds_ext_encrypt_chacha20(
                input,
                key,
                nonce,
                &enc_bench))
        {
            fprintf(stderr, "[ERROR] Encryption failed at iteration %d\n", i);
            return 1;
        }
    }

    double encrypt_end = now_seconds();

    double encrypt_time =
        encrypt_end - encrypt_start;

    double encrypt_bytes =
        (double)DATA_SIZE * ITERATIONS;

    double encrypt_mib =
        encrypt_bytes / (1024.0 * 1024.0);

    double encrypt_speed =
        encrypt_mib / encrypt_time;


    /* =========================================================================
       DECRYPT BENCHMARK
       ========================================================================= */

    FdsBytesView encrypted_view =
        fds_bv(encrypted.data, encrypted.size);

    FdsBytesBuilder dec_bench =
        fds_bb_create(DATA_SIZE);

    /*
        Warm-up decrypt.
    */
    dec_bench.size = 0;

    if (!fds_ext_decrypt_chacha20(
            encrypted_view,
            key,
            &dec_bench))
    {
        fprintf(stderr, "[ERROR] Warm-up decryption failed\n");
        return 1;
    }


    double decrypt_start = now_seconds();

    for (int i = 0; i < ITERATIONS; ++i) {
        dec_bench.size = 0;

        if (!fds_ext_decrypt_chacha20(
                encrypted_view,
                key,
                &dec_bench))
        {
            fprintf(stderr,
                    "[ERROR] Decryption failed at iteration %d\n",
                    i);

            return 1;
        }
    }

    double decrypt_end = now_seconds();

    double decrypt_time =
        decrypt_end - decrypt_start;

    double decrypt_mib =
        encrypt_bytes / (1024.0 * 1024.0);

    double decrypt_speed =
        decrypt_mib / decrypt_time;


    /* =========================================================================
       FINAL VERIFY
       ========================================================================= */

    if (dec_bench.size != DATA_SIZE) {
        fprintf(stderr,
                "[FAIL] Size mismatch: expected %zu, got %zu\n",
                DATA_SIZE,
                dec_bench.size);

        return 1;
    }

    if (memcmp(
            original,
            dec_bench.data,
            DATA_SIZE) != 0)
    {
        fprintf(stderr,
                "[FAIL] Decrypted data does not match original\n");

        return 1;
    }


    /* =========================================================================
       Results
       ========================================================================= */

    double total_time =
        encrypt_time + decrypt_time;

    double roundtrip_speed =
        ((encrypt_bytes * 2.0) / (1024.0 * 1024.0))
        / total_time;

    printf("\n");
    printf("============================================================\n");
    printf("RESULT\n");
    printf("============================================================\n");

    printf("Encryption time      : %.6f sec\n",
           encrypt_time);

    printf("Encryption speed     : %.2f MiB/s\n",
           encrypt_speed);

    printf("\n");

    printf("Decryption time      : %.6f sec\n",
           decrypt_time);

    printf("Decryption speed     : %.2f MiB/s\n",
           decrypt_speed);

    printf("\n");

    printf("Round-trip speed     : %.2f MiB/s\n",
           roundtrip_speed);

    printf("Integrity            : PASS\n");
    printf("Original == Decrypted: YES\n");

    printf("============================================================\n");


    /* ------------------------------------------------------------------------
       Cleanup
       ------------------------------------------------------------------------ */

    fds_bb_destroy(&encrypted);
    fds_bb_destroy(&enc_bench);
    fds_bb_destroy(&dec_bench);

    free(original);

    return 0;
}