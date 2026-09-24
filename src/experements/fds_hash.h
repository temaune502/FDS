#ifndef FDS_HASH_H
#define FDS_HASH_H


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//                          ХЕШ-ФУНКЦІЇ (HASH FUNCTIONS)
// ============================================================================

// 1. SipHash-2-4 (Захист від HashDoS атак)
uint64_t fds_siphash64(const void *data, size_t len, uint64_t k0, uint64_t k1);

// 2. Fast WyHash64 (Максимальна швидкість для якісного розподілу)
uint64_t fds_wyhash64(const void *data, size_t len, uint64_t seed);

// 3. Зручні макроси-обгортки для типом SV та FdsBytesView
static inline uint64_t fds_hash_sv(SV sv, uint64_t seed) {
    return fds_wyhash64(sv.data, sv.count, seed);
}

static inline uint64_t fds_hash_bv(FdsBytesView bv, uint64_t seed) {
    return fds_wyhash64(bv.data, bv.size, seed);
}

// 4. Криптографічний SHA-256
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
} FdsSHA256;

void fds_sha256_init(FdsSHA256 *ctx);
void fds_sha256_update(FdsSHA256 *ctx, const void *data, size_t len);
void fds_sha256_final(FdsSHA256 *ctx, uint8_t out[32]);
void fds_sha256(const void *data, size_t len, uint8_t out[32]);

// ============================================================================
//                       ROBIN HOOD HASH MAP
// ============================================================================

typedef struct {
    FdsBytesView key;   // Ключ у вигляді байтового представлення
    FdsBytesView value; // Значення у вигляді байтового представлення
    uint64_t     hash;  // Кешоване значення хешу
    uint16_t     psl;   // Probe Sequence Length (відстань від ідеального бакета)
    bool         occupied;
} FdsMapItem;

typedef struct {
    FdsMapItem    *items;
    size_t         capacity; // Завжди степінь двійки
    size_t         count;
    uint64_t       seed0;
    uint64_t       seed1;
    fds_allocator *allocator; // Посилання на алокатор FDS (NULL = системний/TLS)
} FdsMap;

// Створення та знищення
FdsMap fds_map_create_a(fds_allocator *allocator, size_t initial_capacity);
static inline FdsMap fds_map_create(size_t initial_capacity) {
    return fds_map_create_a(NULL, initial_capacity);
}
void fds_map_destroy(FdsMap *map);

// Операції над хеш-мапою
bool fds_map_set_bv(FdsMap *map, FdsBytesView key, FdsBytesView val);
bool fds_map_get_bv(const FdsMap *map, FdsBytesView key, FdsBytesView *out_val);
bool fds_map_remove_bv(FdsMap *map, FdsBytesView key);
bool fds_map_contains_bv(const FdsMap *map, FdsBytesView key);
void fds_map_clear(FdsMap *map);

// Зручні обгортки для C-рядків та SV (StringView)
static inline bool fds_map_set_sv(FdsMap *m, SV k, SV v) {
    return fds_map_set_bv(m, fds_bv(k.data, k.count), fds_bv(v.data, v.count));
}

static inline bool fds_map_get_sv(const FdsMap *m, SV k, FdsBytesView *out_v) {
    return fds_map_get_bv(m, fds_bv(k.data, k.count), out_v);
}

static inline bool fds_map_set_str(FdsMap *m, const char *k, const char *v) {
    return fds_map_set_bv(m, fds_bv_from_cstr(k), fds_bv_from_cstr(v));
}

// Макрос ітератора по елементах хеш-мапи
#define FDS_MAP_FOREACH(map_ptr, item_var) \
    for (size_t _i = 0; _i < (map_ptr)->capacity; ++_i) \
        if ((map_ptr)->items[_i].occupied && ((item_var = &(map_ptr)->items[_i]), true))

#ifdef __cplusplus
}
#endif

#endif // FDS_HASH_H


// ============================================================================
//                               РЕАЛІЗАЦІЯ (IMPL)
// ============================================================================
#if defined(FDS_IMPL) || defined(FDS_HASH_IMPL)

// --- SipHash-2-4 Helper Implementation ---
#define ROTL64(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define SIPROUND \
    do { \
        v0 += v1; v1 = ROTL64(v1, 13); v1 ^= v0; v0 = ROTL64(v0, 32); \
        v2 += v3; v3 = ROTL64(v3, 16); v3 ^= v2; \
        v0 += v3; v3 = ROTL64(v3, 21); v3 ^= v0; \
        v2 += v1; v1 = ROTL64(v1, 17); v1 ^= v2; v2 = ROTL64(v2, 32); \
    } while (0)

uint64_t fds_siphash64(const void *data, size_t len, uint64_t k0, uint64_t k1) {
    const uint8_t *in = (const uint8_t *)data;
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616d706c65ULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;
    uint64_t b = ((uint64_t)len) << 56;
    size_t left = len;

    while (left >= 8) {
        uint64_t m;
        memcpy(&m, in, sizeof(uint64_t));
        v3 ^= m;
        SIPROUND;
        SIPROUND;
        v0 ^= m;
        in += 8;
        left -= 8;
    }

    uint64_t tail = 0;
    switch (left) {
        case 7: tail |= ((uint64_t)in[6]) << 48; // fallthrough
        case 6: tail |= ((uint64_t)in[5]) << 40; // fallthrough
        case 5: tail |= ((uint64_t)in[4]) << 32; // fallthrough
        case 4: tail |= ((uint64_t)in[3]) << 24; // fallthrough
        case 3: tail |= ((uint64_t)in[2]) << 16; // fallthrough
        case 2: tail |= ((uint64_t)in[1]) << 8;  // fallthrough
        case 1: tail |= ((uint64_t)in[0]); break;
        case 0: break;
    }
    b |= tail;

    v3 ^= b;
    SIPROUND;
    SIPROUND;
    v0 ^= b;

    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;

    return v0 ^ v1 ^ v2 ^ v3;
}

// --- WyHash Fast Implementation ---
static inline uint64_t _fds_wyhash_mum(uint64_t A, uint64_t B) {
#if defined(__SIZEOF_INT128__)
    unsigned __int128 r = A;
    r *= B;
    return (uint64_t)r ^ (uint64_t)(r >> 64);
#else
    uint64_t ha = A >> 32, la = (uint32_t)A, hb = B >> 32, lb = (uint32_t)B;
    uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
    uint64_t lo = rl + (rm0 << 32), hi = rh + (rm0 >> 32) + (lo < rl);
    uint64_t lo2 = lo + (rm1 << 32);
    hi += (rm1 >> 32) + (lo2 < lo);
    return hi ^ lo2;
#endif
}

uint64_t fds_wyhash64(const void *data, size_t len, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t secret = 0xa0761d6478bd642fULL;
    seed ^= _fds_wyhash_mum(seed ^ secret, 0xe7037ed1a0b428dbULL);
    
    if (len <= 16) {
        if (len >= 4) {
            uint32_t a, b;
            memcpy(&a, p, 4);
            memcpy(&b, p + len - 4, 4);
            uint64_t a64 = (((uint64_t)a) << 32) | b;
            return _fds_wyhash_mum(a64 ^ seed, len ^ secret);
        } else if (len > 0) {
            uint8_t a = p[0], b = p[len >> 1], c = p[len - 1];
            uint64_t a64 = (((uint64_t)a) << 16) | (((uint64_t)b) << 8) | c;
            return _fds_wyhash_mum(a64 ^ seed, len ^ secret);
        }
        return _fds_wyhash_mum(seed, secret);
    }

    uint64_t see1 = seed;
    size_t i = len;
    while (i > 16) {
        uint64_t v1, v2;
        memcpy(&v1, p, 8);
        memcpy(&v2, p + 8, 8);
        seed = _fds_wyhash_mum(v1 ^ seed, v2 ^ secret);
        p += 16;
        i -= 16;
    }
    uint64_t v1, v2;
    memcpy(&v1, p + i - 16, 8);
    memcpy(&v2, p + i - 8, 8);
    return _fds_wyhash_mum(v1 ^ seed, v2 ^ see1) ^ _fds_wyhash_mum(len ^ secret, seed);
}

// --- SHA-256 Implementation ---
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef4a3f7,0xc67178f2
};

#define SHR(x,n) ((x) >> (n))
#define ROTR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
#define S0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define S1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))
#define s0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define s1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

static void _fds_sha256_transform(FdsSHA256 *ctx, const uint8_t data[64]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, w[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        w[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    for (; i < 64; ++i)
        w[i] = S1(w[i - 2]) + w[i - 7] + S0(w[i - 15]) + w[i - 16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        uint32_t t1 = h + s1(e) + CH(e, f, g) + K256[i] + w[i];
        uint32_t t2 = s0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void fds_sha256_init(FdsSHA256 *ctx) {
    ctx->count = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

void fds_sha256_update(FdsSHA256 *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    size_t buffer_bytes = (size_t)(ctx->count & 0x3f);
    ctx->count += len;

    while (len > 0) {
        size_t copy_len = 64 - buffer_bytes;
        if (copy_len > len) copy_len = len;

        memcpy(ctx->buffer + buffer_bytes, p, copy_len);
        buffer_bytes += copy_len;
        p += copy_len;
        len -= copy_len;

        if (buffer_bytes == 64) {
            _fds_sha256_transform(ctx, ctx->buffer);
            buffer_bytes = 0;
        }
    }
}

void fds_sha256_final(FdsSHA256 *ctx, uint8_t out[32]) {
    size_t i = (size_t)(ctx->count & 0x3f);
    ctx->buffer[i++] = 0x80;

    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0x00;
        _fds_sha256_transform(ctx, ctx->buffer);
        i = 0;
    }
    while (i < 56) ctx->buffer[i++] = 0x00;

    uint64_t bits = ctx->count * 8;
    for (int j = 7; j >= 0; --j) {
        ctx->buffer[56 + j] = (uint8_t)(bits & 0xff);
        bits >>= 8;
    }
    _fds_sha256_transform(ctx, ctx->buffer);

    for (i = 0; i < 4; ++i) {
        out[i]      = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0xff);
        out[i + 4]  = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0xff);
        out[i + 8]  = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0xff);
        out[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0xff);
        out[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0xff);
        out[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0xff);
        out[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0xff);
        out[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0xff);
    }
}

void fds_sha256(const void *data, size_t len, uint8_t out[32]) {
    FdsSHA256 ctx;
    fds_sha256_init(&ctx);
    fds_sha256_update(&ctx, data, len);
    fds_sha256_final(&ctx, out);
}

// --- Robin Hood Hash Map Implementation ---

static void *_fds_map_alloc(fds_allocator *a, size_t sz) {
    return a ? fds_alloc_a(a, sz) : fds_alloc(sz);
}

static void _fds_map_free(fds_allocator *a, void *ptr) {
    if (!ptr) return;
    if (a) fds_free_a(a, ptr); else fds_free(ptr);
}

static FdsBytesView _fds_bv_dup(fds_allocator *a, FdsBytesView src) {
    if (src.size == 0 || !src.data) return fds_bv_empty();
    uint8_t *mem = (uint8_t *)_fds_map_alloc(a, src.size);
    memcpy(mem, src.data, src.size);
    return fds_bv(mem, src.size);
}

static void _fds_bv_free(fds_allocator *a, FdsBytesView *bv) {
    if (bv->data) {
        _fds_map_free(a, (void *)bv->data);
        bv->data = NULL;
        bv->size = 0;
    }
}

FdsMap fds_map_create_a(fds_allocator *allocator, size_t initial_capacity) {
    size_t cap = 16;
    while (cap < initial_capacity) cap <<= 1; // Степінь 2

    FdsMap m = {0};
    m.allocator = allocator;
    m.capacity = cap;
    m.count = 0;
    m.seed0 = 0xdeadbeefcafebabeULL;
    m.seed1 = 0x811c9dc500000001ULL;
    m.items = (FdsMapItem *)_fds_map_alloc(allocator, cap * sizeof(FdsMapItem));
    memset(m.items, 0, cap * sizeof(FdsMapItem));
    return m;
}

void fds_map_clear(FdsMap *map) {
    if (!map || !map->items) return;
    for (size_t i = 0; i < map->capacity; ++i) {
        if (map->items[i].occupied) {
            _fds_bv_free(map->allocator, &map->items[i].key);
            _fds_bv_free(map->allocator, &map->items[i].value);
            map->items[i].occupied = false;
        }
    }
    map->count = 0;
}

void fds_map_destroy(FdsMap *map) {
    if (!map) return;
    fds_map_clear(map);
    _fds_map_free(map->allocator, map->items);
    map->items = NULL;
    map->capacity = 0;
}

static bool _fds_map_insert_internal(FdsMap *map, FdsMapItem item) {
    size_t mask = map->capacity - 1;
    size_t idx = item.hash & mask;

    while (true) {
        if (!map->items[idx].occupied) {
            map->items[idx] = item;
            map->count++;
            return true;
        }

        // Оновлення наявного ключа
        if (fds_bv_equals(map->items[idx].key, item.key)) {
            _fds_bv_free(map->allocator, &map->items[idx].value);
            _fds_bv_free(map->allocator, &item.key); // Ключ дублікат більше не потрібен
            map->items[idx].value = item.value;
            return false;
        }

        // Принцип Robin Hood: Якщо новий елемент пробився далі, ніж поточний — міняємо їх місцями
        if (item.psl > map->items[idx].psl) {
            FdsMapItem tmp = map->items[idx];
            map->items[idx] = item;
            item = tmp;
        }

        idx = (idx + 1) & mask;
        item.psl++;
    }
}

static void _fds_map_rehash(FdsMap *map) {
    size_t old_cap = map->capacity;
    FdsMapItem *old_items = map->items;

    map->capacity <<= 1; // X2 рост
    map->count = 0;
    map->items = (FdsMapItem *)_fds_map_alloc(map->allocator, map->capacity * sizeof(FdsMapItem));
    memset(map->items, 0, map->capacity * sizeof(FdsMapItem));

    for (size_t i = 0; i < old_cap; ++i) {
        if (old_items[i].occupied) {
            old_items[i].psl = 0;
            _fds_map_insert_internal(map, old_items[i]);
        }
    }
    _fds_map_free(map->allocator, old_items);
}

bool fds_map_set_bv(FdsMap *map, FdsBytesView key, FdsBytesView val) {
    FDS_ASSERT(map != NULL, "Map is NULL");
    
    // Перевірка коефіцієнта заповнення (Load Factor > 75%)
    if ((map->count + 1) * 4 > map->capacity * 3) {
        _fds_map_rehash(map);
    }

    FdsMapItem item;
    item.key = _fds_bv_dup(map->allocator, key);
    item.value = _fds_bv_dup(map->allocator, val);
    item.hash = fds_siphash64(key.data, key.size, map->seed0, map->seed1);
    item.psl = 0;
    item.occupied = true;

    return _fds_map_insert_internal(map, item);
}

bool fds_map_get_bv(const FdsMap *map, FdsBytesView key, FdsBytesView *out_val) {
    if (!map || map->count == 0) return false;

    uint64_t hash = fds_siphash64(key.data, key.size, map->seed0, map->seed1);
    size_t mask = map->capacity - 1;
    size_t idx = hash & mask;
    uint16_t psl = 0;

    while (map->items[idx].occupied) {
        if (psl > map->items[idx].psl) {
            return false; // За Robin Hood далі шукати немає сенсу
        }
        if (map->items[idx].hash == hash && fds_bv_equals(map->items[idx].key, key)) {
            if (out_val) *out_val = map->items[idx].value;
            return true;
        }
        idx = (idx + 1) & mask;
        psl++;
    }
    return false;
}

bool fds_map_contains_bv(const FdsMap *map, FdsBytesView key) {
    return fds_map_get_bv(map, key, NULL);
}

bool fds_map_remove_bv(FdsMap *map, FdsBytesView key) {
    if (!map || map->count == 0) return false;

    uint64_t hash = fds_siphash64(key.data, key.size, map->seed0, map->seed1);
    size_t mask = map->capacity - 1;
    size_t idx = hash & mask;
    uint16_t psl = 0;

    while (map->items[idx].occupied) {
        if (psl > map->items[idx].psl) return false;

        if (map->items[idx].hash == hash && fds_bv_equals(map->items[idx].key, key)) {
            // Очищаємо пам'ять
            _fds_bv_free(map->allocator, &map->items[idx].key);
            _fds_bv_free(map->allocator, &map->items[idx].value);

            // Backward-shift deletion (зсув елементів назад для уникнення надлишкових томбстоунів)
            size_t next_idx = (idx + 1) & mask;
            while (map->items[next_idx].occupied && map->items[next_idx].psl > 0) {
                map->items[idx] = map->items[next_idx];
                map->items[idx].psl--;
                idx = next_idx;
                next_idx = (next_idx + 1) & mask;
            }

            map->items[idx].occupied = false;
            map->items[idx].psl = 0;
            map->count--;
            return true;
        }

        idx = (idx + 1) & mask;
        psl++;
    }
    return false;
}

#endif // FDS_IMPL
