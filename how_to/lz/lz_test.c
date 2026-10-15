// /*
// test_fds_lz.c

// Build:
// gcc -std=c23 -O2 -Wall -Wextra -Werror -pedantic test_fds_lz.c -o test_fds_lz.exe

// Or with debug memory:
// gcc -std=c23 -g -Wall -Wextra -Werror -pedantic -DDEBUG_MEM test_fds_lz.c -o test_fds_lz.exe
// */

#define FDS_IMPL
#define FDS_LZ_IMPL

#include "fds_lz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== */
/* Test infrastructure                                                        */
/* ========================================================================== */

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_CHECK(cond, ...)                                      \
    do                                                             \
    {                                                              \
        ++g_tests_run;                                             \
        if (!(cond))                                               \
        {                                                          \
            ++g_tests_failed;                                      \
            fprintf(stderr, "[FAIL] %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                          \
            fprintf(stderr, "\n");                                 \
            return false;                                          \
        }                                                          \
    } while (0)

#define TEST_CASE(name)                  \
    do                                   \
    {                                    \
        printf("\n[TEST] %s\n", (name)); \
    } while (0)

static bool bytes_equal(const uint8_t *a, size_t a_size, const uint8_t *b, size_t b_size)
{
    if (a_size != b_size)
        return false;
    if (a_size == 0)
        return true;
    return memcmp(a, b, a_size) == 0;
} /* ========================================================================== */ /* Deterministic pseudo-random generator                                      */ /* ========================================================================== */
static uint32_t rng_next(uint32_t *state)
{ /* xorshift32 */
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}
static void fill_random(uint8_t *data, size_t size, uint32_t *state)
{
    for (size_t i = 0; i < size; ++i)
        data[i] = (uint8_t)rng_next(state);
} /* ========================================================================== */ /* Generic round-trip test                                                    */ /* ========================================================================== */
static bool test_roundtrip(const char *name, const uint8_t *data, size_t size, bool verbose)
{
    FdsBytesBuilder compressed = fds_bb_create(0);
    FdsBytesBuilder decompressed = fds_bb_create(0);
    TEST_CHECK(compressed.data || compressed.capacity == 0, "%s: failed to create compression builder", name);
    TEST_CHECK(decompressed.data || decompressed.capacity == 0, "%s: failed to create decompression builder", name);
    FdsBytesView input = {.data = data, .size = size};
    FdsCompressStatus status = fds_compress_lz(input, &compressed);
    TEST_CHECK(status == FDS_CMP_COMPRESSED || status == FDS_CMP_STORED, "%s: compression returned error", name);
    FdsBytesView compressed_view = fds_bb_to_view(&compressed);
    bool ok = fds_decompress_lz(compressed_view, &decompressed);
    TEST_CHECK(ok, "%s: decompression failed", name);
    TEST_CHECK(decompressed.size == size, "%s: decompressed size mismatch: got %zu, expected %zu", name, decompressed.size, size);
    TEST_CHECK(bytes_equal(decompressed.data, decompressed.size, data, size), "%s: decompressed data differs from original", name); /* Compression result itself must never be larger than:  header + original  because fds_compress_lz() falls back to RAW. */
    TEST_CHECK(compressed.size <= FDS_LZ_HEADER_SIZE + size, "%s: compressed stream is larger than RAW representation", name);
    if (verbose)
    {
        const char *mode = status == FDS_CMP_COMPRESSED ? "LZSS" : "RAW";
        double ratio = 0.0;
        if (size != 0)
            ratio = (double)compressed.size / (double)size;
        printf("       mode=%s, original=%zu, compressed=%zu, ratio=%.3f\n", mode, size, compressed.size, ratio);
    }
    fds_bb_destroy(&compressed);
    fds_bb_destroy(&decompressed);
    return true;
} /* ========================================================================== */ /* Basic cases                                                                */ /* ========================================================================== */
static bool test_empty(void)
{
    TEST_CASE("empty input");
    return test_roundtrip("empty", NULL, 0, true);
}
static bool test_single_byte(void)
{
    TEST_CASE("single byte");
    uint8_t data = 0xAB;
    return test_roundtrip("single byte", &data, 1, true);
}
static bool test_short_text(void)
{
    TEST_CASE("short text");
    static const uint8_t data[] = "Hello, FDS LZSS!";
    return test_roundtrip("short text", data, sizeof(data) - 1, true);
} /* ========================================================================== */ /* Repetitive data                                                            */ /* ========================================================================== */
static bool test_all_zeroes(void)
{
    TEST_CASE("all zeroes");
    const size_t size = 65536;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed");
    memset(data, 0, size);
    bool ok = test_roundtrip("all zeroes", data, size, true);
    free(data);
    return ok;
}
static bool test_repeating_pattern(void)
{
    TEST_CASE("repeating ABCD pattern");
    const size_t size = 65536;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed");
    static const uint8_t pattern[] = {'A', 'B', 'C', 'D'};
    for (size_t i = 0; i < size; ++i)
        data[i] = pattern[i % sizeof(pattern)];
    bool ok = test_roundtrip("ABCD pattern", data, size, true);
    free(data);
    return ok;
}
static bool test_repeating_text(void)
{
    TEST_CASE("repeating text");
    static const char pattern[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. ";
    const size_t pattern_size = sizeof(pattern) - 1;
    const size_t size = 65536;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed");
    for (size_t i = 0; i < size; ++i)
        data[i] = (uint8_t)pattern[i % pattern_size];
    bool ok = test_roundtrip("repeating text", data, size, true);
    free(data);
    return ok;
} /* ========================================================================== */ /* Different binary distributions                                            */ /* ========================================================================== */
static bool test_all_byte_values(void)
{
    TEST_CASE("all byte values");
    const size_t size = 65536;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed");
    for (size_t i = 0; i < size; ++i)
        data[i] = (uint8_t)i;
    bool ok = test_roundtrip("all byte values", data, size, true);
    free(data);
    return ok;
}
static bool test_random_data(void)
{
    TEST_CASE("random data");
    const size_t size = 65536;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed");
    uint32_t rng = 0x12345678U;
    fill_random(data, size, &rng);
    bool ok = test_roundtrip("random data", data, size, true);
    free(data);
    return ok;
} /* ========================================================================== */ /* Match length boundaries                                                    */ /* ========================================================================== */
static bool test_match_lengths(void)
{
    TEST_CASE("match lengths 3..18"); /* Construct separate sections where useful matches have different lengths.  The data does not need to force a particular token exactly; this primarily exercises all valid encoded match lengths. */
    uint8_t data[18 * 32];
    memset(data, 0x00, sizeof(data));
    for (size_t len = 3; len <= 18; ++len)
    {
        size_t offset = (len - 3) * 32;
        for (size_t i = 0; i < len; ++i)
            data[offset + i] = (uint8_t)('A' + len); /* Put another copy shortly after it. */
        for (size_t i = 0; i < len; ++i)
            data[offset + 19 + i] = (uint8_t)('A' + len);
    }
    return test_roundtrip("match lengths", data, sizeof(data), true);
} /* ========================================================================== */ /* Window boundary                                                            */ /* ========================================================================== */
static bool test_window_boundary(void)
{
    TEST_CASE("4096-byte window boundary"); /* Generate a mostly unique deterministic sequence.  Then put copies at exactly:  distance = 4096  and:  distance = 4095  This specifically exercises the distance encoding and the hash-chain behavior close to the window boundary. */
    const size_t prefix_size = 8192;
    const size_t tail_size = 128;
    const size_t size = prefix_size + tail_size;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed");
    uint32_t rng = 0xCAFEBABEU;
    fill_random(data, prefix_size, &rng); /* Copy bytes from 4096 bytes behind. */
    for (size_t i = 0; i < tail_size; ++i)
    {
        size_t src = 4096 + i;
        if (src >= prefix_size)
            src = i;
        data[prefix_size + i] = data[src];
    } /* Also create a second region whose match is exactly 4095 bytes back. */
    for (size_t i = 0; i < 64; ++i)
        data[8192 + i] = data[4097 + i];
    bool ok = test_roundtrip("window boundary", data, size, true);
    free(data);
    return ok;
} /* ========================================================================== */ /* Very long overlapping match                                                */ /* ========================================================================== */
static bool test_overlap_match(void)
{
    TEST_CASE("overlapping match");
    const size_t size = 65536;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed"); /* A single repeated byte should generate distance=1 matches with long overlapping copies. */
    memset(data, 'A', size);
    bool ok = test_roundtrip("overlap match", data, size, true);
    free(data);
    return ok;
} /* ========================================================================== */ /* Builder reuse                                                              */ /* ========================================================================== */
static bool test_builder_reuse(void)
{
    TEST_CASE("builder reuse");
    FdsBytesBuilder compressed = fds_bb_create(0);
    FdsBytesBuilder decompressed = fds_bb_create(0);
    static const uint8_t inputs[][32] = {{'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A'}, {'H', 'e', 'l', 'l', 'o', ' ', 'F', 'D', 'S', ' ', 'L', 'Z', 'S', 'S', '!'}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
    static const size_t sizes[] = {16, 15, 16};
    for (size_t iteration = 0; iteration < 1000; ++iteration)
    {
        size_t index = iteration % 3;
        FdsBytesView input = {.data = inputs[index], .size = sizes[index]};
        FdsCompressStatus status = fds_compress_lz(input, &compressed);
        TEST_CHECK(status != FDS_CMP_ERROR, "iteration %zu: compression failed", iteration);
        bool ok = fds_decompress_lz(fds_bb_to_view(&compressed), &decompressed);
        TEST_CHECK(ok, "iteration %zu: decompression failed", iteration);
        TEST_CHECK(bytes_equal(decompressed.data, decompressed.size, input.data, input.size), "iteration %zu: data mismatch", iteration);
    }
    fds_bb_destroy(&compressed);
    fds_bb_destroy(&decompressed);
    return true;
} /* ========================================================================== */ /* Randomized fuzz-style round trips                                          */ /* ========================================================================== */
static bool test_randomized(void)
{
    TEST_CASE("randomized round-trip");
    uint32_t rng = 0xA5C31F29U;
    for (size_t iteration = 0; iteration < 5000; ++iteration)
    { /* Keep most tests relatively small, but occasionally produce larger blocks. */
        size_t size;
        switch (iteration % 10)
        {
        case 0:
            size = 0;
            break;
        case 1:
            size = 1 + (rng_next(&rng) % 32);
            break;
        case 2:
            size = 4096;
            break;
        case 3:
            size = 4096 + (rng_next(&rng) % 64);
            break;
        case 4:
            size = 8192;
            break;
        case 5:
            size = 16384;
            break;
        default:
            size = rng_next(&rng) % 8192;
            break;
        }
        uint8_t *data = NULL;
        if (size > 0)
        {
            data = malloc(size);
            TEST_CHECK(data != NULL, "iteration %zu: malloc failed for %zu bytes", iteration, size); /* Mix completely random data with repeated data. This gives much better coverage than random alone. */
            switch (iteration % 6)
            {
            case 0:
                fill_random(data, size, &rng);
                break;
            case 1:
                memset(data, (int)(rng_next(&rng) & 0xFF), size);
                break;
            case 2:
                for (size_t i = 0; i < size; ++i)
                    data[i] = (uint8_t)(i & 0xFF);
                break;
            case 3:
                for (size_t i = 0; i < size; ++i)
                    data[i] = (uint8_t)((i * 17U + 31U) & 0xFF);
                break;
            case 4:
                for (size_t i = 0; i < size; ++i)
                    data[i] = (uint8_t)("ABCD"[i & 3]);
                break;
            default:
                fill_random(data, size, &rng); /* Add a repetitive block in the middle. */
                if (size >= 128)
                {
                    for (size_t i = 0; i < 64; ++i)
                        data[size / 2 + i] = data[i];
                }
                break;
            }
        }
        char name[64];
        snprintf(name, sizeof(name), "random iteration %zu", iteration);
        bool ok = test_roundtrip(name, data, size, false);
        free(data);
        if (!ok)
            return false;
        if ((iteration + 1) % 500 == 0)
        {
            printf("       %zu / 5000 iterations passed\n", iteration + 1);
        }
    }
    return true;
} /* ========================================================================== */ /* Truncated/corrupted stream tests                                           */ /* ========================================================================== */
static bool test_truncated_streams(void)
{
    TEST_CASE("truncated streams");
    const size_t size = 8192;
    uint8_t *data = malloc(size);
    TEST_CHECK(data != NULL, "malloc failed"); /* Use data that should definitely compress. */
    for (size_t i = 0; i < size; ++i)
        data[i] = (uint8_t)("0123456789ABCDEF"[i & 15]);
    FdsBytesBuilder compressed = fds_bb_create(0);
    FdsBytesBuilder decompressed = fds_bb_create(0);
    FdsCompressStatus status = fds_compress_lz((FdsBytesView){.data = data, .size = size}, &compressed);
    TEST_CHECK(status != FDS_CMP_ERROR, "compression failed");
    TEST_CHECK(compressed.size > 0, "compressed stream is empty"); /* Every proper prefix of a valid stream should be rejected.  For a RAW stream this is obvious. For LZSS, no proper prefix should be able to fully reproduce the target size because the valid stream contains no trailing bytes after the payload. */
    for (size_t truncated_size = 0; truncated_size < compressed.size; ++truncated_size)
    {
        FdsBytesView truncated = {.data = compressed.data, .size = truncated_size};
        bool ok = fds_decompress_lz(truncated, &decompressed);
        TEST_CHECK(!ok, "truncated stream of size %zu was accepted", truncated_size);
    }
    fds_bb_destroy(&compressed);
    fds_bb_destroy(&decompressed);
    free(data);
    return true;
} /* ========================================================================== */ /* Header validation                                                          */ /* ========================================================================== */
static bool test_invalid_headers(void)
{
    TEST_CASE("invalid headers");
    FdsBytesBuilder output = fds_bb_create(0);
    {
        static const uint8_t data[] = {0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = data, .size = sizeof(data)}, &output), "invalid magic was accepted");
    }
    {
        static const uint8_t data[] = {0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = data, .size = sizeof(data)}, &output), "invalid magic was accepted");
    }
    {
        static const uint8_t data[] = {0x46, 0x43, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00};
        TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = data, .size = sizeof(data)}, &output), "unknown mode was accepted");
    }
    {
        static const uint8_t data[] = {0x46, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
        TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = data, .size = sizeof(data)}, &output), "non-zero reserved byte was accepted");
    }
    {
        static const uint8_t data[] = {0x46, 0x43, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xAA};
        TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = data, .size = sizeof(data)}, &output), "RAW payload with wrong size was accepted");
    }
    fds_bb_destroy(&output);
    return true;
} /* ========================================================================== */ /* Invalid LZSS tokens                                                        */ /* ========================================================================== */
static bool test_invalid_tokens(void)
{
    TEST_CASE("invalid LZSS tokens");
    FdsBytesBuilder output = fds_bb_create(0); /* Header: FC mode = LZSS reserved = 0 original size = 3  Flags: 0x01 -> token  Token: distance = 1 length = 3  But there are 0 previously decoded bytes, therefore distance=1 is invalid. */
    static const uint8_t invalid_distance[] = {0x46, 0x43, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = invalid_distance, .size = sizeof(invalid_distance)}, &output), "invalid distance token was accepted"); /* Header says original size = 2, token says length = 3.  Decompressor must reject the token instead of producing more bytes than declared. */
    static const uint8_t invalid_length[] = {0x46, 0x43, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    TEST_CHECK(!fds_decompress_lz((FdsBytesView){.data = invalid_length, .size = sizeof(invalid_length)}, &output), "length exceeding target size was accepted");
    fds_bb_destroy(&output);
    return true;
} /* ========================================================================== */ /* Test runner                                                                */ /* ========================================================================== */
static bool run_all_tests(void)
{
    if (!test_empty())
        return false;
    if (!test_single_byte())
        return false;
    if (!test_short_text())
        return false;
    if (!test_all_zeroes())
        return false;
    if (!test_repeating_pattern())
        return false;
    if (!test_repeating_text())
        return false;
    if (!test_all_byte_values())
        return false;
    if (!test_random_data())
        return false;
    if (!test_match_lengths())
        return false;
    if (!test_window_boundary())
        return false;
    if (!test_overlap_match())
        return false;
    if (!test_builder_reuse())
        return false;
    if (!test_randomized())
        return false;
    if (!test_truncated_streams())
        return false;
    if (!test_invalid_headers())
        return false;
    if (!test_invalid_tokens())
        return false;
    return true;
} /* ========================================================================== */ /* Main                                                                       */ /* ========================================================================== */
int main(void)
{
    printf("========================================\n");
    printf(" FDS LZSS TEST SUITE\n");
    printf("========================================\n");
    bool ok = run_all_tests();
    printf("\n========================================\n");
    if (ok && g_tests_failed == 0)
    {
        printf("ALL TESTS PASSED\n");
    }
    else
    {
        printf("TESTS FAILED: %d failure(s)\n", g_tests_failed);
    }
    printf("Checks executed: %d\n", g_tests_run);
    printf("========================================\n");
    return ok && g_tests_failed == 0 ? 0 : 1;
}