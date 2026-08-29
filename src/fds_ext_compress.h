/*
    fds_ext_compress.h — Smart LZSS compression module with RAW fallback (FDS Extension).

    Features:
    - Optimal Hash-Chain Search: Deep lookup for max compression ratio.
    - Automatic RAW fallback: If compressed data expands, it saves original bytes instead.
    - Embedded Header: Stores Magic ("FC"), Compression Mode, and Original Size.
    - Strict Validation: Protects against buffer overflows, invalid trailing bytes, and out-of-bounds tokens.
*/

#ifndef FDS_EXT_COMPRESS_H
#define FDS_EXT_COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef FDS_EXT_CMP_DEF
#define FDS_EXT_CMP_DEF extern
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_COMPRESS_H

/* ============================================================================
   IMPLEMENTATION
   ============================================================================ */
#ifdef FDS_EXT_COMPRESS_IMPL

#include <string.h>



#endif // FDS_EXT_COMPRESS_IMPLEMENTATION