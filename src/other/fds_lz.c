#define FDS_LZ_MAGIC_0 0x46 // 'F'
#define FDS_LZ_MAGIC_1 0x43 // 'C'

#define FDS_LZ_MODE_RAW 0x00
#define FDS_LZ_MODE_LZSS 0x01

#define FDS_LZ_WINDOW_SIZE 4096
#define FDS_LZ_MIN_MATCH 3
#define FDS_LZ_MAX_MATCH 18
#define FDS_LZ_HASH_SIZE 4096

// Compression
FdsCompressStatus fds_compress_lz(FdsBytesView input, FdsBytesBuilder *out_builder);
bool fds_decompress_lz(FdsBytesView input, FdsBytesBuilder *out_builder);


 // Compression fuctions Start ================================================================================================================
    FdsCompressStatus fds_compress_lz(FdsBytesView input, FdsBytesBuilder *out_builder)
    {
        if (!out_builder)
            return FDS_CMP_ERROR;
        if (input.size > 0 && !input.data)
            return FDS_CMP_ERROR;
        if (input.size > UINT32_MAX)
            return FDS_CMP_ERROR;

        // Примусово очищуємо білдер, щоб уникнути проблеми "+17 байт" при повторному використанні
        out_builder->size = 0;
        usize start_pos = 0;

        if (input.size == 0)
        {
            fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
            fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
            fds_bb_append_byte(out_builder, FDS_LZ_MODE_RAW);
            fds_bb_append_byte(out_builder, 0x00);
            fds_bb_append_u32_le(out_builder, 0);
            return FDS_CMP_STORED;
        }

        // 1. Заголовок
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
        fds_bb_append_byte(out_builder, FDS_LZ_MODE_LZSS);
        fds_bb_append_byte(out_builder, 0x00);
        fds_bb_append_u32_le(out_builder, (u32)input.size);

        usize header_size = 8;

        // Ланцюжки хешів для глибокого пошуку (забезпечує максимальне стиснення)
        s32 hash_head[FDS_LZ_HASH_SIZE];
        s32 hash_prev[FDS_LZ_WINDOW_SIZE];
        for (s32 i = 0; i < FDS_LZ_HASH_SIZE; ++i)
            hash_head[i] = -1;

        usize pos = 0;

        // 2. Стиснення LZSS
        while (pos < input.size)
        {
            usize flags_offset = out_builder->size;
            fds_bb_append_byte(out_builder, 0x00);
            u8 flags = 0;

            for (s32 bit = 0; bit < 8 && pos < input.size; ++bit)
            {
                usize match_len = 0;
                usize match_dist = 0;

                if (pos + FDS_LZ_MIN_MATCH <= input.size)
                {
                    u32 h = ((u32)input.data[pos] * 251u) ^
                                 ((u32)input.data[pos + 1] * 509u) ^
                                 (u32)input.data[pos + 2];
                    h &= (FDS_LZ_HASH_SIZE - 1);

                    s32 candidate = hash_head[h];
                    s32 limit = 256; // Шукаємо до 256 вузлів вглиб історії

                    while (candidate >= 0 && (pos - candidate) <= FDS_LZ_WINDOW_SIZE && limit-- > 0)
                    {
                        usize dist = pos - candidate;
                        usize max_len = input.size - pos;
                        if (max_len > FDS_LZ_MAX_MATCH)
                            max_len = FDS_LZ_MAX_MATCH;

                        usize len = 0;
                        while (len < max_len && input.data[candidate + len] == input.data[pos + len])
                        {
                            len++;
                        }

                        if (len > match_len)
                        {
                            match_len = len;
                            match_dist = dist;
                            if (match_len == FDS_LZ_MAX_MATCH)
                                break; // Знайшли ідеал - виходимо
                        }
                        candidate = hash_prev[candidate % FDS_LZ_WINDOW_SIZE];
                    }
                }

                if (match_len >= FDS_LZ_MIN_MATCH)
                {
                    flags |= (u8)(1 << bit);
                    u16 dist_enc = (u16)(match_dist - 1);
                    u16 len_enc = (u16)(match_len - FDS_LZ_MIN_MATCH);
                    u16 token = (dist_enc << 4) | (len_enc & 0x0F);

                    fds_bb_append_u16_le(out_builder, token);

                    // Записуємо пропущені байти в словник
                    for (usize k = 0; k < match_len; ++k)
                    {
                        if (pos + k + FDS_LZ_MIN_MATCH <= input.size)
                        {
                            u32 h = ((u32)input.data[pos + k] * 251u) ^
                                         ((u32)input.data[pos + k + 1] * 509u) ^
                                         (u32)input.data[pos + k + 2];
                            h &= (FDS_LZ_HASH_SIZE - 1);
                            hash_prev[(pos + k) % FDS_LZ_WINDOW_SIZE] = hash_head[h];
                            hash_head[h] = (s32)(pos + k);
                        }
                    }
                    pos += match_len;
                }
                else
                {
                    if (pos + FDS_LZ_MIN_MATCH <= input.size)
                    {
                        u32 h = ((u32)input.data[pos] * 251u) ^
                                     ((u32)input.data[pos + 1] * 509u) ^
                                     (u32)input.data[pos + 2];
                        h &= (FDS_LZ_HASH_SIZE - 1);
                        hash_prev[pos % FDS_LZ_WINDOW_SIZE] = hash_head[h];
                        hash_head[h] = (s32)pos;
                    }
                    fds_bb_append_byte(out_builder, input.data[pos++]);
                }
            }

            out_builder->data[flags_offset] = flags;

            // Ранній вихід, якщо стиснення неефективне
            if ((out_builder->size - start_pos) >= (input.size + header_size))
            {
                break;
            }
        }

        usize total_compressed_size = out_builder->size - start_pos;

        // 3. Fallback (збереження RAW)
        if (total_compressed_size >= (input.size + header_size))
        {
            out_builder->size = start_pos;

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

        // Примусово обнуляємо для точного співпадіння розмірів з оригіналом (без append)
        out_builder->size = 0;
        usize start_size = 0;

#define FDS_DECOMP_FAIL()               \
    do                                  \
    {                                   \
        out_builder->size = start_size; \
        return false;                   \
    } while (0)

        // 1. Читання заголовка
        u8 m0, m1, mode, reserved;
        u32 orig_size;

        if (!fds_bv_pop_byte(&input, &m0) || m0 != FDS_LZ_MAGIC_0)
            FDS_DECOMP_FAIL();
        if (!fds_bv_pop_byte(&input, &m1) || m1 != FDS_LZ_MAGIC_1)
            FDS_DECOMP_FAIL();
        if (!fds_bv_pop_byte(&input, &mode))
            FDS_DECOMP_FAIL();
        if (!fds_bv_pop_byte(&input, &reserved))
            FDS_DECOMP_FAIL();
        if (!fds_bv_read_u32_le(&input, &orig_size))
            FDS_DECOMP_FAIL();

        if (reserved != 0)
            FDS_DECOMP_FAIL();

        usize target_size = start_size + orig_size;

        // 2. Декомпресія RAW
        if (mode == FDS_LZ_MODE_RAW)
        {
            if (input.size != orig_size)
                FDS_DECOMP_FAIL();

            fds_bb_reserve(out_builder, orig_size);
            fds_bb_append(out_builder, input.data, orig_size);
            return true;
        }

        // 3. Декомпресія LZSS
        if (mode == FDS_LZ_MODE_LZSS)
        {
            fds_bb_reserve(out_builder, orig_size);

            while (input.size > 0 && out_builder->size < target_size)
            {
                u8 flags;
                if (!fds_bv_pop_byte(&input, &flags))
                    FDS_DECOMP_FAIL();

                for (s32 bit = 0; bit < 8; ++bit)
                {
                    if (out_builder->size >= target_size)
                        break;

                    if ((flags & (1 << bit)) == 0)
                    {
                        u8 byte;
                        if (!fds_bv_pop_byte(&input, &byte))
                            FDS_DECOMP_FAIL();
                        fds_bb_append_byte(out_builder, byte);
                    }
                    else
                    {
                        u16 token;
                        if (!fds_bv_read_u16_le(&input, &token))
                            FDS_DECOMP_FAIL();

                        usize dist = (token >> 4) + 1;
                        usize len = (token & 0x0F) + FDS_LZ_MIN_MATCH;

                        if (len > target_size - out_builder->size)
                            FDS_DECOMP_FAIL();
                        if (dist > (out_builder->size - start_size))
                            FDS_DECOMP_FAIL();

                        usize src_start = out_builder->size - dist;
                        fds_bb_reserve(out_builder, len);
                        for (usize i = 0; i < len; ++i)
                        {
                            out_builder->data[out_builder->size++] = out_builder->data[src_start + i];
                        }
                    }
                }
            }

            if (out_builder->size != target_size || input.size > 0)
            {
                FDS_DECOMP_FAIL();
            }
            return true;
        }

        FDS_DECOMP_FAIL();

#undef FDS_DECOMP_FAIL
    }
    // Compression fuctions End ================================================================================================================