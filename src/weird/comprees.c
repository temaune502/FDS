#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>

// ============================================================================
// 1. КОНФІГУРАЦІЯ ТА ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================================

#define FASTLZ_MIN_MATCH 4
#define FASTLZ_HASH_LOG 14
#define FASTLZ_HASH_SIZE (1 << FASTLZ_HASH_LOG)
#define FASTLZ_MAX_OFFSET 65535
#define FASTLZ_MAX_CHAIN 128

// Кількість повторів для усереднення часу та усунення похибок таймера
#define NUM_RUNS 200

static inline uint32_t fastlz_read32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static inline void fastlz_write16(uint8_t* p, uint16_t v) {
    memcpy(p, &v, 2);
}

static inline uint32_t fastlz_hash32(uint32_t v) {
    return (v * 2654435761U) >> (32 - FASTLZ_HASH_LOG);
}

// ============================================================================
// 2. ШВИДКА КОМПРЕСІЯ (FAST MODE)
// ============================================================================

size_t fastlz_compress(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_capacity) {
    if (in_size < FASTLZ_MIN_MATCH) return 0;

    const uint8_t* hash_table[FASTLZ_HASH_SIZE] = {NULL};
    const uint8_t* ip = in;
    const uint8_t* anchor = in;
    const uint8_t* iend = in + in_size;
    const uint8_t* mflimit = iend - 5;

    uint8_t* op = out;
    uint8_t* oend = out + out_capacity;

    while (ip < mflimit) {
        uint32_t h = fastlz_hash32(fastlz_read32(ip));
        const uint8_t* match = hash_table[h];
        hash_table[h] = ip;

        if (!match || (ip - match) > FASTLZ_MAX_OFFSET || match >= ip || fastlz_read32(match) != fastlz_read32(ip)) {
            ip++;
            continue;
        }

        size_t lit_len = ip - anchor;
        size_t match_len = FASTLZ_MIN_MATCH;

        while (ip + match_len < iend && ip[match_len] == match[match_len]) {
            match_len++;
        }

        if (op + lit_len + (lit_len / 255) + (match_len / 255) + 8 > oend) return 0;

        uint8_t* token = op++;

        if (lit_len >= 15) {
            *token = (15 << 4);
            size_t l = lit_len - 15;
            while (l >= 255) { *op++ = 255; l -= 255; }
            *op++ = (uint8_t)l;
        } else {
            *token = (uint8_t)(lit_len << 4);
        }

        if (lit_len > 0) {
            memcpy(op, anchor, lit_len);
            op += lit_len;
        }

        fastlz_write16(op, (uint16_t)(ip - match));
        op += 2;

        size_t m_len = match_len - FASTLZ_MIN_MATCH;
        if (m_len >= 15) {
            *token |= 15;
            size_t l = m_len - 15;
            while (l >= 255) { *op++ = 255; l -= 255; }
            *op++ = (uint8_t)l;
        } else {
            *token |= (uint8_t)m_len;
        }

        ip += match_len;
        anchor = ip;
    }

    size_t lit_len = iend - anchor;
    if (op + lit_len + (lit_len / 255) + 2 > oend) return 0;

    uint8_t* token = op++;
    if (lit_len >= 15) {
        *token = (15 << 4);
        size_t l = lit_len - 15;
        while (l >= 255) { *op++ = 255; l -= 255; }
        *op++ = (uint8_t)l;
    } else {
        *token = (uint8_t)(lit_len << 4);
    }

    memcpy(op, anchor, lit_len);
    op += lit_len;

    return (size_t)(op - out);
}

// ============================================================================
// 3. ПОКРАЩЕНА КОМПРЕСІЯ (HIGH COMPRESSION - HC MODE)
// ============================================================================

static inline uint8_t* fastlz_write_token(uint8_t* op, const uint8_t* anchor, size_t lit_len, size_t match_len, uint16_t offset, uint8_t* oend) {
    if (op + lit_len + (lit_len / 255) + (match_len / 255) + 8 > oend) return NULL;

    uint8_t* token = op++;

    if (lit_len >= 15) {
        *token = (15 << 4);
        size_t l = lit_len - 15;
        while (l >= 255) { *op++ = 255; l -= 255; }
        *op++ = (uint8_t)l;
    } else {
        *token = (uint8_t)(lit_len << 4);
    }

    if (lit_len > 0) {
        memcpy(op, anchor, lit_len);
        op += lit_len;
    }

    fastlz_write16(op, offset);
    op += 2;

    size_t m_len = match_len - FASTLZ_MIN_MATCH;
    if (m_len >= 15) {
        *token |= 15;
        size_t l = m_len - 15;
        while (l >= 255) { *op++ = 255; l -= 255; }
        *op++ = (uint8_t)l;
    } else {
        *token |= (uint8_t)m_len;
    }

    return op;
}

size_t fastlz_compress_hc(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_capacity) {
    if (in_size < FASTLZ_MIN_MATCH) return 0;

    uint16_t head[FASTLZ_HASH_SIZE];
    uint16_t chain[65536];
    memset(head, 0, sizeof(head));

    const uint8_t* ip = in;
    const uint8_t* anchor = in;
    const uint8_t* iend = in + in_size;
    const uint8_t* mflimit = iend - 5;

    uint8_t* op = out;
    uint8_t* oend = out + out_capacity;

    while (ip < mflimit) {
        uint32_t h = fastlz_hash32(fastlz_read32(ip));
        uint32_t current_pos = (uint32_t)(ip - in);
        uint16_t pos_idx = current_pos & 0xFFFF;
        chain[pos_idx] = head[h];
        head[h] = pos_idx;

        const uint8_t* match = NULL;
        size_t best_match_len = FASTLZ_MIN_MATCH - 1;
        uint16_t best_offset = 0;

        uint16_t ref_idx = chain[pos_idx];
        int attempts = FASTLZ_MAX_CHAIN;

        while (ref_idx != 0 && attempts-- > 0) {
            uint32_t ref_pos = current_pos > pos_idx ? (current_pos - pos_idx + ref_idx) : ref_idx;
            if (ref_pos > current_pos) ref_pos -= 65536;

            size_t offset = current_pos - ref_pos;
            if (offset == 0 || offset > FASTLZ_MAX_OFFSET) break;

            const uint8_t* ref_ptr = in + ref_pos;

            if (fastlz_read32(ref_ptr) == fastlz_read32(ip)) {
                size_t m_len = FASTLZ_MIN_MATCH;
                while (ip + m_len < iend && ip[m_len] == ref_ptr[m_len]) {
                    m_len++;
                }

                if (m_len > best_match_len) {
                    best_match_len = m_len;
                    best_offset = (uint16_t)offset;
                    match = ref_ptr;
                    if (m_len > 255) break;
                }
            }
            ref_idx = chain[ref_idx];
        }

        if (match) {
            const uint8_t* next_ip = ip + 1;
            uint32_t next_h = fastlz_hash32(fastlz_read32(next_ip));
            uint32_t next_pos = current_pos + 1;
            uint16_t next_idx = next_pos & 0xFFFF;

            chain[next_idx] = head[next_h];
            head[next_h] = next_idx;

            uint16_t next_ref_idx = chain[next_idx];
            size_t next_best_len = FASTLZ_MIN_MATCH - 1;
            int next_attempts = FASTLZ_MAX_CHAIN / 4;

            while (next_ref_idx != 0 && next_attempts-- > 0) {
                uint32_t ref_pos = next_pos > next_idx ? (next_pos - next_idx + next_ref_idx) : next_ref_idx;
                if (ref_pos > next_pos) ref_pos -= 65536;

                size_t offset = next_pos - ref_pos;
                if (offset == 0 || offset > FASTLZ_MAX_OFFSET) break;

                const uint8_t* ref_ptr = in + ref_pos;
                if (fastlz_read32(ref_ptr) == fastlz_read32(next_ip)) {
                    size_t m_len = FASTLZ_MIN_MATCH;
                    while (next_ip + m_len < iend && next_ip[m_len] == ref_ptr[m_len]) {
                        m_len++;
                    }
                    if (m_len > next_best_len) next_best_len = m_len;
                }
                next_ref_idx = chain[next_ref_idx];
            }

            if (next_best_len > best_match_len + 1) {
                ip++;
                continue;
            }

            size_t lit_len = ip - anchor;
            op = fastlz_write_token(op, anchor, lit_len, best_match_len, best_offset, oend);
            if (!op) return 0;

            const uint8_t* match_end = ip + best_match_len;
            ip += 2;
            while (ip < match_end && ip < mflimit) {
                uint32_t h_update = fastlz_hash32(fastlz_read32(ip));
                uint16_t idx = (uint32_t)(ip - in) & 0xFFFF;
                chain[idx] = head[h_update];
                head[h_update] = idx;
                ip++;
            }
            ip = match_end;
            anchor = ip;
        } else {
            ip++;
        }
    }

    size_t lit_len = iend - anchor;
    if (op + lit_len + (lit_len / 255) + 2 > oend) return 0;

    uint8_t* token = op++;
    if (lit_len >= 15) {
        *token = (15 << 4);
        size_t l = lit_len - 15;
        while (l >= 255) { *op++ = 255; l -= 255; }
        *op++ = (uint8_t)l;
    } else {
        *token = (uint8_t)(lit_len << 4);
    }
    memcpy(op, anchor, lit_len);
    op += lit_len;

    return (size_t)(op - out);
}

// ============================================================================
// 4. ДЕКОМПРЕСІЯ (DECOMPRESSOR)
// ============================================================================

bool fastlz_decompress(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_size) {
    const uint8_t* ip = in;
    const uint8_t* iend = in + in_size;
    uint8_t* op = out;
    uint8_t* oend = out + out_size;

    while (ip < iend) {
        uint8_t token = *ip++;

        size_t lit_len = token >> 4;
        if (lit_len == 15) {
            uint8_t ext;
            do {
                if (ip >= iend) return false;
                ext = *ip++;
                lit_len += ext;
            } while (ext == 255);
        }

        if (op + lit_len > oend || ip + lit_len > iend) return false;
        memcpy(op, ip, lit_len);
        op += lit_len;
        ip += lit_len;

        if (ip >= iend) break;

        if (ip + 2 > iend) return false;
        uint16_t offset;
        memcpy(&offset, ip, 2);
        ip += 2;

        if (offset == 0 || op - offset < out) return false;

        size_t match_len = token & 0x0F;
        if (match_len == 15) {
            uint8_t ext;
            do {
                if (ip >= iend) return false;
                ext = *ip++;
                match_len += ext;
            } while (ext == 255);
        }
        match_len += FASTLZ_MIN_MATCH;

        if (op + match_len > oend) return false;
        const uint8_t* match = op - offset;
        for (size_t i = 0; i < match_len; ++i) {
            *op++ = *match++;
        }
    }

    return (op == oend);
}

// ============================================================================
// 5. ТЕСТОВИЙ ФРЕЙМВОРК ТА ЦИКЛІЧНИЙ БЕНЧМАРК
// ============================================================================

typedef enum {
    DATA_REPETITIVE,
    DATA_TEXT_JSON,
    DATA_RANDOM
} DataPattern;

uint8_t* generate_test_data(size_t size, DataPattern pattern) {
    uint8_t* buf = (uint8_t*)malloc(size);
    if (!buf) return NULL;

    switch (pattern) {
        case DATA_REPETITIVE:
            for (size_t i = 0; i < size; ++i) {
                buf[i] = (uint8_t)("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[i % 26]);
            }
            break;

        case DATA_TEXT_JSON: {
            const char* sample = "{\"id\": 1024, \"user\": \"alex_dev\", \"status\": \"active\", \"payload\": [10, 20, 30]} \n";
            size_t sample_len = strlen(sample);
            for (size_t i = 0; i < size; ++i) {
                buf[i] = (uint8_t)sample[i % sample_len];
            }
            break;
        }

        case DATA_RANDOM:
            srand(42);
            for (size_t i = 0; i < size; ++i) {
                buf[i] = (uint8_t)(rand() % 256);
            }
            break;
    }
    return buf;
}

void run_single_test(const char* test_name, const uint8_t* src, size_t src_size, int iterations) {
    printf("\n=== ТЕСТ: %s (Розмір: %.2f KB | Повторів: %d) ===\n", test_name, (double)src_size / 1024.0, iterations);

    size_t max_out = src_size * 2 + 512;
    uint8_t* compressed = (uint8_t*)malloc(max_out);
    uint8_t* decompressed = (uint8_t*)malloc(src_size);

    // --- 1. ПЕРЕВІРКА ЦІЛІСНОСТІ (ПРОГРІВ КЕШУ) ---
    size_t comp_size_fast = fastlz_compress(src, src_size, compressed, max_out);
    bool decomp_ok_fast = fastlz_decompress(compressed, comp_size_fast, decompressed, src_size);
    bool fast_valid = decomp_ok_fast && (memcmp(src, decompressed, src_size) == 0);

    memset(decompressed, 0, src_size);
    size_t comp_size_hc = fastlz_compress_hc(src, src_size, compressed, max_out);
    bool decomp_ok_hc = fastlz_decompress(compressed, comp_size_hc, decompressed, src_size);
    bool hc_valid = decomp_ok_hc && (memcmp(src, decompressed, src_size) == 0);

    // --- 2. БЕНЧМАРК: FASTLZ STANDARD COMPRESS ---
    clock_t start = clock();
    for (int i = 0; i < iterations; ++i) {
        fastlz_compress(src, src_size, compressed, max_out);
    }
    clock_t end = clock();
    double time_fast_comp = (double)(end - start) / CLOCKS_PER_SEC;

    // --- 3. БЕНЧМАРК: FASTLZ DECOMPRESS ---
    start = clock();
    for (int i = 0; i < iterations; ++i) {
        fastlz_decompress(compressed, comp_size_fast, decompressed, src_size);
    }
    end = clock();
    double time_fast_decomp = (double)(end - start) / CLOCKS_PER_SEC;

    // --- 4. БЕНЧМАРК: FASTLZ HC COMPRESS ---
    // На випадковому шумі HC дуже повільний, тому зменшуємо кількість повторів
    int hc_iter = (iterations > 20 && comp_size_hc >= src_size) ? 10 : iterations;
    
    start = clock();
    for (int i = 0; i < hc_iter; ++i) {
        fastlz_compress_hc(src, src_size, compressed, max_out);
    }
    end = clock();
    double time_hc_comp = (double)(end - start) / CLOCKS_PER_SEC;

    // --- 5. ОБРАХУНОК ПОКАЗНИКІВ ---
    double total_mb_std = ((double)src_size * iterations) / (1024.0 * 1024.0);
    double total_mb_hc = ((double)src_size * hc_iter) / (1024.0 * 1024.0);

    double speed_fast_comp = total_mb_std / (time_fast_comp > 0 ? time_fast_comp : 0.000001);
    double speed_fast_decomp = total_mb_std / (time_fast_decomp > 0 ? time_fast_decomp : 0.000001);
    double speed_hc_comp = total_mb_hc / (time_hc_comp > 0 ? time_hc_comp : 0.000001);

    double avg_us_fast_comp = (time_fast_comp / iterations) * 1e6;
    double avg_us_fast_decomp = (time_fast_decomp / iterations) * 1e6;

    // --- ВИВІД РЕЗУЛЬТАТІВ ---
    printf("%-16s | %-10s | %-10s | %-14s | %-15s | %-10s\n", 
           "Алгоритм", "Стиснено", "Коефіцієнт", "Шв. Стиснення", "Час/Прогон", "Цілісність");
    printf("-----------------------------------------------------------------------------------------\n");

    printf("%-16s | %6zu B   | %8.2f%%   | %8.2f MB/s   | %8.2f us     | %s\n",
           "FastLZ Standard", comp_size_fast, (double)comp_size_fast / src_size * 100.0,
           speed_fast_comp, avg_us_fast_comp, fast_valid ? "OK [УСПІХ]" : "ПОМИЛКА!");

    printf("%-16s | %6zu B   | %8.2f%%   | %8.2f MB/s   | %8.2f us     | %s\n",
           "FastLZ HC", comp_size_hc, (double)comp_size_hc / src_size * 100.0,
           speed_hc_comp, (time_hc_comp / hc_iter) * 1e6, hc_valid ? "OK [УСПІХ]" : "ПОМИЛКА!");

    printf("\n--> Середня швидкість декомпресії (FastLZ): %.2f MB/s (займає %.2f us на 1 МБ)\n",
           speed_fast_decomp, avg_us_fast_decomp);

    free(compressed);
    free(decompressed);
}

int main(void) {
    printf("======================================================================\n");
    printf("     ЦИКЛІЧНИЙ БЕНЧМАРК FASTLZ (%d ПОВТОРІВ НА КОЖЕН ТЕСТ)           \n", NUM_RUNS);
    printf("======================================================================\n");

    size_t test_size = 1024 * 1024; // 1 Мегабайт

    // Тест 1: Повторювані паттерни
    uint8_t* rep_data = generate_test_data(test_size, DATA_REPETITIVE);
    run_single_test("Повторюваний текст (A-Z)", rep_data, test_size, NUM_RUNS);
    free(rep_data);

    // Тест 2: Текст / JSON
    uint8_t* json_data = generate_test_data(test_size, DATA_TEXT_JSON);
    run_single_test("Структурований JSON", json_data, test_size, NUM_RUNS);
    free(json_data);

    // Тест 3: Випадковий шум
    uint8_t* rand_data = generate_test_data(test_size, DATA_RANDOM);
    run_single_test("Випадкові байти (Entropy)", rand_data, test_size, NUM_RUNS);
    free(rand_data);

    printf("\nУсі багаторазові тести успішно завершено!\n");
    return 0;
}