#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define KB ((size_t)1024)
#define MB (KB * 1024)
#define GB (MB * 1024)
#define TB (GB * 1024)
#define TODO(...) \
    do { \
        fprintf(stderr, "[TODO] %s:%d (%s): ", __FILE__, __LINE__, __func__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } while(0)

/* ── FixedArena: фіксований буфер, без зростання ───────────── */
typedef struct {
    unsigned char *data;
    size_t offset;
    size_t capacity;
} FixedArena;

typedef size_t ArenaMark;

// ── створення / знищення ────────────────────────────────────────
FixedArena fixed_arena_create(size_t capacity) {
    FixedArena arena = {0};
    arena.data = (unsigned char *)malloc(capacity);
    assert(arena.data);  // malloc повернув NULL – помилка програміста (не вистачає пам'яті)
    arena.capacity = capacity;
    return arena;
}

void fixed_arena_free(FixedArena *arena) {
    assert(arena);
    free(arena->data);
    arena->data = NULL;
    arena->offset = 0;
    arena->capacity = 0;
}

void fixed_arena_reset(FixedArena *arena) {
    assert(arena);
    arena->offset = 0;
}

// ── інформація про стан ─────────────────────────────────────────
size_t fixed_arena_used(const FixedArena *arena) {
    assert(arena);
    return arena->offset;
}

size_t fixed_arena_available(const FixedArena *arena) {
    assert(arena);
    return arena->capacity - arena->offset;
}

int fixed_arena_is_empty(const FixedArena *arena) {
    assert(arena);
    return arena->offset == 0;
}

int fixed_arena_contains(const FixedArena *arena, const void *ptr) {
    assert(arena);
    const unsigned char *p = (const unsigned char *)ptr;
    return (p >= arena->data) && (p < arena->data + arena->offset);
}

ArenaMark fixed_arena_mark(const FixedArena *arena) {
    assert(arena);
    return arena->offset;
}

void fixed_arena_restore(FixedArena *arena, ArenaMark mark) {
    assert(arena);
    assert(mark <= arena->offset);
    arena->offset = mark;
}

// ── основні алокатори ───────────────────────────────────────────
void *fixed_arena_alloc_align(FixedArena *arena, size_t size, size_t alignment) {
    assert(arena);
    assert(size > 0);
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);  // степінь двійки

    uintptr_t ptr = (uintptr_t)(arena->data + arena->offset);
    uintptr_t aligned = (ptr + alignment - 1) & ~(uintptr_t)(alignment - 1);
    size_t padding = aligned - ptr;

    // Захист від переповнення
    if (padding > arena->capacity - arena->offset ||
        size > arena->capacity - arena->offset - padding) {
        fprintf(stderr, "FixedArena out of memory (capacity %zu, needed %zu)\n",
                arena->capacity, arena->offset + padding + size);
        abort();
    }

    arena->offset += padding;
    void *result = arena->data + arena->offset;
    arena->offset += size;
    return result;
}
#define fixed_arena_new_aligned(arena, Type, alignment) \
    ((Type *)fixed_arena_alloc_align((arena), sizeof(Type), (alignment)))

void *fixed_arena_alloc(FixedArena *arena, size_t size) {
    return fixed_arena_alloc_align(arena, size, sizeof(void *));
}

void *fixed_arena_alloc_zero(FixedArena *arena, size_t size) {
    void *ptr = fixed_arena_alloc(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void *fixed_arena_alloc_array(FixedArena *arena, size_t count, size_t element_size) {
    // Перевірка на переповнення множення
    size_t total;
    if (count > 0 && element_size > SIZE_MAX / count) {
        fprintf(stderr, "Array size overflow\n");
        abort();
    }
    total = count * element_size;
    return fixed_arena_alloc(arena, total);
}

// ── копіювання даних ────────────────────────────────────────────
void *fixed_arena_memdup(FixedArena *arena, const void *src, size_t size) {
    void *dst = fixed_arena_alloc(arena, size);
    memcpy(dst, src, size);
    return dst;
}

char *fixed_arena_strndup(FixedArena *arena, const char *str, size_t len) {
    char *dst = (char *)fixed_arena_alloc(arena, len + 1);
    memcpy(dst, str, len);
    dst[len] = '\0';
    return dst;
}

char *fixed_arena_strdup(FixedArena *arena, const char *str) {
    return fixed_arena_strndup(arena, str, strlen(str));
}

// ── макроси ─────────────────────────────────────────────────────
#define fixed_arena_new(arena, Type) \
    ((Type *)fixed_arena_alloc((arena), sizeof(Type)))

#define fixed_arena_new_zero(arena, Type) \
    ((Type *)fixed_arena_alloc_zero((arena), sizeof(Type)))

#define fixed_arena_array(arena, Type, count) \
    ((Type *)fixed_arena_alloc_array((arena), (count), sizeof(Type)))

#define fixed_arena_array_zero(arena, Type, count) \
    ((Type *)memset(fixed_arena_alloc_array((arena), (count), sizeof(Type)), 0, sizeof(Type) * (count)))

// ── приклад використання ───────────────────────────────────────
typedef struct {
    int x;
    float y;
} Test;
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
#define TEST(name) printf("  TEST: %s ... ", name)
#define OK() printf("OK\n")

// перевірка, що вказівник вирівняний
int is_aligned(const void *ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

// ────────────────────────────────────────────────────────────────────
void test_basic_alloc(void) {
    TEST("basic alloc & alignment");
    FixedArena a = fixed_arena_create(1024);

    int *p1 = fixed_arena_new(&a, int);
    *p1 = 0xDEAD;
    assert(*p1 == 0xDEAD);

    double *p2 = fixed_arena_new(&a, double);
    *p2 = 3.1415;
    assert(*p2 == 3.1415);
    assert(((uintptr_t)p2 & (alignof(double) - 1)) == 0); // ignore error

    // перевірка, що пам'ять не затирається
    assert(*p1 == 0xDEAD);

    fixed_arena_free(&a);
    OK();
}

void test_alloc_zero(void) {
    TEST("alloc_zero");
    FixedArena a = fixed_arena_create(256);
    int *p = fixed_arena_new_zero(&a, int);
    assert(*p == 0);
    *p = 5;
    assert(*p == 5);
    fixed_arena_free(&a);
    OK();
}

void test_arrays(void) {
    TEST("arrays (int, double, char)");
    FixedArena a = fixed_arena_create(1024);

    int *arr = fixed_arena_array(&a, int, 10);
    for (int i = 0; i < 10; i++) arr[i] = i * i;
    for (int i = 0; i < 10; i++) assert(arr[i] == i * i);

    // zero array
    double *darr = fixed_arena_array_zero(&a, double, 5);
    for (int i = 0; i < 5; i++) assert(darr[i] == 0.0);

    // char array
    char *str = fixed_arena_array(&a, char, 12);
    memcpy(str, "hello world", 12);
    assert(strcmp(str, "hello world") == 0);

    fixed_arena_free(&a);
    OK();
}

void test_strings(void) {
    TEST("string duplication");
    FixedArena a = fixed_arena_create(128);

    char *s1 = fixed_arena_strdup(&a, "FixedArena");
    assert(strcmp(s1, "FixedArena") == 0);

    char *s2 = fixed_arena_strndup(&a, "Hello, World!", 5);
    assert(strcmp(s2, "Hello") == 0);

    // порожній рядок
    char *empty = fixed_arena_strndup(&a, "", 0);
    assert(strcmp(empty, "") == 0);

    fixed_arena_free(&a);
    OK();
}

void test_mark_restore(void) {
    TEST("mark / restore");
    FixedArena a = fixed_arena_create(512);

    int *x = fixed_arena_new(&a, int);
    *x = 42;

    ArenaMark m = fixed_arena_mark(&a);

    // виділення, які будуть скасовані
    double *tmp = fixed_arena_new(&a, double);
    *tmp = 99.9;
    (void)tmp;

    fixed_arena_restore(&a, m);
    // x має залишитися недоторканим
    assert(*x == 42);
    // пам'ять після мітки тепер вільна
    size_t used = fixed_arena_used(&a);
    // перевіримо, що можемо знову алокувати на тому ж місці
    double *y = fixed_arena_new(&a, double);
    *y = 77.7;
    assert(*x == 42);
    assert(*y == 77.7);
    // переконаємось, що використана пам'ять збільшилась знов
    assert(fixed_arena_used(&a) > used);

    fixed_arena_free(&a);
    OK();
}

void test_reset(void) {
    TEST("reset & reuse");
    FixedArena a = fixed_arena_create(64);

    int *p = fixed_arena_new(&a, int);
    *p = 123;
    assert(fixed_arena_used(&a) == sizeof(int));

    fixed_arena_reset(&a);
    assert(fixed_arena_used(&a) == 0);
    assert(fixed_arena_is_empty(&a));

    // нове виділення
    int *q = fixed_arena_new(&a, int);
    *q = 456;
    // p більше не валідний за логікою арени, але пам'ять може залишатися
    assert(*q == 456);

    fixed_arena_free(&a);
    OK();
}

void test_contains(void) {
    TEST("contains");
    FixedArena a = fixed_arena_create(256);

    int *x = fixed_arena_new(&a, int);
    assert(fixed_arena_contains(&a, x));
    // адреса за межами арени
    int stack_var;
    assert(!fixed_arena_contains(&a, &stack_var));

    fixed_arena_reset(&a);
    // після reset вказівник x не повинен вважатися таким, що належить арені
    assert(!fixed_arena_contains(&a, x));

    fixed_arena_free(&a);
    OK();
}

void test_exact_fill(void) {
    TEST("exact capacity fill");
    const size_t cap = 128;
    FixedArena a = fixed_arena_create(cap);

    /* Заповнюємо арену int'ами, поки гарантовано вистачає місця
     * з урахуванням вирівнювання (sizeof(void*) байт). */
    while (fixed_arena_available(&a) >= sizeof(int) + (sizeof(void*) - 1)) {
        int *p = fixed_arena_new(&a, int);
        *p = (int)fixed_arena_used(&a);   // просто для запису
    }

    /* Тепер вільного місця менше ніж sizeof(int)+padding,
     * але спробуємо виділити окремі байти, щоб заповнити залишок. */
    while (fixed_arena_available(&a) > 0) {
        /* Виділяємо char (1 байт) без паддінгу, використовуючи
         * fixed_arena_alloc, який все одно вирівняє, але для 1 байта
         * паддінг буде не більше sizeof(void*)-1, і ми гарантовано
         * не вийдемо за межі, бо залишок перевірено. */
        if (fixed_arena_available(&a) < 1 + (sizeof(void*) - 1))
            break;   // не вистачить навіть на 1 байт із вирівнюванням
        char *c = fixed_arena_alloc(&a, 1);
        *c = 'X';
    }

    /* Арена майже повна, і жодного abort не відбулося. */
    assert(fixed_arena_available(&a) < sizeof(int) + sizeof(void*));

    fixed_arena_free(&a);
    OK();
}
void test_large_alignment(void) {
    TEST("large alignment (64)");
    FixedArena a = fixed_arena_create(256);

    void *p = fixed_arena_alloc_align(&a, 8, 64);
    assert(is_aligned(p, 64));
    memset(p, 0xAB, 8);

    void *q = fixed_arena_alloc_align(&a, 1, 128);
    assert(is_aligned(q, 128));
    *(char*)q = 'Z';

    fixed_arena_free(&a);
    OK();
}

void test_memdup(void) {
    TEST("memdup");
    FixedArena a = fixed_arena_create(128);
    int src[] = {1, 2, 3, 4, 5};
    int *cpy = fixed_arena_memdup(&a, src, sizeof(src));
    for (int i = 0; i < 5; i++) assert(cpy[i] == src[i]);
    fixed_arena_free(&a);
    OK();
}

// ────────────────────────────────────────────────────────────────────
int main(void) {
    printf("Stress tests for FixedArena\n");

    test_basic_alloc();
    test_alloc_zero();
    test_arrays();
    test_strings();
    test_mark_restore();
    test_reset();
    test_contains();
    test_exact_fill();
    test_large_alignment();
    test_memdup();
    printf("\nAll tests passed.\n");
    return 0;
}