#ifndef FDS2_ALLOCATOR_H
#define FDS2_ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fds2_allocator fds2_allocator;

static inline bool fds2_size_add_overflow(size_t left, size_t right, size_t *result) {
    if (left > SIZE_MAX - right) return true;
    if (result) *result = left + right;
    return false;
}

static inline bool fds2_size_mul_overflow(size_t left, size_t right, size_t *result) {
    if (left != 0 && right > SIZE_MAX / left) return true;
    if (result) *result = left * right;
    return false;
}

typedef struct fds2_allocator_stats {
    size_t alloc_count;
    size_t realloc_count;
    size_t free_count;
    size_t tmp_alloc_calls;
    size_t tmp_new_blocks;
    size_t permanent_count;
    size_t current_allocated;
    size_t peak_allocated;
    size_t total_allocated;
    size_t total_freed;
} fds2_allocator_stats;

struct fds2_allocator {
    void* (*alloc_fn)(fds2_allocator *a, size_t size);
    void* (*calloc_fn)(fds2_allocator *a, size_t num, size_t size);
    void* (*realloc_fn)(fds2_allocator *a, void *ptr, size_t size);
    void  (*free_fn)(fds2_allocator *a, void *ptr);
    void* (*alloc_tmp_fn)(fds2_allocator *a, size_t size);
    void* (*alloc_permanent_fn)(fds2_allocator *a, size_t size);

    void *all_blocks;
    void *tmp_active;
    void *tmp_free;

    size_t live_blocks_count;

#ifdef DEBUG_MEM
    size_t stats_alloc_count;
    size_t stats_realloc_count;
    size_t stats_free_count;
    size_t stats_tmp_alloc_calls;
    size_t stats_tmp_new_blocks;
    size_t stats_permanent_count;
    size_t stats_current_allocated;
    size_t stats_peak_allocated;
    size_t stats_total_allocated;
    size_t stats_total_freed;
#endif
};

// Створення та знищення алокатора
fds2_allocator* fds2_allocator_create(void);
void fds2_allocator_destroy(fds2_allocator *a);

// Керування контекстом (TLS)
void fds2_allocator_push(fds2_allocator *a);
void fds2_allocator_pop(void);
fds2_allocator* fds2_allocator_current(void);

// Явне очищення тимчасової пам'яті
void fds2_allocator_clear_tmp(fds2_allocator *a);

// Статистика та логування
size_t fds2_allocator_live_blocks_count(fds2_allocator *a);
#ifdef DEBUG_MEM
void fds2_allocator_get_stats(fds2_allocator *a, fds2_allocator_stats *out_stats);
void fds2_allocator_print_stats(fds2_allocator *a);
#endif

// Внутрішні реалізації алокації
void* fds2_alloc_impl(fds2_allocator *a, size_t size);
void* fds2_calloc_impl(fds2_allocator *a, size_t num, size_t size);
void* fds2_realloc_impl(fds2_allocator *a, void *ptr, size_t size);
void  fds2_free_impl(fds2_allocator *a, void *ptr);
void* fds2_alloc_tmp_impl(fds2_allocator *a, size_t size);
void* fds2_alloc_permanent_impl(fds2_allocator *a, size_t size);

#ifdef DEBUG_MEM
void* fds2_alloc_impl_tracked(fds2_allocator *a, size_t size, const char *file, int line);
void* fds2_calloc_impl_tracked(fds2_allocator *a, size_t num, size_t size, const char *file, int line);
void* fds2_realloc_impl_tracked(fds2_allocator *a, void *ptr, size_t size, const char *file, int line);
void* fds2_alloc_tmp_impl_tracked(fds2_allocator *a, size_t size, const char *file, int line);
void* fds2_alloc_permanent_impl_tracked(fds2_allocator *a, size_t size, const char *file, int line);
#endif

// Макроси-обгортки
#ifdef DEBUG_MEM
    #define fds2_alloc(size)                fds2_alloc_impl_tracked(NULL, size, __FILE__, __LINE__)
    #define fds2_calloc(num, size)          fds2_calloc_impl_tracked(NULL, num, size, __FILE__, __LINE__)
    #define fds2_free(ptr)                  fds2_free_impl(NULL, ptr)
    #define fds2_realloc(ptr, size)         fds2_realloc_impl_tracked(NULL, ptr, size, __FILE__, __LINE__)
    #define fds2_alloc_tmp(size)            fds2_alloc_tmp_impl_tracked(NULL, size, __FILE__, __LINE__)
    #define fds2_alloc_permanent(size)      fds2_alloc_permanent_impl_tracked(NULL, size, __FILE__, __LINE__)
    
    #define fds2_alloc_a(a, size)           fds2_alloc_impl_tracked(a, size, __FILE__, __LINE__)
    #define fds2_calloc_a(a, num, size)     fds2_calloc_impl_tracked(a, num, size, __FILE__, __LINE__)
    #define fds2_free_a(a, ptr)             fds2_free_impl(a, ptr)
    #define fds2_realloc_a(a, ptr, size)    fds2_realloc_impl_tracked(a, ptr, size, __FILE__, __LINE__)
    #define fds2_alloc_tmp_a(a, size)       fds2_alloc_tmp_impl_tracked(a, size, __FILE__, __LINE__)
    #define fds2_alloc_permanent_a(a, size) fds2_alloc_permanent_impl_tracked(a, size, __FILE__, __LINE__)
#else
    #define fds2_alloc(size)                fds2_alloc_impl(NULL, size)
    #define fds2_calloc(num, size)          fds2_calloc_impl(NULL, num, size)
    #define fds2_free(ptr)                  fds2_free_impl(NULL, ptr)
    #define fds2_realloc(ptr, size)         fds2_realloc_impl(NULL, ptr, size)
    #define fds2_alloc_tmp(size)            fds2_alloc_tmp_impl(NULL, size)
    #define fds2_alloc_permanent(size)      fds2_alloc_permanent_impl(NULL, size)
    
    #define fds2_alloc_a(a, size)           fds2_alloc_impl(a, size)
    #define fds2_calloc_a(a, num, size)     fds2_calloc_impl(a, num, size)
    #define fds2_free_a(a, ptr)             fds2_free_impl(a, ptr)
    #define fds2_realloc_a(a, ptr, size)    fds2_realloc_impl(a, ptr, size)
    #define fds2_alloc_tmp_a(a, size)       fds2_alloc_tmp_impl(a, size)
    #define fds2_alloc_permanent_a(a, size) fds2_alloc_permanent_impl(a, size)
#endif

#ifdef __cplusplus
}
#endif

#endif // FDS2_ALLOCATOR_H

#ifdef FDS2_ALLOCATOR_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FDS2_ALIGNMENT 16
#define FDS2_ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))

typedef struct block_header {
    size_t size;
    struct block_header *next;
    struct block_header *prev;
    int is_tmp;
#ifdef DEBUG_MEM
    const char *file;
    int line;
    int is_permanent;
#endif
} block_header;

#define HEADER_SIZE FDS2_ALIGN_UP(sizeof(block_header), FDS2_ALIGNMENT)

static block_header *header_from_ptr(void *ptr) {
    return (block_header*)((char*)ptr - HEADER_SIZE);
}

static void *ptr_from_header(block_header *h) {
    return (void*)((char*)h + HEADER_SIZE);
}

static block_header *raw_alloc_block(size_t size) {
    block_header *h = (block_header*)malloc(HEADER_SIZE + size);
    if (!h) return NULL;
    h->size = size;
    h->next = NULL;
    h->prev = NULL;
    h->is_tmp = 0;
#ifdef DEBUG_MEM
    h->file = NULL;
    h->line = 0;
    h->is_permanent = 0;
#endif
    return h;
}

static void raw_free_block(block_header *h) {
    free(h);
}

static void list_push(block_header **head, block_header *h) {
    h->next = *head;
    h->prev = NULL;
    if (*head) {
        (*head)->prev = h;
    }
    *head = h;
}

static void list_remove(block_header **head, block_header *target) {
    if (target->prev) {
        target->prev->next = target->next;
    } else {
        *head = target->next;
    }
    if (target->next) {
        target->next->prev = target->prev;
    }
    target->prev = NULL;
    target->next = NULL;
}

void fds2_allocator_clear_tmp(fds2_allocator *a) {
    if (!a) a = fds2_allocator_current();
    if (!a || !a->tmp_active) return;
    
    block_header *active = (block_header*)a->tmp_active;
    block_header *last = active;
    while (last->next) last = last->next;
    
    last->next = (block_header*)a->tmp_free;
    if (a->tmp_free) {
        ((block_header*)a->tmp_free)->prev = last;
    }
    
    a->tmp_free = active;
    active->prev = NULL;
    a->tmp_active = NULL;
}

#ifdef DEBUG_MEM
static void update_peak(fds2_allocator *a) {
    if (a->stats_current_allocated > a->stats_peak_allocated) {
        a->stats_peak_allocated = a->stats_current_allocated;
    }
}
#endif

static void *alloc_internal(fds2_allocator *a, size_t size, const char *file, int line, int is_permanent) {
    if (!a) a = fds2_allocator_current();

    block_header *h = raw_alloc_block(size);
    if (!h) return NULL;
    
    h->is_tmp = 0;
    list_push((block_header**)&a->all_blocks, h);
    a->live_blocks_count++;

#ifdef DEBUG_MEM
    h->file = file;
    h->line = line;
    h->is_permanent = is_permanent;
    if (is_permanent) a->stats_permanent_count++;
    a->stats_alloc_count++;
    a->stats_current_allocated += size;
    a->stats_total_allocated += size;
    update_peak(a);
#else
    (void)file; (void)line; (void)is_permanent;
#endif

    return ptr_from_header(h);
}

void *fds2_alloc_impl(fds2_allocator *a, size_t size) {
    return alloc_internal(a, size, NULL, 0, 0);
}

void *fds2_calloc_impl(fds2_allocator *a, size_t num, size_t size) {
    if (!a) a = fds2_allocator_current();
    size_t total = num * size;
    void *ptr = alloc_internal(a, total, NULL, 0, 0);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *fds2_realloc_impl(fds2_allocator *a, void *ptr, size_t new_size) {
    if (!a) a = fds2_allocator_current();
    if (ptr == NULL) return alloc_internal(a, new_size, NULL, 0, 0);
    if (new_size == 0) {
        fds2_free_impl(a, ptr);
        return NULL;
    }
    
    block_header *h = header_from_ptr(ptr);
    if (h->is_tmp) {
        list_remove((block_header**)&a->tmp_active, h);
    } else {
        list_remove((block_header**)&a->all_blocks, h);
    }
    
    block_header *new_h = (block_header*)realloc(h, HEADER_SIZE + new_size);
    if (!new_h) {
        if (h->is_tmp) {
            list_push((block_header**)&a->tmp_active, h);
        } else {
            list_push((block_header**)&a->all_blocks, h);
        }
        return NULL;
    }
    
    new_h->size = new_size;
    if (new_h->is_tmp) {
        list_push((block_header**)&a->tmp_active, new_h);
    } else {
        list_push((block_header**)&a->all_blocks, new_h);
    }
    return ptr_from_header(new_h);
}

void fds2_free_impl(fds2_allocator *a, void *ptr) {
    if (!a) a = fds2_allocator_current();
    if (!ptr) return;

    block_header *h = header_from_ptr(ptr);
    if (h->is_tmp) {
        list_remove((block_header**)&a->tmp_active, h);
    } else {
        list_remove((block_header**)&a->all_blocks, h);
        a->live_blocks_count--;
#ifdef DEBUG_MEM
        a->stats_free_count++;
        a->stats_current_allocated -= h->size;
        a->stats_total_freed += h->size;
#endif
    }
    raw_free_block(h);
}

void *fds2_alloc_tmp_impl(fds2_allocator *a, size_t size) {
    if (!a) a = fds2_allocator_current();
    block_header *h = NULL;
    block_header **indirect = (block_header**)&a->tmp_free;
    
    while (*indirect) {
        if ((*indirect)->size >= size) {
            h = *indirect;
            *indirect = h->next;
            if (*indirect) (*indirect)->prev = NULL;
            break;
        }
        indirect = &(*indirect)->next;
    }
    
    if (!h) {
        h = raw_alloc_block(size);
        if (!h) return NULL;
    } else {
        h->size = size;
    }
    
    h->is_tmp = 1;
    list_push((block_header**)&a->tmp_active, h);
    return ptr_from_header(h);
}

void *fds2_alloc_permanent_impl(fds2_allocator *a, size_t size) {
    return alloc_internal(a, size, NULL, 0, 1);
}

#ifdef DEBUG_MEM
void *fds2_alloc_impl_tracked(fds2_allocator *a, size_t size, const char *file, int line) {
    return alloc_internal(a, size, file, line, 0);
}

void *fds2_calloc_impl_tracked(fds2_allocator *a, size_t num, size_t size, const char *file, int line) {
    if (!a) a = fds2_allocator_current();
    size_t total = num * size;
    void *ptr = alloc_internal(a, total, file, line, 0);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *fds2_realloc_impl_tracked(fds2_allocator *a, void *ptr, size_t new_size, const char *file, int line) {
    if (!a) a = fds2_allocator_current();
    if (ptr == NULL) return alloc_internal(a, new_size, file, line, 0);
    if (new_size == 0) {
        fds2_free_impl(a, ptr);
        return NULL;
    }
    
    block_header *h = header_from_ptr(ptr);
    if (h->is_tmp) {
        list_remove((block_header**)&a->tmp_active, h);
    } else {
        list_remove((block_header**)&a->all_blocks, h);
    }
    
    size_t old_size = h->size;
    block_header *new_h = (block_header*)realloc(h, HEADER_SIZE + new_size);
    if (!new_h) {
        if (h->is_tmp) {
            list_push((block_header**)&a->tmp_active, h);
        } else {
            list_push((block_header**)&a->all_blocks, h);
        }
        return NULL;
    }
    
    new_h->size = new_size;
    new_h->file = file;
    new_h->line = line;
    
    if (new_h->is_tmp) {
        list_push((block_header**)&a->tmp_active, new_h);
    } else {
        list_push((block_header**)&a->all_blocks, new_h);
        a->stats_realloc_count++;
        if (new_size > old_size) {
            a->stats_current_allocated += (new_size - old_size);
            a->stats_total_allocated += (new_size - old_size);
        } else {
            a->stats_current_allocated -= (old_size - new_size);
            a->stats_total_freed += (old_size - new_size);
        }
        update_peak(a);
    }

    return ptr_from_header(new_h);
}

void *fds2_alloc_tmp_impl_tracked(fds2_allocator *a, size_t size, const char *file, int line) {
    if (!a) a = fds2_allocator_current();
    block_header *h = NULL;
    block_header **indirect = (block_header**)&a->tmp_free;
    
    while (*indirect) {
        if ((*indirect)->size >= size) {
            h = *indirect;
            *indirect = h->next;
            if (*indirect) (*indirect)->prev = NULL;
            break;
        }
        indirect = &(*indirect)->next;
    }

    int new_block_created = 0;
    if (!h) {
        h = raw_alloc_block(size);
        if (!h) return NULL;
        new_block_created = 1;
    } else {
        h->size = size;
    }
    
    h->is_tmp = 1;
    h->file = file;
    h->line = line;
    h->is_permanent = 0; 
    list_push((block_header**)&a->tmp_active, h);

    a->stats_tmp_alloc_calls++;
    if (new_block_created) {
        a->stats_tmp_new_blocks++;
        a->stats_current_allocated += size;
        a->stats_total_allocated += size;
        update_peak(a);
    }

    return ptr_from_header(h);
}

void *fds2_alloc_permanent_impl_tracked(fds2_allocator *a, size_t size, const char *file, int line) {
    return alloc_internal(a, size, file, line, 1);
}
#endif

fds2_allocator *fds2_allocator_create(void) {
    fds2_allocator *a = (fds2_allocator*)calloc(1, sizeof(fds2_allocator));
    if (!a) return NULL;

    a->alloc_fn = fds2_alloc_impl;
    a->calloc_fn = fds2_calloc_impl;
    a->realloc_fn = fds2_realloc_impl;
    a->free_fn = fds2_free_impl;
    a->alloc_tmp_fn = fds2_alloc_tmp_impl;
    a->alloc_permanent_fn = fds2_alloc_permanent_impl;

    return a;
}

void fds2_allocator_destroy(fds2_allocator *a) {
    if (!a) return;

#ifdef DEBUG_MEM
    block_header *h_leak = (block_header*)a->all_blocks;
    int leak_count = 0;
    fprintf(stderr, "=== Leak report for allocator %p ===\n", (void*)a);
    while (h_leak) {
        if (!h_leak->is_permanent) {
            fprintf(stderr, "LEAK: block of %zu bytes allocated at %s:%d\n",
                    h_leak->size, h_leak->file ? h_leak->file : "unknown", h_leak->line);
            leak_count++;
        }
        h_leak = h_leak->next;
    }
    if (leak_count == 0) {
        fprintf(stderr, "No non-permanent leaks detected.\n");
    } else {
        fprintf(stderr, "Total non-permanent leaks: %d\n", leak_count);
    }
    fprintf(stderr, "===================================\n");
#endif

    void *lists[] = { a->all_blocks, a->tmp_active, a->tmp_free };
    for (int i = 0; i < 3; i++) {
        block_header *curr = (block_header*)lists[i];
        while (curr) {
            block_header *next = curr->next;
            raw_free_block(curr);
            curr = next;
        }
    }

    free(a);
}

#define FDS2_MAX_ALLOC_STACK 16

static _Thread_local fds2_allocator *tls_current_allocator = NULL;
static _Thread_local fds2_allocator *tls_alloc_stack[FDS2_MAX_ALLOC_STACK];
static _Thread_local int tls_alloc_stack_depth = 0;

fds2_allocator *fds2_allocator_current(void) {
    if (tls_current_allocator == NULL) {
        static _Thread_local fds2_allocator *default_allocator = NULL;
        if (default_allocator == NULL) {
            default_allocator = fds2_allocator_create();
        }
        return default_allocator;
    }
    return tls_current_allocator;
}

void fds2_allocator_push(fds2_allocator *a) {
    if (tls_alloc_stack_depth < FDS2_MAX_ALLOC_STACK) {
        tls_alloc_stack[tls_alloc_stack_depth++] = tls_current_allocator;
    }
    tls_current_allocator = a;
}

void fds2_allocator_pop(void) {
    if (tls_alloc_stack_depth > 0) {
        tls_current_allocator = tls_alloc_stack[--tls_alloc_stack_depth];
    } else {
        tls_current_allocator = NULL;
    }
}

size_t fds2_allocator_live_blocks_count(fds2_allocator *a) {
    if (!a) a = fds2_allocator_current();
    return a->live_blocks_count;
}

#ifdef DEBUG_MEM
void fds2_allocator_get_stats(fds2_allocator *a, fds2_allocator_stats *out_stats) {
    if (!a) a = fds2_allocator_current();
    if (!out_stats) return;
    
    out_stats->alloc_count = a->stats_alloc_count;
    out_stats->realloc_count = a->stats_realloc_count;
    out_stats->free_count = a->stats_free_count;
    out_stats->tmp_alloc_calls = a->stats_tmp_alloc_calls;
    out_stats->tmp_new_blocks = a->stats_tmp_new_blocks;
    out_stats->permanent_count = a->stats_permanent_count;
    out_stats->current_allocated = a->stats_current_allocated;
    out_stats->peak_allocated = a->stats_peak_allocated;
    out_stats->total_allocated = a->stats_total_allocated;
    out_stats->total_freed = a->stats_total_freed;
}

void fds2_allocator_print_stats(fds2_allocator *a) {
    if (!a) a = fds2_allocator_current();
    fds2_allocator_stats s;
    fds2_allocator_get_stats(a, &s);
    
    fprintf(stderr, "=== Allocator stats ===\n");
    fprintf(stderr, "Alloc count:        %zu\n", s.alloc_count);
    fprintf(stderr, "Realloc count:      %zu\n", s.realloc_count);
    fprintf(stderr, "Free count:         %zu\n", s.free_count);
    fprintf(stderr, "Temp alloc calls:   %zu\n", s.tmp_alloc_calls);
    fprintf(stderr, "Temp new blocks:    %zu\n", s.tmp_new_blocks);
    fprintf(stderr, "Temp reuse count:   %zu\n", s.tmp_alloc_calls - s.tmp_new_blocks);
    fprintf(stderr, "Permanent blocks:   %zu\n", s.permanent_count);
    fprintf(stderr, "Current allocated:  %zu bytes\n", s.current_allocated);
    fprintf(stderr, "Peak allocated:     %zu bytes\n", s.peak_allocated);
    fprintf(stderr, "Total allocated:    %zu bytes\n", s.total_allocated);
    fprintf(stderr, "Total freed:        %zu bytes\n", s.total_freed);
    fprintf(stderr, "=======================\n");
}
#endif

#endif // FDS2_ALLOCATOR_IMPLEMENTATION