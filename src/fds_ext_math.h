/*
    fds_ext_math.h — STB-style швидка та наближена математика (Sloppy Math) для FDS.
    Містить: float equality, int/float min/max/clamp, fast math, Xorshift32 PRNG та 2D колізії.

    Використання:
    1. #include "fds_ext_math.h"
    2. В ОДНОМУ C-файлі додайте:
       #define FDS_EXT_MATH_IMPLEMENTATION
       #include "fds_ext_math.h"
*/

#ifndef FDS_EXT_MATH_H
#define FDS_EXT_MATH_H

#include <stdint.h>
#include <stdbool.h>

#ifndef FDS_ASSERT
    #include <assert.h>
    #define FDS_ASSERT(cond, msg) assert((cond) && (msg))
#endif

#ifndef FDS_MATH_DEF
    #define FDS_MATH_DEF extern
#endif

#define FDS_PI          3.14159265358979323846f
#define FDS_TAU         6.28318530717958647692f
#define FDS_DEG2RAD     (FDS_PI / 180.0f)
#define FDS_RAD2DEG     (180.0f / FDS_PI)
#define FDS_EPSILON     1e-6f

// --- Базові структури ---

typedef struct {
    uint32_t state;
} FdsRng;

typedef struct { float x, y; } FdsVec2;
typedef struct { float x, y, z; } FdsVec3;

// AABB (Axis-Aligned Bounding Box)
typedef struct {
    FdsVec2 min; // Нижня-ліва (або верхня-ліва, залежить від вашої системи координат)
    FdsVec2 max; // Верхня-права (або нижня-права)
} FdsAABB;

// Коло
typedef struct {
    FdsVec2 center;
    float radius;
} FdsCircle;


#ifdef __cplusplus
extern "C" {
#endif

// --- Базові математичні операції та порівняння (Float) ---
FDS_MATH_DEF float    fds_absf(float v);
FDS_MATH_DEF float    fds_signf(float v);
FDS_MATH_DEF float    fds_min_f32(float a, float b);
FDS_MATH_DEF float    fds_max_f32(float a, float b);
FDS_MATH_DEF float    fds_clamp_f32(float v, float min_val, float max_val);

// --- Порівняння Float (Nearly Equal) ---
FDS_MATH_DEF bool     fds_nearly_equal_eps(float a, float b, float epsilon);
FDS_MATH_DEF bool     fds_nearly_equal(float a, float b);
FDS_MATH_DEF bool     fds_is_zero(float a);

// --- Інтерполяція та Remap ---
FDS_MATH_DEF float    fds_lerp(float a, float b, float t);
FDS_MATH_DEF float    fds_inv_lerp(float a, float b, float v);
FDS_MATH_DEF float    fds_remap(float v, float in_min, float in_max, float out_min, float out_max);
FDS_MATH_DEF float    fds_smoothstepf(float edge0, float edge1, float x);

// --- Цілочисельні Min / Max / Clamp ---
FDS_MATH_DEF int32_t  fds_min_i32(int32_t a, int32_t b);
FDS_MATH_DEF int32_t  fds_max_i32(int32_t a, int32_t b);
FDS_MATH_DEF int32_t  fds_clamp_i32(int32_t v, int32_t min_val, int32_t max_val);

// --- Швидкі наближені обчислення (Fast / Sloppy Math) ---
FDS_MATH_DEF float    fds_fast_inv_sqrtf(float number);
FDS_MATH_DEF float    fds_fast_sqrtf(float number);
FDS_MATH_DEF float    fds_fast_sinf(float x); // x в радіанах [-PI, PI]
FDS_MATH_DEF float    fds_fast_cosf(float x);

// --- Генератор псевдовипадкових чисел (Xorshift32 PRNG) ---
FDS_MATH_DEF FdsRng   fds_rng_init(uint32_t seed);
FDS_MATH_DEF uint32_t fds_rng_u32(FdsRng *rng);
FDS_MATH_DEF float    fds_rng_f32(FdsRng *rng); // [0.0f, 1.0f]
FDS_MATH_DEF int32_t  fds_rng_rangei(FdsRng *rng, int32_t min_val, int32_t max_val);
FDS_MATH_DEF float    fds_rng_rangef(FdsRng *rng, float min_val, float max_val);
FDS_MATH_DEF bool     fds_rng_bool(FdsRng *rng);
FDS_MATH_DEF bool     fds_rng_chance(FdsRng *rng, float probability); // probability [0.0f, 1.0f]

// --- Легкі 2D-вектори ---
FDS_MATH_DEF FdsVec2  fds_vec2(float x, float y);
FDS_MATH_DEF FdsVec2  fds_vec2_add(FdsVec2 a, FdsVec2 b);
FDS_MATH_DEF FdsVec2  fds_vec2_sub(FdsVec2 a, FdsVec2 b);
FDS_MATH_DEF FdsVec2  fds_vec2_scale(FdsVec2 v, float s);
FDS_MATH_DEF float    fds_vec2_dot(FdsVec2 a, FdsVec2 b);
FDS_MATH_DEF float    fds_vec2_length_sq(FdsVec2 v);
FDS_MATH_DEF FdsVec2  fds_vec2_normalize(FdsVec2 v);

// --- 2D Геометрія та Колізії (Overlap) ---
FDS_MATH_DEF FdsAABB  fds_aabb(FdsVec2 min, FdsVec2 max);
FDS_MATH_DEF FdsAABB  fds_aabb_from_pos_size(FdsVec2 pos, FdsVec2 size);
FDS_MATH_DEF FdsCircle fds_circle(FdsVec2 center, float radius);

FDS_MATH_DEF FdsVec2  fds_aabb_center(FdsAABB aabb);
FDS_MATH_DEF FdsVec2  fds_closest_point_aabb(FdsAABB aabb, FdsVec2 p);

FDS_MATH_DEF bool     fds_overlap_aabb(FdsAABB a, FdsAABB b);
FDS_MATH_DEF bool     fds_overlap_circle(FdsCircle a, FdsCircle b);
FDS_MATH_DEF bool     fds_overlap_aabb_circle(FdsAABB aabb, FdsCircle circle);

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_MATH_H

/* ============================================================================
   IMPLEMENTATION
   ============================================================================ */
#ifdef FDS_EXT_MATH_IMPL

// --- Float Math ---

FDS_MATH_DEF float fds_absf(float v) {
    return v < 0.0f ? -v : v;
}

FDS_MATH_DEF float fds_signf(float v) {
    if (v > 0.0f) return 1.0f;
    if (v < 0.0f) return -1.0f;
    return 0.0f;
}

FDS_MATH_DEF float fds_min_f32(float a, float b) { return a < b ? a : b; }
FDS_MATH_DEF float fds_max_f32(float a, float b) { return a > b ? a : b; }

FDS_MATH_DEF float fds_clamp_f32(float v, float min_val, float max_val) {
    FDS_ASSERT(min_val <= max_val, "min_val must be <= max_val");
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

// --- Float Equality ---

FDS_MATH_DEF bool fds_nearly_equal_eps(float a, float b, float epsilon) {
    FDS_ASSERT(epsilon >= 0.0f, "Epsilon must be non-negative");
    return fds_absf(a - b) <= epsilon;
}

FDS_MATH_DEF bool fds_nearly_equal(float a, float b) {
    return fds_nearly_equal_eps(a, b, FDS_EPSILON);
}

FDS_MATH_DEF bool fds_is_zero(float a) {
    return fds_absf(a) <= FDS_EPSILON;
}

// --- Interpolation ---

FDS_MATH_DEF float fds_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

FDS_MATH_DEF float fds_inv_lerp(float a, float b, float v) {
    float range = b - a;
    if (fds_is_zero(range)) return 0.0f;
    return (v - a) / range;
}

FDS_MATH_DEF float fds_remap(float v, float in_min, float in_max, float out_min, float out_max) {
    float t = fds_inv_lerp(in_min, in_max, v);
    return fds_lerp(out_min, out_max, t);
}

FDS_MATH_DEF float fds_smoothstepf(float edge0, float edge1, float x) {
    float range = edge1 - edge0;
    FDS_ASSERT(!fds_is_zero(range), "Edge range cannot be 0");
    float t = fds_clamp_f32((x - edge0) / range, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// --- Int Math ---

FDS_MATH_DEF int32_t fds_min_i32(int32_t a, int32_t b) {
    return a < b ? a : b;
}

FDS_MATH_DEF int32_t fds_max_i32(int32_t a, int32_t b) {
    return a > b ? a : b;
}

FDS_MATH_DEF int32_t fds_clamp_i32(int32_t v, int32_t min_val, int32_t max_val) {
    FDS_ASSERT(min_val <= max_val, "min_val must be <= max_val");
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

// --- Fast Math ---

FDS_MATH_DEF float fds_fast_inv_sqrtf(float number) {
    FDS_ASSERT(number > 0.0f, "Number must be > 0 for inverse sqrt");
    union { float f; uint32_t i; } conv;
    float x2 = number * 0.5f;
    conv.f = number;
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f = conv.f * (1.5f - (x2 * conv.f * conv.f));
    return conv.f;
}

FDS_MATH_DEF float fds_fast_sqrtf(float number) {
    if (number <= 0.0f) return 0.0f;
    return 1.0f / fds_fast_inv_sqrtf(number);
}

FDS_MATH_DEF float fds_fast_sinf(float x) {
    while (x < -FDS_PI) x += FDS_TAU;
    while (x >  FDS_PI) x -= FDS_TAU;

    float sin_val;
    if (x < 0.0f) {
        sin_val = 1.27323954f * x + 0.405284735f * x * x;
        if (sin_val < 0.0f) sin_val = 0.225f * (sin_val * -sin_val - sin_val) + sin_val;
        else                sin_val = 0.225f * (sin_val *  sin_val - sin_val) + sin_val;
    } else {
        sin_val = 1.27323954f * x - 0.405284735f * x * x;
        if (sin_val < 0.0f) sin_val = 0.225f * (sin_val * -sin_val - sin_val) + sin_val;
        else                sin_val = 0.225f * (sin_val *  sin_val - sin_val) + sin_val;
    }
    return sin_val;
}

FDS_MATH_DEF float fds_fast_cosf(float x) {
    return fds_fast_sinf(x + (FDS_PI * 0.5f));
}

// --- RNG ---

FDS_MATH_DEF FdsRng fds_rng_init(uint32_t seed) {
    FDS_ASSERT(seed != 0, "RNG seed cannot be 0");
    return (FdsRng){ .state = seed };
}

FDS_MATH_DEF uint32_t fds_rng_u32(FdsRng *rng) {
    FDS_ASSERT(rng != NULL, "RNG pointer is NULL");
    FDS_ASSERT(rng->state != 0, "RNG state cannot be 0");
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

FDS_MATH_DEF float fds_rng_f32(FdsRng *rng) {
    return (float)(fds_rng_u32(rng) & 0xFFFFFF) / 16777216.0f;
}

FDS_MATH_DEF int32_t fds_rng_rangei(FdsRng *rng, int32_t min_val, int32_t max_val) {
    FDS_ASSERT(min_val <= max_val, "min_val must be <= max_val");
    if (min_val == max_val) return min_val;
    uint32_t range = (uint32_t)(max_val - min_val + 1);
    return min_val + (int32_t)(fds_rng_u32(rng) % range);
}

FDS_MATH_DEF float fds_rng_rangef(FdsRng *rng, float min_val, float max_val) {
    FDS_ASSERT(min_val <= max_val, "min_val must be <= max_val");
    return min_val + fds_rng_f32(rng) * (max_val - min_val);
}

FDS_MATH_DEF bool fds_rng_bool(FdsRng *rng) {
    return (fds_rng_u32(rng) & 1) != 0;
}

FDS_MATH_DEF bool fds_rng_chance(FdsRng *rng, float probability) {
    return fds_rng_f32(rng) <= probability;
}

// --- 2D Vectors ---

FDS_MATH_DEF FdsVec2 fds_vec2(float x, float y) {
    return (FdsVec2){ .x = x, .y = y };
}

FDS_MATH_DEF FdsVec2 fds_vec2_add(FdsVec2 a, FdsVec2 b) {
    return (FdsVec2){ .x = a.x + b.x, .y = a.y + b.y };
}

FDS_MATH_DEF FdsVec2 fds_vec2_sub(FdsVec2 a, FdsVec2 b) {
    return (FdsVec2){ .x = a.x - b.x, .y = a.y - b.y };
}

FDS_MATH_DEF FdsVec2 fds_vec2_scale(FdsVec2 v, float s) {
    return (FdsVec2){ .x = v.x * s, .y = v.y * s };
}

FDS_MATH_DEF float fds_vec2_dot(FdsVec2 a, FdsVec2 b) {
    return a.x * b.x + a.y * b.y;
}

FDS_MATH_DEF float fds_vec2_length_sq(FdsVec2 v) {
    return v.x * v.x + v.y * v.y;
}

FDS_MATH_DEF FdsVec2 fds_vec2_normalize(FdsVec2 v) {
    float len_sq = fds_vec2_length_sq(v);
    if (len_sq < FDS_EPSILON) return (FdsVec2){ 0.0f, 0.0f };
    float inv_len = fds_fast_inv_sqrtf(len_sq);
    return fds_vec2_scale(v, inv_len);
}


// --- 2D Геометрія та Колізії (Overlap) ---

FDS_MATH_DEF FdsAABB fds_aabb(FdsVec2 min, FdsVec2 max) {
    return (FdsAABB){ .min = min, .max = max };
}

// Допоміжна функція для створення AABB з позиції та розміру (ширини/висоти)
FDS_MATH_DEF FdsAABB fds_aabb_from_pos_size(FdsVec2 pos, FdsVec2 size) {
    FdsVec2 max = fds_vec2_add(pos, size);
    return (FdsAABB){ .min = pos, .max = max };
}

FDS_MATH_DEF FdsCircle fds_circle(FdsVec2 center, float radius) {
    return (FdsCircle){ .center = center, .radius = radius };
}

FDS_MATH_DEF FdsVec2 fds_aabb_center(FdsAABB aabb) {
    return fds_vec2(
        (aabb.min.x + aabb.max.x) * 0.5f,
        (aabb.min.y + aabb.max.y) * 0.5f
    );
}

// Знаходить найближчу точку всередині (або на краю) AABB до заданої точки P
FDS_MATH_DEF FdsVec2 fds_closest_point_aabb(FdsAABB aabb, FdsVec2 p) {
    return fds_vec2(
        fds_clamp_f32(p.x, aabb.min.x, aabb.max.x),
        fds_clamp_f32(p.y, aabb.min.y, aabb.max.y)
    );
}

// Перевірка AABB vs AABB
FDS_MATH_DEF bool fds_overlap_aabb(FdsAABB a, FdsAABB b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x &&
            a.min.y <= b.max.y && a.max.y >= b.min.y);
}

// Перевірка Circle vs Circle (уникаємо sqrt)
FDS_MATH_DEF bool fds_overlap_circle(FdsCircle a, FdsCircle b) {
    float dist_sq = fds_vec2_length_sq(fds_vec2_sub(a.center, b.center));
    float radius_sum = a.radius + b.radius;
    return dist_sq <= (radius_sum * radius_sum);
}

// Перевірка AABB vs Circle (найближча точка)
FDS_MATH_DEF bool fds_overlap_aabb_circle(FdsAABB aabb, FdsCircle circle) {
    // 1. Знаходимо найближчу точку на AABB до центру кола
    FdsVec2 closest = fds_closest_point_aabb(aabb, circle.center);
    
    // 2. Перевіряємо відстань від цієї точки до центру кола
    FdsVec2 diff = fds_vec2_sub(closest, circle.center);
    float dist_sq = fds_vec2_length_sq(diff);
    
    return dist_sq <= (circle.radius * circle.radius);
}

#endif // FDS_EXT_MATH_IMPLEMENTATION