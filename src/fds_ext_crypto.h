/*
    fds_ext_crypto.h — ChaCha20 Stream Encryption Module with Adler-32 Integrity
                       (FDS Extension)

    Features:
    - Zero external dependencies.
    - ChaCha20 core.
    - x86/x86-64 inline assembly for the ChaCha20 quarter-round.
    - C fallback on non-x86 targets.
    - Embedded container header:
        [Magic 2B]["FX"]
        [Mode 1B]
        [Reserved 1B]
        [Nonce 12B]
        [Checksum 4B]
        [Original Size 4B]
        [Ciphertext ...]
    - Adler-32 detects accidental corruption.
      NOTE: Adler-32 is NOT cryptographic authentication.
    - Transactional decryption: builder size is rolled back on failure.
    - Explicit validation of header and payload size.
    - ChaCha20 counter-overflow protection.

    Usage:
    In EXACTLY ONE C file:

        #define FDS_EXT_CRYPTO_IMPLEMENTATION
        #include "fds_ext_crypto.h"

    Backward compatibility:
        #define FDS_EXT_CRYPTO_IMPL
    is also accepted.
*/

#ifndef FDS_EXT_CRYPTO_H
#define FDS_EXT_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
    Support both names so old source files continue to work.
*/
#if defined(FDS_EXT_CRYPTO_IMPL) && !defined(FDS_EXT_CRYPTO_IMPLEMENTATION)
    #define FDS_EXT_CRYPTO_IMPLEMENTATION
#endif

#ifndef FDS_EXT_CRYPTO_DEF
    #define FDS_EXT_CRYPTO_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

FDS_EXT_CRYPTO_DEF bool
fds_ext_encrypt_chacha20(
    FdsBytesView input,
    const uint8_t key[32],
    const uint8_t nonce[12],
    FdsBytesBuilder *out_builder
);

FDS_EXT_CRYPTO_DEF bool
fds_ext_decrypt_chacha20(
    FdsBytesView input,
    const uint8_t key[32],
    FdsBytesBuilder *out_builder
);

FDS_EXT_CRYPTO_DEF bool fds_crypto_random_bytes(uint8_t *dest, size_t size);
FDS_EXT_CRYPTO_DEF bool fds_crypto_generate_key(uint8_t key[32]);
FDS_EXT_CRYPTO_DEF bool fds_crypto_generate_nonce(uint8_t nonce[12]);
#ifdef __cplusplus
}
#endif

#endif /* FDS_EXT_CRYPTO_H */


/* ============================================================================
   IMPLEMENTATION
   ============================================================================ */
#ifdef FDS_EXT_CRYPTO_IMPLEMENTATION

#include <string.h>
#include <limits.h>

#define FDS_FX_MAGIC_0       0x46u /* 'F' */
#define FDS_FX_MAGIC_1       0x58u /* 'X' */
#define FDS_FX_MODE_CHACHA20 0x01u

#define FDS_FX_HEADER_SIZE   24u

/*
    ChaCha20 uses a 32-bit block counter.

    Starting from counter = 1, the maximum number of usable blocks is:

        UINT32_MAX

    because counter 0 is intentionally skipped.

    2^32 - 1 blocks * 64 bytes.
*/
#define FDS_CHACHA20_MAX_BYTES \
    ((uint64_t)UINT32_MAX * 64ull)


/* ============================================================================
   Little-endian helpers
   ============================================================================ */

static uint32_t fds_crypto_load32_le(const uint8_t *p)
{
    return ((uint32_t)p[0])
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void fds_crypto_store32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

FDS_EXT_CRYPTO_DEF bool fds_crypto_random_bytes(uint8_t *dest, size_t size) {
    if (!dest || size == 0) return false;

    // Ініціалізуємо генератор випадкових чисел поточним часом.
    // Використання статичного прапорця гарантує, що srand() викличеться лише один раз за весь час роботи програми,
    // інакше при частих викликах функції в межах однієї секунди дані будуть дублюватися.
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    // Заповнюємо буфер випадковими байтами за допомогою простого rand()
    for (size_t i = 0; i < size; ++i) {
        dest[i] = (uint8_t)(rand() % 256);
    }

    return true;
}
FDS_EXT_CRYPTO_DEF bool fds_crypto_generate_key(uint8_t key[32]) {
    return fds_crypto_random_bytes(key, 32);
}

FDS_EXT_CRYPTO_DEF bool fds_crypto_generate_nonce(uint8_t nonce[12]) {
    return fds_crypto_random_bytes(nonce, 12);
}
/* ============================================================================
   Adler-32
   ============================================================================ */

static uint32_t fds_crypto_adler32(const uint8_t *data, size_t size)
{
    uint32_t a = 1;
    uint32_t b = 0;

    /*
        Adler-32 has a defined result for an empty buffer:
            adler32("") == 1

        Therefore data may be NULL when size == 0.
    */
    for (size_t i = 0; i < size; ++i) {
        a += data[i];

        if (a >= 65521u)
            a -= 65521u;

        b += a;

        /*
            b is at most:
                65520 + 65520 = 131040
            so one subtraction is enough.
        */
        if (b >= 65521u)
            b -= 65521u;
    }

    return (b << 16) | a;
}


/* ============================================================================
   ChaCha20 Quarter Round
   ============================================================================ */

#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))

/*
    x86/x86-64 inline assembly implementation.

    This is scalar assembly, not SIMD.

    The compiler still allocates registers and handles register pressure;
    we only provide the actual arithmetic/bitwise instructions.
*/
static inline void
fds_chacha20_qr_asm(
    uint32_t *a,
    uint32_t *b,
    uint32_t *c,
    uint32_t *d
)
{
    uint32_t aa = *a;
    uint32_t bb = *b;
    uint32_t cc = *c;
    uint32_t dd = *d;

    __asm__ volatile (
        "addl %[b], %[a]      \n\t"
        "xorl %[a], %[d]      \n\t"
        "roll $16, %[d]       \n\t"

        "addl %[d], %[c]      \n\t"
        "xorl %[c], %[b]      \n\t"
        "roll $12, %[b]       \n\t"

        "addl %[b], %[a]      \n\t"
        "xorl %[a], %[d]      \n\t"
        "roll $8, %[d]        \n\t"

        "addl %[d], %[c]      \n\t"
        "xorl %[c], %[b]      \n\t"
        "roll $7, %[b]        \n\t"

        : [a] "+r" (aa),
          [b] "+r" (bb),
          [c] "+r" (cc),
          [d] "+r" (dd)

        :
        : "cc"
    );

    *a = aa;
    *b = bb;
    *c = cc;
    *d = dd;
}

#else

/*
    Portable fallback.
*/
static inline uint32_t
fds_crypto_rotl32(uint32_t v, unsigned n)
{
    return (v << n) | (v >> (32u - n));
}

static inline void
fds_chacha20_qr_asm(
    uint32_t *a,
    uint32_t *b,
    uint32_t *c,
    uint32_t *d
)
{
    *a += *b;
    *d ^= *a;
    *d = fds_crypto_rotl32(*d, 16);

    *c += *d;
    *b ^= *c;
    *b = fds_crypto_rotl32(*b, 12);

    *a += *b;
    *d ^= *a;
    *d = fds_crypto_rotl32(*d, 8);

    *c += *d;
    *b ^= *c;
    *b = fds_crypto_rotl32(*b, 7);
}

#endif


/* ============================================================================
   ChaCha20 Block
   ============================================================================ */

static void
fds_chacha20_block(
    uint32_t output[16],
    const uint32_t input[16]
)
{
    uint32_t x[16];

    memcpy(x, input, sizeof(x));

    for (int i = 0; i < 10; ++i) {
        /* Column rounds */
        fds_chacha20_qr_asm(&x[0], &x[4], &x[8],  &x[12]);
        fds_chacha20_qr_asm(&x[1], &x[5], &x[9],  &x[13]);
        fds_chacha20_qr_asm(&x[2], &x[6], &x[10], &x[14]);
        fds_chacha20_qr_asm(&x[3], &x[7], &x[11], &x[15]);

        /* Diagonal rounds */
        fds_chacha20_qr_asm(&x[0], &x[5], &x[10], &x[15]);
        fds_chacha20_qr_asm(&x[1], &x[6], &x[11], &x[12]);
        fds_chacha20_qr_asm(&x[2], &x[7], &x[8],  &x[13]);
        fds_chacha20_qr_asm(&x[3], &x[4], &x[9],  &x[14]);
    }

    for (int i = 0; i < 16; ++i)
        output[i] = x[i] + input[i];
}


/* ============================================================================
   ChaCha20 XOR
   ============================================================================ */

static bool
fds_chacha20_xor(
    const uint8_t *src,
    uint8_t *dst,
    size_t size,
    const uint8_t key[32],
    const uint8_t nonce[12],
    uint32_t counter
)
{
    if (size == 0)
        return true;

    if (!src || !dst || !key || !nonce)
        return false;

    /*
        Protect against 32-bit block counter wrap.

        counter=1 is used by this container.
    */
    if ((uint64_t)counter > (uint64_t)UINT32_MAX)
        return false;

    uint64_t max_blocks =
        (uint64_t)UINT32_MAX - (uint64_t)counter + 1ull;

    uint64_t required_blocks =
        ((uint64_t)size + 63ull) / 64ull;

    if (required_blocks > max_blocks)
        return false;

    uint32_t state[16];

    /* "expand 32-byte k" */
    state[0] = 0x61707865u;
    state[1] = 0x3320646eu;
    state[2] = 0x79622d32u;
    state[3] = 0x6b206574u;

    /* Key */
    for (int i = 0; i < 8; ++i)
        state[4 + i] = fds_crypto_load32_le(key + i * 4);

    /* Counter */
    state[12] = counter;

    /* 96-bit nonce */
    state[13] = fds_crypto_load32_le(nonce + 0);
    state[14] = fds_crypto_load32_le(nonce + 4);
    state[15] = fds_crypto_load32_le(nonce + 8);

    size_t offset = 0;

    while (offset < size) {
        uint32_t block[16];

        fds_chacha20_block(block, state);

        size_t chunk = size - offset;
        if (chunk > 64)
            chunk = 64;

        for (size_t i = 0; i < chunk; ++i) {
            /*
                We serialize the block output explicitly as little-endian.
            */
            uint8_t k =
                (uint8_t)(block[i >> 2] >> ((i & 3u) * 8u));

            dst[offset + i] = src[offset + i] ^ k;
        }

        offset += chunk;

        /*
            We have already validated the maximum number of blocks,
            therefore this increment cannot cause a reused counter.
        */
        state[12]++;
    }

    return true;
}


/* ============================================================================
   Encrypt
   ============================================================================ */

FDS_EXT_CRYPTO_DEF bool
fds_ext_encrypt_chacha20(
    FdsBytesView input,
    const uint8_t key[32],
    const uint8_t nonce[12],
    FdsBytesBuilder *out_builder
)
{
    if (!out_builder || !key || !nonce)
        return false;

    if (input.size > 0 && !input.data)
        return false;

    /*
        Original size is stored as uint32_t.
    */
    if (input.size > UINT32_MAX)
        return false;

    /*
        ChaCha20 block-counter limit.
    */
    if ((uint64_t)input.size > FDS_CHACHA20_MAX_BYTES)
        return false;

    /*
        Protect:
            out_builder->size + header + payload
    */
    if (out_builder->size > SIZE_MAX - FDS_FX_HEADER_SIZE)
        return false;

    size_t after_header =
        out_builder->size + FDS_FX_HEADER_SIZE;

    if (input.size > SIZE_MAX - after_header)
        return false;

    uint32_t checksum =
        fds_crypto_adler32(input.data, input.size);

    /*
        Header:

        [0]    Magic 'F'
        [1]    Magic 'X'
        [2]    Mode
        [3]    Reserved
        [4:15] Nonce
        [16:19] Adler-32
        [20:23] Original size
    */
    fds_bb_append_byte(out_builder, FDS_FX_MAGIC_0);
    fds_bb_append_byte(out_builder, FDS_FX_MAGIC_1);
    fds_bb_append_byte(out_builder, FDS_FX_MODE_CHACHA20);
    fds_bb_append_byte(out_builder, 0x00);

    fds_bb_append(out_builder, nonce, 12);

    fds_bb_append_u32_le(out_builder, checksum);
    fds_bb_append_u32_le(out_builder, (uint32_t)input.size);

    if (input.size == 0)
        return true;

    size_t payload_offset = out_builder->size;

    fds_bb_reserve(out_builder, input.size);

    /*
        reserve() is assumed to provide enough capacity according to
        the FDS bytes-builder contract.
    */
    out_builder->size += input.size;

    if (!fds_chacha20_xor(
            input.data,
            &out_builder->data[payload_offset],
            input.size,
            key,
            nonce,
            1))
    {
        /*
            Encryption should normally never fail here after all
            checks above, but preserve transactional behavior anyway.
        */
        out_builder->size = payload_offset;
        return false;
    }

    return true;
}


/* ============================================================================
   Decrypt
   ============================================================================ */

FDS_EXT_CRYPTO_DEF bool
fds_ext_decrypt_chacha20(
    FdsBytesView input,
    const uint8_t key[32],
    FdsBytesBuilder *out_builder
)
{
    if (!out_builder || !key)
        return false;

    if (input.size > 0 && !input.data)
        return false;

    const size_t start_size = out_builder->size;

#define FDS_DECRYPT_FAIL() \
    do { \
        out_builder->size = start_size; \
        return false; \
    } while (0)

    uint8_t m0;
    uint8_t m1;
    uint8_t mode;
    uint8_t reserved;

    uint8_t nonce[12];

    uint32_t expected_checksum;
    uint32_t orig_size;

    /*
        Header:
            Magic
            Mode
            Reserved
            Nonce
            Checksum
            Original size
    */

    if (!fds_bv_pop_byte(&input, &m0))
        FDS_DECRYPT_FAIL();

    if (m0 != FDS_FX_MAGIC_0)
        FDS_DECRYPT_FAIL();

    if (!fds_bv_pop_byte(&input, &m1))
        FDS_DECRYPT_FAIL();

    if (m1 != FDS_FX_MAGIC_1)
        FDS_DECRYPT_FAIL();

    if (!fds_bv_pop_byte(&input, &mode))
        FDS_DECRYPT_FAIL();

    if (mode != FDS_FX_MODE_CHACHA20)
        FDS_DECRYPT_FAIL();

    if (!fds_bv_pop_byte(&input, &reserved))
        FDS_DECRYPT_FAIL();

    if (reserved != 0)
        FDS_DECRYPT_FAIL();

    for (int i = 0; i < 12; ++i) {
        if (!fds_bv_pop_byte(&input, &nonce[i]))
            FDS_DECRYPT_FAIL();
    }

    if (!fds_bv_read_u32_le(&input, &expected_checksum))
        FDS_DECRYPT_FAIL();

    if (!fds_bv_read_u32_le(&input, &orig_size))
        FDS_DECRYPT_FAIL();

    /*
        The remaining bytes must be EXACTLY the original size.
    */
    if (input.size != (size_t)orig_size)
        FDS_DECRYPT_FAIL();

    /*
        ChaCha20 counter bound.
    */
    if ((uint64_t)orig_size > FDS_CHACHA20_MAX_BYTES)
        FDS_DECRYPT_FAIL();

    /*
        Protect builder size arithmetic.
    */
    if ((size_t)orig_size > SIZE_MAX - out_builder->size)
        FDS_DECRYPT_FAIL();

    /*
        Reserve destination space.

        For transactional behavior, the logical builder size is only
        committed after reservation succeeds according to the builder
        contract.
    */
    fds_bb_reserve(out_builder, orig_size);

    size_t dest_offset = out_builder->size;

    if (orig_size > 0) {
        out_builder->size += (size_t)orig_size;

        if (!fds_chacha20_xor(
                input.data,
                &out_builder->data[dest_offset],
                (size_t)orig_size,
                key,
                nonce,
                1))
        {
            FDS_DECRYPT_FAIL();
        }
    }

    /*
        IMPORTANT:
        Verify even for orig_size == 0.
        Adler-32("") == 1, so a modified checksum in an empty container
        is still detected.
    */
    uint32_t actual_checksum =
        fds_crypto_adler32(
            orig_size > 0
                ? &out_builder->data[dest_offset]
                : NULL,
            (size_t)orig_size
        );

    if (actual_checksum != expected_checksum)
        FDS_DECRYPT_FAIL();

#undef FDS_DECRYPT_FAIL

    return true;
}

#endif /* FDS_EXT_CRYPTO_IMPLEMENTATION */