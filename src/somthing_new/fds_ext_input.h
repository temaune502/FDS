/*
    fds_ext_input.h — Простий та гнучкий консольний ввід для FDS
    
    ВИКОРИСТАННЯ:
      #define FDS_EXT_INPUT_IMPLEMENTATION
      #include "fds_ext_input.h"
*/

#ifndef FDS_EXT_INPUT_H
#define FDS_EXT_INPUT_H

// #include "fds.h" // Очікує SB та fds_str_from_parts()
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- ОСНОВНИЙ API ---

// Python-like input: виводить prompt (якщо не NULL), чекає Enter, повертає SB.
// Буфер автоматично реалокується і перевикористовується між викликами.
SB fds_input(const char *prompt);

// Аліас для максимальної лаконічності
#define input(prompt) fds_input(prompt)

// Помічники для швидкого читання чисел із заповненням дефолтного значення при помилці
int64_t fds_input_int(const char *prompt, int64_t default_val);
double  fds_input_float(const char *prompt, double default_val);

// --- ІСТОРІЯ ВВОДУ ---

void    fds_input_history_add(SB str);
SB fds_input_history_get(size_t index);
size_t  fds_input_history_count(void);
void    fds_input_history_clear(void);
void    fds_input_history_set_max(size_t max_items); // 0 = безліміт

// Увімкнути/вимкнути автоматичне збереження кожного вводу в історію
void    fds_input_set_auto_history(bool enable);

// Очищення внутрішніх буферів та пам'яті історії
void    fds_input_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_INPUT_H

// ============================================================================
//                               РЕАЛІЗАЦІЯ
// ============================================================================

#ifdef FDS_EXT_INPUT_IMPLEMENTATION

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
    size_t max_items;
} Fds_InputHistoryInternal;

typedef struct {
    char *buf;
    size_t cap;
    size_t count;
    
    Fds_InputHistoryInternal history;
    bool auto_history;
} Fds_InputState;

static Fds_InputState g_fds_input = {
    .buf = NULL,
    .cap = 0,
    .count = 0,
    .history = { .items = NULL, .count = 0, .capacity = 0, .max_items = 100 },
    .auto_history = false
};

SB fds_input(const char *prompt) {
    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    g_fds_input.count = 0;
    int c;

    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (g_fds_input.count + 2 >= g_fds_input.cap) {
            size_t new_cap = g_fds_input.cap == 0 ? 128 : g_fds_input.cap * 2;
            char *new_buf = (char *)realloc(g_fds_input.buf, new_cap);
            if (!new_buf) break;
            g_fds_input.buf = new_buf;
            g_fds_input.cap = new_cap;
        }
        g_fds_input.buf[g_fds_input.count++] = (char)c;
    }

    // Тримінг Windows CRLF ('\r')
    if (g_fds_input.count > 0 && g_fds_input.buf[g_fds_input.count - 1] == '\r') {
        g_fds_input.count--;
    }

    if (g_fds_input.buf) {
        g_fds_input.buf[g_fds_input.count] = '\0';
    } else {
        static char empty_str[1] = "";
        return sb_from_cstr(empty_str);
    }

    SB result = sv_to_sb(sv_from_parts(g_fds_input.buf, g_fds_input.count));

    if (g_fds_input.auto_history && g_fds_input.count > 0) {
        fds_input_history_add(result);
    }

    return result;
}

int64_t fds_input_int(const char *prompt, int64_t default_val) {
    SB str = fds_input(prompt);
    if (str.count == 0) return default_val;

    char *endptr = NULL;
    int64_t val = strtoll(str.items, &endptr, 10);
    if (endptr == str.items) return default_val;
    return val;
}

double fds_input_float(const char *prompt, double default_val) {
    SB str = fds_input(prompt);
    if (str.count == 0) return default_val;

    char *endptr = NULL;
    double val = strtod(str.items, &endptr);
    if (endptr == str.items) return default_val;
    return val;
}

void fds_input_history_add(SB str) {
    if (!str.items || str.count == 0) return;
    Fds_InputHistoryInternal *h = &g_fds_input.history;

    if (h->max_items > 0 && h->count >= h->max_items) {
        free(h->items[0]);
        memmove(h->items, h->items + 1, sizeof(char *) * (h->count - 1));
        h->count--;
    }

    if (h->count >= h->capacity) {
        size_t new_cap = h->capacity == 0 ? 16 : h->capacity * 2;
        char **new_items = (char **)realloc(h->items, sizeof(char *) * new_cap);
        if (!new_items) return;
        h->items = new_items;
        h->capacity = new_cap;
    }

    char *copy = (char *)malloc(str.count + 1);
    if (!copy) return;
    memcpy(copy, str.items, str.count);
    copy[str.count] = '\0';

    h->items[h->count++] = copy;
}

SB fds_input_history_get(size_t index) {
    Fds_InputHistoryInternal *h = &g_fds_input.history;
    if (index >= h->count) return sb_from_cstr("");
    return sv_to_sb(sv_from_parts(h->items[index], strlen(h->items[index])));
}

size_t fds_input_history_count(void) {
    return g_fds_input.history.count;
}

void fds_input_history_clear(void) {
    Fds_InputHistoryInternal *h = &g_fds_input.history;
    for (size_t i = 0; i < h->count; i++) {
        free(h->items[i]);
    }
    free(h->items);
    h->items = NULL;
    h->count = 0;
    h->capacity = 0;
}

void fds_input_history_set_max(size_t max_items) {
    g_fds_input.history.max_items = max_items;
}

void fds_input_set_auto_history(bool enable) {
    g_fds_input.auto_history = enable;
}

void fds_input_cleanup(void) {
    if (g_fds_input.buf) {
        free(g_fds_input.buf);
        g_fds_input.buf = NULL;
        g_fds_input.cap = 0;
        g_fds_input.count = 0;
    }
    fds_input_history_clear();
}

#endif // FDS_EXT_INPUT_IMPLEMENTATION