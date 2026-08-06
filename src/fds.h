#ifndef FDS_H
#define FDS_H

#include <stddef.h>  /* for size_t */

#define SB_INITIAL_CAPACITY 16

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

/* -------------------- String Builder (SB) functions -------------------- */
SB   sb_from_cstr(const char *str);
SB   sb_new(void);
void sb_append_sv(SB *sb, SV sv);
void sb_free(SB *sb);
void sb_append(SB *sb, const char *str);
void sb_append_n(SB *sb, const char *str, size_t len);
void sb_reserve(SB *sb, size_t capacity);
void sb_reserve_extra(SB *sb, size_t extra);       /* renamed from sb_alloc */
char *sb_to_cstr(SB *sb);
void sb_append_null(SB *sb);
void sb_append_char(SB *sb, char c);
SB   sb_clone(const SB *sb);
void sb_appendf(SB *sb, const char *fmt, ...);

/* -------------------- String View (SV) functions -------------------- */
void   sv_remove_prefix(SV *sv, size_t count);
char * sv_to_cstr(SV sv);
char   sv_at(SV sv, size_t index);                 /* now pass-by-value */
SV     sv_new(void);
SV     sv_from_cstr(const char *str);
SV     sv_from_sb(const SB *sb);
SV     sv_from_parts(const char *str, size_t len);
int    sv_eq(SV sv1, SV sv2);
int    sv_eq_cstr(SV sv1, const char *str);

/* Додаткові SV-функції, яких бракувало в оригінальному заголовку */
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

/* Підключаємо необхідні стандартні заголовки */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* ====================================================================
 *  Реалізація String View (SV)
 * ==================================================================== */

void sv_remove_prefix(SV *sv, size_t count)
{
    if (count > sv->count) count = sv->count;
    sv->data += count;
    sv->count -= count;
}

char *sv_to_cstr(SV sv)
{
    char *cstr = malloc(sv.count + 1);
    if (cstr == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    memcpy(cstr, sv.data, sv.count);
    cstr[sv.count] = '\0';
    return cstr;
}

char sv_at(SV sv, size_t index)
{
    if (index >= sv.count) {
        fprintf(stderr, "Index out of bounds\n");
        abort();
    }
    return sv.data[index];
}

SV sv_new(void)
{
    SV sv = {0};
    return sv;
}

SV sv_from_cstr(const char *str)
{
    SV sv = {0};
    sv.count = strlen(str);
    sv.data = str;
    return sv;
}

SV sv_from_sb(const SB *sb)
{
    SV sv = {0};
    sv.count = sb->count;
    sv.data = sb->items;
    return sv;
}

SV sv_from_parts(const char *str, size_t len)
{
    SV sv;
    sv.data = str;
    sv.count = len;
    return sv;
}

int sv_eq(SV sv1, SV sv2)
{
    if (sv1.count != sv2.count) return 0;
    return memcmp(sv1.data, sv2.data, sv1.count) == 0;
}

int sv_eq_cstr(SV sv1, const char *str)
{
    size_t len = strlen(str);
    if (sv1.count != len) return 0;
    return memcmp(sv1.data, str, sv1.count) == 0;
}

/* ---------- допоміжні SV-функції (trim, slice, split, ...) ---------- */
void sv_trim_left(SV *sv)
{
    while (sv->count && isspace((unsigned char)*sv->data)) {
        sv->data++;
        sv->count--;
    }
}

void sv_trim_right(SV *sv)
{
    while (sv->count && isspace((unsigned char)sv->data[sv->count - 1])) {
        sv->count--;
    }
}

void sv_trim(SV *sv)
{
    sv_trim_left(sv);
    sv_trim_right(sv);
}

void sv_slice(SV *sv, size_t begin, size_t end)
{
    if (begin > end) begin = end;
    if (end > sv->count) end = sv->count;
    sv->data += begin;
    sv->count = end - begin;
}

void sv_remove_suffix(SV *sv, size_t count)
{
    if (count > sv->count) count = sv->count;
    sv->count -= count;
}

int sv_ends_with(SV sv, SV suffix)
{
    if (suffix.count > sv.count) return 0;
    return memcmp(sv.data + sv.count - suffix.count, suffix.data, suffix.count) == 0;
}

int sv_starts_with(SV sv, SV prefix)
{
    if (prefix.count > sv.count) return 0;
    return memcmp(sv.data, prefix.data, prefix.count) == 0;
}

int sv_starts_with_char(SV sv, char c)
{
    return sv.count && sv.data[0] == c;
}

int sv_ends_with_char(SV sv, char c)
{
    return sv.count && sv.data[sv.count - 1] == c;
}

SV sv_split_left(SV *sv, char c)
{
    size_t pos = sv_find_char(*sv, c);
    if (pos == SIZE_MAX) {
        SV out = *sv;
        sv->count = 0;                 /* правий залишок – порожній */
        return out;
    }
    SV out = sv_from_parts(sv->data, pos);
    sv_remove_prefix(sv, pos + 1);
    return out;
}

SV sv_split_right(SV *sv, char c)
{
    size_t pos = sv_rfind_char(*sv, c);
    if (pos == SIZE_MAX) {
        /* роздільника немає: ліва частина = весь *sv, права = порожня */
        return sv_new();
    }
    /* out — права частина (після роздільника), *sv стає лівою */
    SV out = sv_from_parts(sv->data + pos + 1, sv->count - pos - 1);
    sv->count = pos;                  /* ліва частина залишається в *sv */
    return out;
}

size_t sv_find_char(SV sv, char c)
{
    for (size_t i = 0; i < sv.count; i++)
        if (sv.data[i] == c) return i;
    return SIZE_MAX;
}

size_t sv_rfind_char(SV sv, char c)
{
    for (size_t i = sv.count; i > 0; i--)
        if (sv.data[i - 1] == c) return i - 1;
    return SIZE_MAX;
}

int sv_consume_char(SV *sv, char c)
{
    if (sv->count == 0 || sv->data[0] != c) return 0;
    sv_remove_prefix(sv, 1);
    return 1;
}

int sv_consume(SV *sv, SV prefix)
{
    if (!sv_starts_with(*sv, prefix)) return 0;
    sv_remove_prefix(sv, prefix.count);
    return 1;
}


/* ====================================================================
 *  Реалізація String Builder (SB)
 * ==================================================================== */

static void sb_grow(SB *sb, size_t size)
{
    /* Виправлено: захист від нульової ємності */
    if (sb->capacity == 0)
        sb->capacity = SB_INITIAL_CAPACITY;

    if (sb->count + size + 1 <= sb->capacity)
        return;

    while (sb->count + size + 1 > sb->capacity)
        sb->capacity *= 2;

    void *item = realloc(sb->items, sb->capacity);
    if (item == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb->items = item;
}

void sb_append_sv(SB *sb, SV sv)
{
    sb_grow(sb, sv.count);
    memcpy(sb->items + sb->count, sv.data, sv.count);
    sb->count += sv.count;
    sb->items[sb->count] = '\0';
}

SB sb_from_cstr(const char *str)
{
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

SB sb_new(void)
{
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

void sb_free(SB *sb)
{
    free(sb->items);
    sb->items = NULL;
    sb->count = 0;
    sb->capacity = 0;
}

void sb_append(SB *sb, const char *str)
{
    size_t len = strlen(str);
    sb_grow(sb, len);
    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_append_n(SB *sb, const char *str, size_t len)
{
    sb_grow(sb, len);
    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_reserve(SB *sb, size_t capacity)
{
    if (capacity <= sb->capacity) return;
    void *item = realloc(sb->items, capacity);
    if (item == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb->items = item;
    sb->capacity = capacity;
}

/* Перейменовано з sb_alloc – тепер назва відбиває, що ємність збільшується */
void sb_reserve_extra(SB *sb, size_t extra)
{
    if (extra == 0) return;
    sb->capacity += extra;
    void *item = realloc(sb->items, sb->capacity);
    if (item == NULL) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }
    sb->items = item;
}

char *sb_to_cstr(SB *sb)
{
    sb->items[sb->count] = '\0';
    return sb->items;
}

void sb_append_null(SB *sb)
{
    sb_grow(sb, 0);
    sb->items[sb->count] = '\0';
}

void sb_append_char(SB *sb, char c)
{
    sb_grow(sb, 1);
    sb->items[sb->count++] = c;
    sb->items[sb->count] = '\0';
}

SB sb_clone(const SB *sb)
{
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

void sb_appendf(SB *sb, const char *fmt, ...)
{
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
    vsnprintf(sb->items + sb->count, len + 1, fmt, args);
    sb->count += (size_t)len;
    va_end(args);
}

#endif /* FDS_IMPLEMENTATION */
#endif /* FDS_H */