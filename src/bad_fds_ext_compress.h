/*
    fds_ext_compress.h — STB-style lightweight data compression (FDS).
    
    Features:
    - Implements a Hybrid RLE (Run-Length Encoding) algorithm.
    - Safe: never significantly expands random/uncompressible data.
    - Zero external dependencies: relies only on fds_bytes_view and fds_bytes_builder.
    - Fast: single-pass compression and decompression.

    Usage:
    1. Make sure "fds_bytes_view.h" and "fds_bytes_builder.h" are available.
    2. #include "fds_ext_compress.h"
    3. In EXACTLY ONE C file, add:
       #define FDS_EXT_COMPRESS_IMPLEMENTATION
       #include "fds_ext_compress.h"
*/

#ifndef FDS_EXT_COMPRESS_H
#define FDS_EXT_COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>



#ifndef FDS_ASSERT
    #include <assert.h>
    #define FDS_ASSERT(cond, msg) assert((cond) && (msg))
#endif

#ifndef FDS_EXT_COMP_DEF
    #define FDS_EXT_COMP_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// COMPRESSION API
// ============================================================================

// Compresses data from a BytesView and appends it to a BytesBuilder.
// Returns true on success.
// e.g., fds_ext_compress_rle(fds_bv(raw_data, size), &out_builder);
FDS_EXT_COMP_DEF bool fds_ext_compress_rle(FdsBytesView input, FdsBytesBuilder *out_bb);

// Decompresses RLE-encoded data from a BytesView and appends it to a BytesBuilder.
// Returns true on success, false if the compressed data is corrupted or truncated.
// e.g., fds_ext_decompress_rle(fds_bv(comp_data, comp_size), &out_builder);
FDS_EXT_COMP_DEF bool fds_ext_decompress_rle(FdsBytesView input, FdsBytesBuilder *out_bb);

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_COMPRESS_H

/* ============================================================================
   IMPLEMENTATION
   ============================================================================ */
#ifdef FDS_EXT_COMPRESS_IMPL

// Helper to count consecutive repeating bytes (max 130 for our encoding)
static size_t fds_ext_rle_count_run(FdsBytesView view) {
    if (view.size == 0) return 0;
    size_t count = 1;
    uint8_t byte = view.data[0];
    
    // Max run length we can encode in 7 bits with a +3 offset is 130
    // (127 + 3 = 130).
    while (count < 130 && count < view.size && view.data[count] == byte) {
        count++;
    }
    return count;
}

// Helper to count non-repeating literal bytes (max 128)
static size_t fds_ext_rle_count_literal(FdsBytesView view) {
    size_t count = 0;
    while (count < 128 && count < view.size) {
        // If we see 3 of the same bytes in a row, it's better to start a run.
        if (count + 2 < view.size &&
            view.data[count] == view.data[count + 1] &&
            view.data[count] == view.data[count + 2]) {
            break;
        }
        count++;
    }
    return count;
}

FDS_EXT_COMP_DEF bool fds_ext_compress_rle(FdsBytesView input, FdsBytesBuilder *out_bb) {
    FDS_ASSERT(out_bb != NULL, "Output builder is NULL");
    if (input.size == 0) return true; // Nothing to compress

    while (input.size > 0) {
        size_t run_len = fds_ext_rle_count_run(input);

        if (run_len >= 3) {
            // Encode as a run. 
            // MSB is 1 (0x80). Remaining 7 bits store (length - 3).
            uint8_t token = 0x80 | (uint8_t)(run_len - 3);
            fds_bb_append_byte(out_bb, token);
            fds_bb_append_byte(out_bb, input.data[0]); // The byte being repeated
            
            input = fds_bv_skip(input, run_len);
        } else {
            // Encode as literal bytes.
            // MSB is 0. Remaining 7 bits store (length - 1).
            size_t lit_len = fds_ext_rle_count_literal(input);
            if (lit_len == 0) lit_len = 1; // Fallback for safety

            uint8_t token = (uint8_t)(lit_len - 1);
            fds_bb_append_byte(out_bb, token);
            
            FdsBytesView literal_data = fds_bv_pop_bytes(&input, lit_len);
            fds_bb_append_view(out_bb, literal_data);
        }
    }
    return true;
}

FDS_EXT_COMP_DEF bool fds_ext_decompress_rle(FdsBytesView input, FdsBytesBuilder *out_bb) {
    FDS_ASSERT(out_bb != NULL, "Output builder is NULL");
    
    while (input.size > 0) {
        uint8_t token;
        if (!fds_bv_pop_byte(&input, &token)) return false;

        if ((token & 0x80) == 0) {
            // Literal block
            size_t lit_len = (size_t)token + 1;
            
            if (input.size < lit_len) return false; // Corrupted: unexpected EOF
            
            FdsBytesView literal_data = fds_bv_pop_bytes(&input, lit_len);
            fds_bb_append_view(out_bb, literal_data);
        } else {
            // Run block
            size_t run_len = (size_t)(token & 0x7F) + 3;
            
            uint8_t byte;
            if (!fds_bv_pop_byte(&input, &byte)) return false; // Corrupted: missing run byte
            
            // Append the byte 'run_len' times
            fds_bb_reserve(out_bb, run_len);
            for (size_t i = 0; i < run_len; ++i) {
                out_bb->data[out_bb->size++] = byte;
            }
        }
    }
    return true;
}

#endif // FDS_EXT_COMPRESS_IMPLEMENTATION