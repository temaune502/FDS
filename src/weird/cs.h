/*
    cs_std.h — C# Style Standard Library experiment with Garbage Collector
    STB-style single-header library.

    Для використання реалізації у ровно ОДНОМУ C-файлі додайте:
        #define CS_STD_IMPLEMENTATION
        #include "cs_std.h"
*/

#ifndef CS_STD_H
#define CS_STD_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Garbage Collector ---
typedef struct {
    void (*Init)(void);            // Має викликатися на початку main()
    void (*Collect)(void);         // Примусова збірка сміття
    void* (*Alloc)(size_t size);   // Виділення пам'яті під управлінням GC
} GC_Namespace;

extern const GC_Namespace GC;

// --- Console ---
typedef struct {
    void (*WriteLine)(const char* format, ...);
    void (*Write)(const char* format, ...);
    char* (*ReadLine)(void);       // Повертає рядок, підпорядкований GC
} Console_Namespace;

extern const Console_Namespace Console;

// --- File ---
typedef struct {
    char* (*Read)(const char* path);                 // Читає весь файл у буфер (GC)
    bool (*Exists)(const char* path);                // Перевіряє існування файлу
    bool (*Write)(const char* path, const char* text);// Записує текст у файл
} File_Namespace;

extern const File_Namespace File;

// --- Time ---
typedef struct {
    int64_t (*Now)(void);          // Повертає UNIX timestamp у мілісекундах
} Time_Namespace;

extern const Time_Namespace Time;

#ifdef __cplusplus
}
#endif

#endif // CS_STD_H

// ============================================================================
//                               РЕАЛІЗАЦІЯ
// ============================================================================
#ifdef CS_STD_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// ----------------------------------------------------------------------------
// GC: Консервативний Mark-and-Sweep Garbage Collector
// ----------------------------------------------------------------------------

typedef struct GCObject {
    void* ptr;
    size_t size;
    bool marked;
    struct GCObject* next;
} GCObject;

static GCObject* g_gc_head = NULL;
static void* g_gc_stack_base = NULL;
static size_t g_gc_allocated_bytes = 0;
static size_t g_gc_threshold = 64 * 1024; // Поріг автозбору (64 KB для тесту)

static void gc_mark_ptr(void* p) {
    if (!p) return;

    GCObject* curr = g_gc_head;
    while (curr) {
        uintptr_t start = (uintptr_t)curr->ptr;
        uintptr_t end = start + curr->size;
        uintptr_t val = (uintptr_t)p;

        // Перевіряємо, чи вказівник вказує в межі виділеного блоку
        if (val >= start && val < end) {
            if (!curr->marked) {
                curr->marked = true;
                // Рекурсивно скануємо тіло виділеного об'єкта на інші вказівники
                for (uintptr_t inner = start; inner + sizeof(void*) <= end; inner += sizeof(void*)) {
                    void* inner_ptr = *(void**)inner;
                    gc_mark_ptr(inner_ptr);
                }
            }
            break;
        }
        curr = curr->next;
    }
}

static void gc_collect_impl(void) {
    if (!g_gc_stack_base) return;

    // 1. Скидаємо прапорці маркування
    GCObject* curr = g_gc_head;
    while (curr) {
        curr->marked = false;
        curr = curr->next;
    }

    // 2. Маркуємо коріння зі стеку (від вершини до основи)
    volatile int dummy;
    void* stack_top = (void*)&dummy;

    uintptr_t low = (uintptr_t)stack_top < (uintptr_t)g_gc_stack_base ? (uintptr_t)stack_top : (uintptr_t)g_gc_stack_base;
    uintptr_t high = (uintptr_t)stack_top > (uintptr_t)g_gc_stack_base ? (uintptr_t)stack_top : (uintptr_t)g_gc_stack_base;

    for (uintptr_t p = low; p + sizeof(void*) <= high; p += sizeof(void*)) {
        void* candidate = *(void**)p;
        gc_mark_ptr(candidate);
    }

    // 3. Sweep (Очищення невідмічених об'єктів)
    GCObject** prev = &g_gc_head;
    curr = g_gc_head;
    while (curr) {
        if (!curr->marked) {
            GCObject* unreferenced = curr;
            *prev = curr->next;
            curr = curr->next;

            g_gc_allocated_bytes -= unreferenced->size;
            free(unreferenced->ptr);
            free(unreferenced);
        } else {
            prev = &curr->next;
            curr = curr->next;
        }
    }
}

static void* gc_alloc_impl(size_t size) {
    if (size == 0) return NULL;

    // Тригер автоматичного збирання сміття при перевищенні ліміту
    if (g_gc_allocated_bytes + size > g_gc_threshold) {
        gc_collect_impl();
    }

    void* ptr = malloc(size);
    if (!ptr) return NULL;

    GCObject* obj = (GCObject*)malloc(sizeof(GCObject));
    if (!obj) {
        free(ptr);
        return NULL;
    }

    obj->ptr = ptr;
    obj->size = size;
    obj->marked = false;
    obj->next = g_gc_head;
    g_gc_head = obj;

    g_gc_allocated_bytes += size;
    return ptr;
}

static void gc_init_impl(void) {
    volatile int dummy;
    g_gc_stack_base = (void*)&dummy;
}

const GC_Namespace GC = {
    .Init = gc_init_impl,
    .Collect = gc_collect_impl,
    .Alloc = gc_alloc_impl
};

// ----------------------------------------------------------------------------
// Console Implementation
// ----------------------------------------------------------------------------

static void console_write_line_impl(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static void console_write_impl(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static char* console_read_line_impl(void) {
    size_t capacity = 128;
    size_t length = 0;
    char* buffer = (char*)GC.Alloc(capacity);
    if (!buffer) return NULL;

    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        if (c == '\r') continue;
        if (length + 1 >= capacity) {
            size_t new_cap = capacity * 2;
            char* new_buf = (char*)GC.Alloc(new_cap);
            if (!new_buf) break;
            memcpy(new_buf, buffer, length);
            buffer = new_buf;
            capacity = new_cap;
        }
        buffer[length++] = (char)c;
    }
    buffer[length] = '\0';
    return buffer;
}

const Console_Namespace Console = {
    .WriteLine = console_write_line_impl,
    .Write = console_write_impl,
    .ReadLine = console_read_line_impl
};

// ----------------------------------------------------------------------------
// File Implementation
// ----------------------------------------------------------------------------

static char* file_read_impl(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char* buffer = (char*)GC.Alloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    buffer[read_bytes] = '\0';
    fclose(f);
    return buffer;
}

static bool file_exists_impl(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static bool file_write_impl(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    if (text) {
        fputs(text, f);
    }
    fclose(f);
    return true;
}

const File_Namespace File = {
    .Read = file_read_impl,
    .Exists = file_exists_impl,
    .Write = file_write_impl
};

// ----------------------------------------------------------------------------
// Time Implementation
// ----------------------------------------------------------------------------

static int64_t time_now_impl(void) {
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC)) {
        return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
#endif
    return (int64_t)time(NULL) * 1000;
}

const Time_Namespace Time = {
    .Now = time_now_impl
};

#endif // CS_STD_IMPLEMENTATION