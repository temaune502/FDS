#ifndef FDS_H
#define FDS_H

#include <stddef.h>  /* for size_t */

#define SB_INITIAL_CAPACITY 64

#define SV_FMT "%.*s"
#define SV_ARGS(sv) (int)(sv).count, (sv).data   /* int cast is required by printf's "%.*s" */
#define array_len(arr) (sizeof(arr)/sizeof((arr)[0]))

typedef struct {
    size_t count;
    size_t capacity;
    char *items;
} SB;

typedef struct {
    size_t count;
    const char *data;
} SV;

/* -------------------- Щось додаткове ----------------------------------- */
int safe_add(size_t a, size_t b, size_t *res);
/* -------------------- String Builder (SB) functions -------------------- */
SB   sb_from_cstr(const char *str);
SB   sb_new(void);
void sb_append_sv(SB *sb, SV sv);
void sb_free(SB *sb);
void sb_append(SB *sb, const char *str);
void sb_append_n(SB *sb, const char *str, size_t len);
void sb_reserve(SB *sb, size_t capacity);
void sb_reserve_extra(SB *sb, size_t extra);
char *sb_to_cstr(SB *sb);
void sb_append_null(SB *sb);
void sb_append_char(SB *sb, char c);
SB   sb_clone(const SB *sb);
void sb_appendf(SB *sb, const char *fmt, ...);

/* -------------------- String View (SV) functions -------------------- */
void   sv_remove_prefix(SV *sv, size_t count);
char * sv_to_cstr(SV sv);
char   sv_at(SV sv, size_t index);
SV     sv_new(void);
SV     sv_from_cstr(const char *str);
SV     sv_from_sb(const SB *sb);
SV     sv_from_parts(const char *str, size_t len);
int    sv_eq(SV sv1, SV sv2);
int    sv_eq_cstr(SV sv1, const char *str);
void   sv_trim_left(SV *sv);
void   sv_trim_right(SV *sv);
void   sv_trim(SV *sv);
void   sv_slice(SV *sv, size_t begin, size_t end);
void   sv_remove_suffix(SV *sv, size_t count);
int    sv_ends_with(SV sv, SV suffix);
int    sv_starts_with(SV sv, SV prefix);
int    sv_starts_with_char(SV sv, char c);
int    sv_ends_with_char(SV sv, char c);
SV     sv_split_left(SV *sv, char c);
SV     sv_split_right(SV *sv, char c);
size_t sv_find_char(SV sv, char c);
size_t sv_rfind_char(SV sv, char c);
int    sv_consume_char(SV *sv, char c);
int    sv_consume(SV *sv, SV prefix);

#ifdef FDS_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

/* ====================================================================
 *  Внутрішні допоміжні засоби
 * ==================================================================== */

/* Порожній рядок-літерал, щоб уникнути NULL-вказівників у порожніх SV */
static const char EMPTY_STR[] = "";

/* Перевірка на безпечне додавання (переповнення) */
int safe_add(size_t a, size_t b, size_t *res) {
    if (a > SIZE_MAX - b) return 0;
    *res = a + b;
    return 1;
}





/*===============================================================
Реалізація динамічного масиву (Dynamic Array) для демонстрації.
=================================================================*/
#define da_realloc(da, new_cap)                                                \
    do {                                                                       \
        void *_p = realloc((da)->items, (new_cap) * sizeof(*(da)->items));    \
        if (!_p) {                                                             \
            fprintf(stderr, "Out of memory\n");                                \
            abort();                                                           \
        }                                                                      \
        (da)->items = _p;                                                      \
        (da)->capacity = (new_cap);                                            \
    } while (0)

// ============================================================
// Ітераційні макроси
// ============================================================

// Прямий прохід: безпечний навіть для порожнього масиву (цикл не виконається)
#define da_foreach(Type, it, da) \
    for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

// Зворотний прохід: щоб уникнути недійсного вказівника при count == 0,
// обгорнуто в if ((da)->count > 0). Тоді початковий ітератор обчислюється
// лише за наявності елементів.
#define da_foreach_reverse(Type, it, da) \
    if ((da)->count > 0) \
        for (Type *it = (da)->items + (da)->count - 1; it >= (da)->items; --it)

// ============================================================
// Базові операції над масивом
// ============================================================

// Звільнити всю пам'ять і скинути поля
#define da_free(da) \
    do { \
        free((da)->items); \
        (da)->items = NULL; \
        (da)->count = 0; \
        (da)->capacity = 0; \
    } while (0)

// Очистити масив, не зменшуючи ємності
#define da_clear(da) \
    do { \
        (da)->count = 0; \
    } while (0)

#define da_empty(da) ((da)->count == 0)

// Отримати останній елемент; має бути хоча б один елемент
#define da_back(da) \
    (assert((da)->count > 0), (da)->items[(da)->count - 1])

// Отримати перший елемент; має бути хоча б один елемент
#define da_front(da) \
    (assert((da)->count > 0), (da)->items[0])

// Зарезервувати ємність не менше cap; не зменшує ємність, якщо cap < capacity
#define da_reserve(da, cap) \
    do { \
        if ((cap) > (da)->capacity) { \
            da_realloc((da), (cap)); \
        } \
    } while (0)

// Видалити останній елемент і повернути його; має бути хоча б один елемент
#define da_pop(da) \
    (assert((da)->count > 0), (da)->items[--(da)->count])

// Вставити value на позицію index (0 <= index <= count)
#define da_insert(da, index, value) \
    do { \
        assert((index) <= (da)->count); \
        da_push((da), (value)); \
        memmove( \
            &(da)->items[(index) + 1], \
            &(da)->items[(index)], \
            ((da)->count - (index) - 1) * sizeof(*(da)->items)); \
        (da)->items[(index)] = (value); \
    } while (0)

// Видалити елемент на позиції index (0 <= index < count)
#define da_remove(da, index) \
    do { \
        assert((index) < (da)->count); \
        memmove( \
            &(da)->items[(index)], \
            &(da)->items[(index) + 1], \
            ((da)->count - (index) - 1) * sizeof(*(da)->items)); \
        --(da)->count; \
    } while (0)

// Швидке видалення: замінити елемент index останнім і зменшити count
#define da_swap_remove(da, index) \
    do { \
        assert((index) < (da)->count); \
        (da)->items[index] = (da)->items[(da)->count - 1]; \
        --(da)->count; \
    } while (0)

// Глибоке копіювання: dst отримує копію даних src
#define da_clone(dst, src) \
    do { \
        da_reserve((dst), (src)->count); \
        memcpy((dst)->items, \
               (src)->items, \
               (src)->count * sizeof(*(src)->items)); \
        (dst)->count = (src)->count; \
    } while (0)

// Додати cnt елементів з масиву ptr
#define da_append(da, ptr, cnt) \
    do { \
        da_reserve((da), (da)->count + (cnt)); \
        memcpy((da)->items + (da)->count, \
               (ptr), \
               (cnt) * sizeof(*(da)->items)); \
        (da)->count += (cnt); \
    } while (0)

// Додати один елемент у кінець (з автоматичним розширенням)
#define da_push(da, value) \
    do { \
        if ((da)->count >= (da)->capacity) { \
            size_t new_cap = (da)->capacity ? (da)->capacity * 2 : 4; \
            da_realloc((da), new_cap); \
        } \
        (da)->items[(da)->count++] = (value); \
    } while (0)

// Гарантувати, що можна додати extra елементів без перерозподілу
#define da_grow(da, extra) \
    do { \
        size_t need = (da)->count + (extra); \
        if (need > (da)->capacity) { \
            size_t cap = (da)->capacity ? (da)->capacity * 2 : 4; \
            while (cap < need) \
                cap *= 2; \
            da_realloc((da), cap); \
        } \
    } while (0)

// Змінити розмір масиву. Нові елементи (якщо розмір збільшено) заповнюються нулями.
// Це виправляє попередню помилку з неініціалізованими даними.
#define da_resize(da, size) \
    do { \
        size_t _old_cnt = (da)->count; \
        da_reserve((da), (size)); \
        (da)->count = (size); \
        if ((size) > _old_cnt) { \
            memset((da)->items + _old_cnt, 0, ((size) - _old_cnt) * sizeof(*(da)->items)); \
        } \
    } while (0)























/* ====================================================================
 *  Реалізація String View (SV)
 * ==================================================================== */

void sv_remove_prefix(SV *sv, size_t count) {
    if (count > sv->count) count = sv->count;
    sv->data += count;
    sv->count -= count;
}

char *sv_to_cstr(SV sv) {
    char *cstr = malloc(sv.count + 1);
    if (cstr == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    if (sv.count > 0) {
        memcpy(cstr, sv.data, sv.count);
    }
    cstr[sv.count] = '\0';
    return cstr;
}

char sv_at(SV sv, size_t index) {
    assert(index < sv.count);
    return sv.data[index];
}

SV sv_new(void) {
    SV sv;
    sv.count = 0;
    sv.data = EMPTY_STR;   /* безпечний порожній рядок */
    return sv;
}

SV sv_from_cstr(const char *str) {
    assert(str != NULL);
    SV sv;
    sv.count = strlen(str);
    sv.data = sv.count ? str : EMPTY_STR;
    return sv;
}

SV sv_from_sb(const SB *sb) {
    SV sv;
    sv.count = sb->count;
    sv.data = sb->count ? sb->items : EMPTY_STR;
    return sv;
}

SV sv_from_parts(const char *str, size_t len) {
    assert(str != NULL || len == 0);
    SV sv;
    sv.count = len;
    sv.data = (len > 0) ? str : EMPTY_STR;
    return sv;
}

int sv_eq(SV sv1, SV sv2) {
    if (sv1.count != sv2.count) return 0;
    if (sv1.count == 0) return 1;   /* обидва порожні – рівні */
    return memcmp(sv1.data, sv2.data, sv1.count) == 0;
}

int sv_eq_cstr(SV sv1, const char *str) {
    assert(str != NULL);
    size_t len = strlen(str);
    if (sv1.count != len) return 0;
    if (sv1.count == 0) return 1;
    return memcmp(sv1.data, str, sv1.count) == 0;
}

void sv_trim_left(SV *sv) {
    while (sv->count && isspace((unsigned char)*sv->data)) {
        sv->data++;
        sv->count--;
    }
}

void sv_trim_right(SV *sv) {
    while (sv->count && isspace((unsigned char)sv->data[sv->count - 1])) {
        sv->count--;
    }
}

void sv_trim(SV *sv) {
    sv_trim_left(sv);
    sv_trim_right(sv);
}

void sv_slice(SV *sv, size_t begin, size_t end) {
    if (begin > end) begin = end;
    if (end > sv->count) end = sv->count;
    sv->data += begin;
    sv->count = end - begin;
    if (sv->count == 0) sv->data = EMPTY_STR;
}

void sv_remove_suffix(SV *sv, size_t count) {
    if (count > sv->count) count = sv->count;
    sv->count -= count;
    if (sv->count == 0) sv->data = EMPTY_STR;
}

int sv_ends_with(SV sv, SV suffix) {
    if (suffix.count == 0) return 1;
    if (suffix.count > sv.count) return 0;
    return memcmp(sv.data + sv.count - suffix.count, suffix.data, suffix.count) == 0;
}

int sv_starts_with(SV sv, SV prefix) {
    if (prefix.count == 0) return 1;
    if (prefix.count > sv.count) return 0;
    return memcmp(sv.data, prefix.data, prefix.count) == 0;
}

int sv_starts_with_char(SV sv, char c) {
    return sv.count > 0 && sv.data[0] == c;
}

int sv_ends_with_char(SV sv, char c) {
    return sv.count > 0 && sv.data[sv.count - 1] == c;
}

SV sv_split_left(SV *sv, char c) {
    size_t pos = sv_find_char(*sv, c);
    if (pos == SIZE_MAX) {
        SV out = *sv;
        sv->count = 0;
        sv->data = EMPTY_STR;   /* правий залишок – порожній */
        return out;
    }
    SV out = sv_from_parts(sv->data, pos);
    sv_remove_prefix(sv, pos + 1);
    return out;
}

SV sv_split_right(SV *sv, char c) {
    size_t pos = sv_rfind_char(*sv, c);
    if (pos == SIZE_MAX) {
        /* роздільника немає: ліва частина = весь *sv, права = порожня */
        SV out = sv_new();
        return out;
    }
    SV out = sv_from_parts(sv->data + pos + 1, sv->count - pos - 1);
    sv->count = pos;
    if (sv->count == 0) sv->data = EMPTY_STR;
    return out;
}

size_t sv_find_char(SV sv, char c) {
    for (size_t i = 0; i < sv.count; i++)
        if (sv.data[i] == c) return i;
    return SIZE_MAX;
}

size_t sv_rfind_char(SV sv, char c) {
    for (size_t i = sv.count; i > 0; i--)
        if (sv.data[i - 1] == c) return i - 1;
    return SIZE_MAX;
}

int sv_consume_char(SV *sv, char c) {
    if (sv->count == 0 || sv->data[0] != c) return 0;
    sv_remove_prefix(sv, 1);
    return 1;
}

int sv_consume(SV *sv, SV prefix) {
    if (!sv_starts_with(*sv, prefix)) return 0;
    sv_remove_prefix(sv, prefix.count);
    return 1;
}



















/* ====================================================================
 *  Реалізація String Builder (SB)
 * ==================================================================== */

static void sb_grow(SB *sb, size_t size) {
    /* Перевірка на переповнення при обчисленні потрібного обсягу */
    size_t needed;
    if (!safe_add(sb->count, size, &needed) || !safe_add(needed, 1, &needed)) {
        fprintf(stderr, "Requested size too large\n");
        abort();
    }

    /* Якщо поточної ємності достатньо – нічого не робимо */
    if (needed <= sb->capacity) return;

    /* Обчислюємо нову ємність */
    size_t new_cap = sb->capacity;
    if (new_cap == 0) {
        new_cap = SB_INITIAL_CAPACITY;
    }
    while (new_cap < needed) {
        /* Перевірка на переповнення множення */
        if (new_cap > SIZE_MAX / 2) {
            new_cap = needed;   /* досягли максимуму, просто беремо потрібний */
            break;
        }
        new_cap *= 2;
    }

    /* Виділення пам'яті */
    char *new_items = realloc(sb->items, new_cap);
    if (new_items == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb->items = new_items;
    sb->capacity = new_cap;
}

void sb_append_sv(SB *sb, SV sv) {
    sb_grow(sb, sv.count);
    memcpy(sb->items + sb->count, sv.data, sv.count);
    sb->count += sv.count;
    sb->items[sb->count] = '\0';
}

SB sb_from_cstr(const char *str) {
    assert(str != NULL);
    SB sb = {0};
    size_t len = strlen(str);
    sb.count = len;
    sb.capacity = len + 1;
    sb.items = malloc(sb.capacity);
    if (sb.items == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    memcpy(sb.items, str, len + 1);
    return sb;
}

SB sb_new(void) {
    SB sb = {0};
    sb.capacity = SB_INITIAL_CAPACITY;
    sb.items = malloc(sb.capacity);
    if (sb.items == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb.items[0] = '\0';
    return sb;
}

void sb_free(SB *sb) {
    free(sb->items);
    sb->items = NULL;
    sb->count = 0;
    sb->capacity = 0;
}

void sb_append(SB *sb, const char *str) {
    assert(str != NULL);
    size_t len = strlen(str);
    sb_grow(sb, len);
    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_append_n(SB *sb, const char *str, size_t len) {
    assert(str != NULL || len == 0);
    if (len == 0) return;
    sb_grow(sb, len);
    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_reserve(SB *sb, size_t capacity) {
    if (capacity <= sb->capacity) return;
    char *new_items = realloc(sb->items, capacity);
    if (new_items == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb->items = new_items;
    sb->capacity = capacity;
}

void sb_reserve_extra(SB *sb, size_t extra) {
    if (extra == 0) return;
    size_t new_cap;
    if (!safe_add(sb->capacity, extra, &new_cap)) {
        fprintf(stderr, "Capacity overflow\n");
        abort();
    }
    char *new_items = realloc(sb->items, new_cap);
    if (new_items == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb->items = new_items;
    sb->capacity = new_cap;
}

char *sb_to_cstr(SB *sb) {
    sb->items[sb->count] = '\0';
    return sb->items;
}

void sb_append_null(SB *sb) {
    sb_grow(sb, 0);
    sb->items[sb->count] = '\0';
}

void sb_append_char(SB *sb, char c) {
    sb_grow(sb, 1);
    sb->items[sb->count++] = c;
    sb->items[sb->count] = '\0';
}

SB sb_clone(const SB *sb) {
    SB copy = {0};
    copy.count = sb->count;
    copy.capacity = sb->count + 1;
    copy.items = malloc(copy.capacity);
    if (copy.items == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    memcpy(copy.items, sb->items, sb->count + 1);
    return copy;
}

void sb_appendf(SB *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (len <= 0) {
        va_end(args);
        return;
    }
    sb_grow(sb, (size_t)len);
    vsnprintf(sb->items + sb->count, (size_t)len + 1, fmt, args);
    sb->count += (size_t)len;
    va_end(args);
}

#endif /* FDS_IMPLEMENTATION */
#endif /* FDS_H */