/*
    fds_lz.h — Small LZSS compression/decompression module for FDS.

    Depends on:
        fds.h

    Single-header usage:

        // One translation unit:
        #define FDS_LZ_IMPLEMENTATION
        #include "fds_lz.h"

        // Other translation units:
        #include "fds_lz.h"
*/

#ifndef FDS_LZ_H
#define FDS_LZ_H

#include "../fds.h"

#ifndef FDS_LZ_MAGIC_0
#define FDS_LZ_MAGIC_0 0x46 // 'F'
#endif

#ifndef FDS_LZ_MAGIC_1
#define FDS_LZ_MAGIC_1 0x43 // 'C'
#endif

#ifndef FDS_LZ_MODE_RAW
#define FDS_LZ_MODE_RAW 0x00
#endif

#ifndef FDS_LZ_MODE_LZSS
#define FDS_LZ_MODE_LZSS 0x01
#endif

#ifndef FDS_LZ_WINDOW_SIZE
#define FDS_LZ_WINDOW_SIZE 4096
#endif

#ifndef FDS_LZ_MIN_MATCH
#define FDS_LZ_MIN_MATCH 3
#endif

#ifndef FDS_LZ_MAX_MATCH
#define FDS_LZ_MAX_MATCH 18
#endif

#ifndef FDS_LZ_HASH_SIZE
#define FDS_LZ_HASH_SIZE 4096
#endif

/*
    Maximum amount of hash-chain nodes inspected for one match.

    Larger values can slightly improve compression on repetitive data,
    but increase compression time significantly.
*/
#ifndef FDS_LZ_MAX_CHAIN
#define FDS_LZ_MAX_CHAIN 256
#endif

#define FDS_LZ_HEADER_SIZE 8

/*
    Format:

        +0  : 'F'
        +1  : 'C'
        +2  : mode
        +3  : reserved (must be 0)
        +4  : original size, u32 LE

    RAW:
        payload = original bytes

    LZSS:
        groups of:

            1 byte flags
            up to 8 items

        flags bit:
            0 = literal byte
            1 = 16-bit match token, little-endian

        token:
            bits 15..4 = distance - 1   [0 .. 4095]
            bits  3..0 = length - 3     [0 .. 15]
*/

_Static_assert(
    (FDS_LZ_HASH_SIZE & (FDS_LZ_HASH_SIZE - 1)) == 0,
    "FDS_LZ_HASH_SIZE must be a power of two");

_Static_assert(
    FDS_LZ_WINDOW_SIZE >= 1 &&
        FDS_LZ_WINDOW_SIZE <= 4096,
    "FDS_LZ_WINDOW_SIZE must be in range 1..4096");

_Static_assert(
    FDS_LZ_MIN_MATCH >= 2 &&
        FDS_LZ_MAX_MATCH >= FDS_LZ_MIN_MATCH &&
        (FDS_LZ_MAX_MATCH - FDS_LZ_MIN_MATCH) <= 15,
    "LZSS length field supports only 16 length values");

_Static_assert(
    FDS_LZ_HEADER_SIZE == 8,
    "Unexpected FDS LZ header size");

FdsCompressStatus fds_compress_lz(
    FdsBytesView input,
    FdsBytesBuilder *out_builder);

bool fds_decompress_lz(
    FdsBytesView input,
    FdsBytesBuilder *out_builder);

#endif /* FDS_LZ_H */

#if defined(FDS_LZ_IMPLEMENTATION) || defined(FDS_LZ_IMPL)

#ifndef FDS_LZ_IMPLEMENTATION_INCLUDED
#define FDS_LZ_IMPLEMENTATION_INCLUDED

/* ========================================================================== */
/* Internal helpers                                                           */
/* ========================================================================== */

#define FDS_LZ_HASH_NONE UINT32_MAX

static u32
fds_lz_hash3(const u8 *p)
{
    u32 h =
        ((u32)p[0] * 251u) ^
        ((u32)p[1] * 509u) ^
        ((u32)p[2]);

    return h & (FDS_LZ_HASH_SIZE - 1u);
}

static void fds_lz_insert_position(FdsBytesView input, usize pos, u32 *hash_head, u32 *hash_prev)
{
    if (input.size - pos < FDS_LZ_MIN_MATCH)
        return;
    u32 h = fds_lz_hash3(input.data + pos);
    usize slot = pos % FDS_LZ_WINDOW_SIZE;
    hash_prev[slot] = hash_head[h];
    hash_head[h] = (u32)pos;
}
static void fds_lz_find_match(FdsBytesView input, usize pos, const u32 *hash_head, const u32 *hash_prev, usize *out_len, usize *out_dist)
{
    *out_len = 0;
    *out_dist = 0;
    if (input.size - pos < FDS_LZ_MIN_MATCH)
        return;
    u32 h = fds_lz_hash3(input.data + pos);
    u32 candidate = hash_head[h];
    usize chain_count = 0;
    while (candidate != FDS_LZ_HASH_NONE && chain_count < FDS_LZ_MAX_CHAIN)
    { 
        if ((usize)candidate >= pos)
            break;
        usize dist = pos - (usize)candidate;
        if (dist > FDS_LZ_WINDOW_SIZE)
            break;
        usize max_len = input.size - pos;
        if (max_len > FDS_LZ_MAX_MATCH)
            max_len = FDS_LZ_MAX_MATCH;
        usize len = 0;
        while (len < max_len && input.data[(usize)candidate + len] == input.data[pos + len])
        {
            ++len;
        }
        if (len > *out_len)
        {
            *out_len = len;
            *out_dist = dist;
            if (len == FDS_LZ_MAX_MATCH)
                break;
        }
        ++chain_count;
        if (dist >= FDS_LZ_WINDOW_SIZE)
            break;
        candidate = hash_prev[(usize)candidate % FDS_LZ_WINDOW_SIZE];
    }
}
static bool fds_lz_find_lazy_match(FdsBytesView input, usize pos, const u32 *hash_head, const u32 *hash_prev, usize *out_len, usize *out_dist, u32 *temp_hash, u32 *temp_old_head, u32 *temp_old_prev)
{
    *out_len = 0;
    *out_dist = 0;
    if (input.size - pos < FDS_LZ_MIN_MATCH) return false;
    usize next_pos = pos + 1;
    if (input.size - next_pos < FDS_LZ_MIN_MATCH) return false; 
    u32 h = fds_lz_hash3(input.data + pos);
    usize slot = pos % FDS_LZ_WINDOW_SIZE;
    *temp_hash = h;
    *temp_old_head = hash_head[h];
    *temp_old_prev = hash_prev[slot];
    u32 *mutable_head = (u32 *)(uintptr_t)hash_head;
    u32 *mutable_prev = (u32 *)(uintptr_t)hash_prev;
    mutable_prev[slot] = mutable_head[h];
    mutable_head[h] = (u32)pos;
    fds_lz_find_match(input, next_pos, mutable_head, mutable_prev, out_len, out_dist);
    mutable_head[h] = *temp_old_head;
    mutable_prev[slot] = *temp_old_prev;
    return *out_len >= FDS_LZ_MIN_MATCH;
}   
FdsCompressStatus fds_compress_lz(FdsBytesView input, FdsBytesBuilder *out_builder)
{
    if (!out_builder)                  return FDS_CMP_ERROR;
    if (input.size > 0 && !input.data) return FDS_CMP_ERROR; 
    if (input.size > UINT32_MAX)       return FDS_CMP_ERROR;
    out_builder->size = 0;
    if (input.size == 0)
    {
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
        fds_bb_append_byte(out_builder, FDS_LZ_MODE_RAW);
        fds_bb_append_byte(out_builder, 0x00);
        fds_bb_append_u32_le(out_builder, 0);
        return FDS_CMP_STORED;
    }
    fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
    fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
    fds_bb_append_byte(out_builder, FDS_LZ_MODE_LZSS);
    fds_bb_append_byte(out_builder, 0x00);
    fds_bb_append_u32_le(out_builder, (u32)input.size);
    u32 hash_head[FDS_LZ_HASH_SIZE];
    u32 hash_prev[FDS_LZ_WINDOW_SIZE];
    for (usize i = 0; i < FDS_LZ_HASH_SIZE; ++i)
        hash_head[i] = FDS_LZ_HASH_NONE;
    for (usize i = 0; i < FDS_LZ_WINDOW_SIZE; ++i)
        hash_prev[i] = FDS_LZ_HASH_NONE;
    usize pos = 0;
    while (pos < input.size)
    {
        if (out_builder->size >= FDS_LZ_HEADER_SIZE + input.size) break;
        usize flags_offset = out_builder->size;
        fds_bb_append_byte(out_builder, 0);
        u8 flags = 0;
        for (s32 bit = 0; bit < 8 && pos < input.size; ++bit)
        {
            usize match_len = 0;
            usize match_dist = 0;
            fds_lz_find_match(input, pos, hash_head, hash_prev, &match_len, &match_dist); 
            bool use_lazy_literal = false;
            if (match_len >= FDS_LZ_MIN_MATCH && input.size - pos > 1 && input.size - (pos + 1) >= FDS_LZ_MIN_MATCH)
            {  
                u32 temp_hash = 0;
                u32 temp_old_head = 0;
                u32 temp_old_prev = 0;
                usize next_len = 0;
                usize next_dist = 0;
                if (fds_lz_find_lazy_match(input, pos, hash_head, hash_prev, &next_len, &next_dist, &temp_hash, &temp_old_head, &temp_old_prev))
                {
                    (void)next_dist;
                    if (next_len > match_len + 1) use_lazy_literal = true;
                }
            }
            if (match_len >= FDS_LZ_MIN_MATCH && !use_lazy_literal)
            {
                usize max_dist = FDS_LZ_WINDOW_SIZE;
                if (match_dist < 1 || match_dist > max_dist)
                {
                    use_lazy_literal = true;
                }
                else
                {
                    flags |= (u8)(1u << bit);
                    u16 dist_enc = (u16)(match_dist - 1);
                    u16 len_enc = (u16)(match_len - FDS_LZ_MIN_MATCH);
                    u16 token = (u16)((dist_enc << 4) | (len_enc & 0x0F));
                    fds_bb_append_u16_le(out_builder, token);
                    for (usize k = 0; k < match_len; ++k)
                    {
                        usize insert_pos = pos + k;
                        if (input.size - insert_pos >= FDS_LZ_MIN_MATCH)
                        {
                            fds_lz_insert_position(input, insert_pos, hash_head, hash_prev);
                        }
                    }
                    pos += match_len;
                }
            }
            if (match_len < FDS_LZ_MIN_MATCH || use_lazy_literal)
            {
                fds_bb_append_byte(out_builder, input.data[pos]);
                fds_lz_insert_position(input, pos, hash_head, hash_prev);
                ++pos;
            }
        }
        out_builder->data[flags_offset] = flags;
    }
    usize compressed_size = out_builder->size; 
    if (compressed_size >= FDS_LZ_HEADER_SIZE + input.size)
    {
        out_builder->size = 0;
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
        fds_bb_append_byte(out_builder, FDS_LZ_MODE_RAW);
        fds_bb_append_byte(out_builder, 0x00);
        fds_bb_append_u32_le(out_builder, (u32)input.size);
        fds_bb_append(out_builder, input.data, input.size);
        return FDS_CMP_STORED;
    }
    return FDS_CMP_COMPRESSED;
}

bool fds_decompress_lz(FdsBytesView input, FdsBytesBuilder *out_builder)
{
    if (!out_builder)
        return false;
    out_builder->size = 0;
#define FDS_LZ_DECOMP_FAIL()   \
    do                         \
    {                          \
        out_builder->size = 0; \
        return false;          \
    } while (0)

    
    if (input.size < FDS_LZ_HEADER_SIZE)         FDS_LZ_DECOMP_FAIL();
    u8 m0, m1, mode, reserved;
    u32 orig_size;
    if (!fds_bv_pop_byte(&input, &m0))           FDS_LZ_DECOMP_FAIL();
    if (m0 != FDS_LZ_MAGIC_0)                    FDS_LZ_DECOMP_FAIL();
    if (!fds_bv_pop_byte(&input, &m1))           FDS_LZ_DECOMP_FAIL();
    if (m1 != FDS_LZ_MAGIC_1)                    FDS_LZ_DECOMP_FAIL();
    if (!fds_bv_pop_byte(&input, &mode))         FDS_LZ_DECOMP_FAIL();
    if (!fds_bv_pop_byte(&input, &reserved))     FDS_LZ_DECOMP_FAIL();
    if (!fds_bv_read_u32_le(&input, &orig_size)) FDS_LZ_DECOMP_FAIL();
    if (reserved != 0)                           FDS_LZ_DECOMP_FAIL();
    usize target_size = (usize)orig_size;
    
    if (mode == FDS_LZ_MODE_RAW)
    { 
        if (input.size != target_size)
            FDS_LZ_DECOMP_FAIL();
        fds_bb_reserve(out_builder, target_size);
        if (target_size > 0)
            fds_bb_append(out_builder, input.data, target_size);
        return out_builder->size == target_size;
    } 
    if (mode == FDS_LZ_MODE_LZSS)
    {
      fds_bb_reserve(out_builder, target_size);
        while (out_builder->size < target_size)
        {
            u8 flags;
            if (!fds_bv_pop_byte(&input, &flags)) FDS_LZ_DECOMP_FAIL();
            for (s32 bit = 0; bit < 8 && out_builder->size < target_size; ++bit)
            {
                if ((flags & (u8)(1u << bit)) == 0)
                {
                    u8 byte;
                    if (!fds_bv_pop_byte(&input, &byte)) FDS_LZ_DECOMP_FAIL();
                    fds_bb_append_byte(out_builder, byte);
                }
                else
                {
                    u16 token;
                    if (!fds_bv_read_u16_le(&input, &token)) FDS_LZ_DECOMP_FAIL();
                    usize dist = (usize)((token >> 4) + 1u);
                    usize len = (usize)((token & 0x0Fu) + FDS_LZ_MIN_MATCH);
                    if (dist < 1 || dist > FDS_LZ_WINDOW_SIZE)  FDS_LZ_DECOMP_FAIL();
                    if (len > target_size - out_builder->size)  FDS_LZ_DECOMP_FAIL();
                    if (dist > out_builder->size)               FDS_LZ_DECOMP_FAIL();
                    usize src_start = out_builder->size - dist;
                    fds_bb_reserve(out_builder, len);                        
                    for (usize i = 0; i < len; ++i)
                    {
                        out_builder->data[out_builder->size++] = out_builder->data[src_start + i];
                    }
                }
            }
        }
        if (out_builder->size != target_size) FDS_LZ_DECOMP_FAIL();
        if (input.size != 0)                  FDS_LZ_DECOMP_FAIL();
        return true;
    } 
    FDS_LZ_DECOMP_FAIL();
#undef FDS_LZ_DECOMP_FAIL return false;
}
#endif 

#endif 