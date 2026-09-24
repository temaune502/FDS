#ifndef FDS_ALLOCATOR_H
#define FDS_ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fds_allocator fds_allocator;

typedef struct fds_allocator_stats {
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
} fds_allocator_stats;

struct fds_allocator {
    void* (*alloc_fn)(fds_allocator *a, size_t size);
    void* (*calloc_fn)(fds_allocator *a, size_t num, size_t size);
    void* (*realloc_fn)(fds_allocator *a, void *ptr, size_t size);
    void  (*free_fn)(fds_allocator *a, void *ptr);
    void* (*alloc_tmp_fn)(fds_allocator *a, size_t size);
    void* (*alloc_permanent_fn)(fds_allocator *a, size_t size);

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
fds_allocator* fds_allocator_create(void);
void fds_allocator_destroy(fds_allocator *a);

// Керування контекстом (TLS)
void fds_allocator_push(fds_allocator *a);
void fds_allocator_pop(void);
fds_allocator* fds_allocator_current(void);

// Явне очищення тимчасової пам'яті
void fds_allocator_clear_tmp(fds_allocator *a);

// Статистика та логування
size_t fds_allocator_live_blocks_count(fds_allocator *a);
#ifdef DEBUG_MEM
void fds_allocator_get_stats(fds_allocator *a, fds_allocator_stats *out_stats);
void fds_allocator_print_stats(fds_allocator *a);
#endif

// Внутрішні реалізації алокації (Оригінальний API)
void* fds_alloc_impl(fds_allocator *a, size_t size);
void* fds_calloc_impl(fds_allocator *a, size_t num, size_t size);
void* fds_realloc_impl(fds_allocator *a, void *ptr, size_t size);
void  fds_free_impl(fds_allocator *a, void *ptr);
void* fds_alloc_tmp_impl(fds_allocator *a, size_t size);
void* fds_alloc_permanent_impl(fds_allocator *a, size_t size);

#ifdef DEBUG_MEM
void* fds_alloc_impl_tracked(fds_allocator *a, size_t size, const char *file, int line);
void* fds_calloc_impl_tracked(fds_allocator *a, size_t num, size_t size, const char *file, int line);
void* fds_realloc_impl_tracked(fds_allocator *a, void *ptr, size_t size, const char *file, int line);
void* fds_alloc_tmp_impl_tracked(fds_allocator *a, size_t size, const char *file, int line);
void* fds_alloc_permanent_impl_tracked(fds_allocator *a, size_t size, const char *file, int line);
#endif

// Макроси-обгортки (Оригінальні)
#ifdef DEBUG_MEM
    #define fds_alloc(size)                fds_alloc_impl_tracked(NULL, size, __FILE__, __LINE__)
    #define fds_calloc(num, size)          fds_calloc_impl_tracked(NULL, num, size, __FILE__, __LINE__)
    #define fds_free(ptr)                  fds_free_impl(NULL, ptr)
    #define fds_realloc(ptr, size)         fds_realloc_impl_tracked(NULL, ptr, size, __FILE__, __LINE__)
    #define fds_alloc_tmp(size)            fds_alloc_tmp_impl_tracked(NULL, size, __FILE__, __LINE__)
    #define fds_alloc_permanent(size)      fds_alloc_permanent_impl_tracked(NULL, size, __FILE__, __LINE__)
    
    #define fds_alloc_a(a, size)           fds_alloc_impl_tracked(a, size, __FILE__, __LINE__)
    #define fds_calloc_a(a, num, size)     fds_calloc_impl_tracked(a, num, size, __FILE__, __LINE__)
    #define fds_free_a(a, ptr)             fds_free_impl(a, ptr)
    #define fds_realloc_a(a, ptr, size)    fds_realloc_impl_tracked(a, ptr, size, __FILE__, __LINE__)
    #define fds_alloc_tmp_a(a, size)       fds_alloc_tmp_impl_tracked(a, size, __FILE__, __LINE__)
    #define fds_alloc_permanent_a(a, size) fds_alloc_permanent_impl_tracked(a, size, __FILE__, __LINE__)
#else
    #define fds_alloc(size)                fds_alloc_impl(NULL, size)
    #define fds_calloc(num, size)          fds_calloc_impl(NULL, num, size)
    #define fds_free(ptr)                  fds_free_impl(NULL, ptr)
    #define fds_realloc(ptr, size)         fds_realloc_impl(NULL, ptr, size)
    #define fds_alloc_tmp(size)            fds_alloc_tmp_impl(NULL, size)
    #define fds_alloc_permanent(size)      fds_alloc_permanent_impl(NULL, size)
    
    #define fds_alloc_a(a, size)           fds_alloc_impl(a, size)
    #define fds_calloc_a(a, num, size)     fds_calloc_impl(a, num, size)
    #define fds_free_a(a, ptr)             fds_free_impl(a, ptr)
    #define fds_realloc_a(a, ptr, size)    fds_realloc_impl(a, ptr, size)
    #define fds_alloc_tmp_a(a, size)       fds_alloc_tmp_impl(a, size)
    #define fds_alloc_permanent_a(a, size) fds_alloc_permanent_impl(a, size)
#endif

#ifdef __cplusplus
}
#endif

#endif // FDS_ALLOCATOR_H

#ifdef FDS_ALLOCATOR_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FDS_MAGIC 0x46445332U
#define FDS_ALIGNMENT 16
#define FDS_ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))

typedef struct block_header {
    uint32_t magic;
    uint32_t is_tmp : 1;
    uint32_t is_permanent : 1;
    
    fds_allocator *owner; // Захист від segfault — фіксуємо реального власника
    
    size_t size;
    struct block_header *next;
    struct block_header *prev;
#ifdef DEBUG_MEM
    const char *file;
    int line;
#endif
} block_header;

#define HEADER_SIZE FDS_ALIGN_UP(sizeof(block_header), FDS_ALIGNMENT)

static inline block_header *header_from_ptr(void *ptr) {
    if (!ptr) return NULL;
    block_header *h = (block_header*)((char*)ptr - HEADER_SIZE);
    if (h->magic != FDS_MAGIC) {
        fprintf(stderr, "[FDS ERROR] Corrupted header or invalid pointer %p!\n", ptr);
        return NULL;
    }
    return h;
}

static inline void *ptr_from_header(block_header *h) {
    return (void*)((char*)h + HEADER_SIZE);
}

static block_header *raw_alloc_block(fds_allocator *owner, size_t size) {
    if (size > SIZE_MAX - HEADER_SIZE) return NULL;

    block_header *h = (block_header*)malloc(HEADER_SIZE + size);
    if (!h) return NULL;

    h->magic = FDS_MAGIC;
    h->owner = owner;
    h->size = size;
    h->next = NULL;
    h->prev = NULL;
    h->is_tmp = 0;
    h->is_permanent = 0;
#ifdef DEBUG_MEM
    h->file = NULL;
    h->line = 0;
#endif
    return h;
}

static void raw_free_block(block_header *h) {
    if (!h) return;
    h->magic = 0;
    h->owner = NULL;
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
    } else if (*head == target) {
        *head = target->next;
    }
    if (target->next) {
        target->next->prev = target->prev;
    }
    target->prev = NULL;
    target->next = NULL;
}

void fds_allocator_clear_tmp(fds_allocator *a) {
    if (!a) a = fds_allocator_current();
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
static inline void update_peak(fds_allocator *a) {
    if (a->stats_current_allocated > a->stats_peak_allocated) {
        a->stats_peak_allocated = a->stats_current_allocated;
    }
}
#endif

static void *alloc_internal(fds_allocator *a, size_t size, const char *file, int line, int is_permanent) {
    if (!a) a = fds_allocator_current();
    if (!a) return NULL;

    block_header *h = raw_alloc_block(a, size);
    if (!h) return NULL;

    h->is_tmp = 0;
    h->is_permanent = is_permanent ? 1 : 0;
    list_push((block_header**)&a->all_blocks, h);
    a->live_blocks_count++;

#ifdef DEBUG_MEM
    h->file = file;
    h->line = line;
    if (is_permanent) a->stats_permanent_count++;
    a->stats_alloc_count++;
    a->stats_current_allocated += size;
    a->stats_total_allocated += size;
    update_peak(a);
#else
    (void)file; (void)line;
#endif

    return ptr_from_header(h);
}

void *fds_alloc_impl(fds_allocator *a, size_t size) {
    return alloc_internal(a, size, NULL, 0, 0);
}

void *fds_calloc_impl(fds_allocator *a, size_t num, size_t size) {
    if (num != 0 && size > SIZE_MAX / num) return NULL;
    size_t total = num * size;
    void *ptr = alloc_internal(a, total, NULL, 0, 0);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *fds_realloc_impl(fds_allocator *a, void *ptr, size_t new_size) {
    if (ptr == NULL) return alloc_internal(a, new_size, NULL, 0, 0);
    if (new_size == 0) {
        fds_free_impl(a, ptr);
        return NULL;
    }

    block_header *h = header_from_ptr(ptr);
    if (!h) return NULL;

    // Використовуємо ДІЙСНОГО власника, який створював блок
    fds_allocator *owner = h->owner ? h->owner : (a ? a : fds_allocator_current());
    if (!owner) return NULL;

    if (h->is_tmp) {
        list_remove((block_header**)&owner->tmp_active, h);
    } else {
        list_remove((block_header**)&owner->all_blocks, h);
    }

    block_header *new_h = (block_header*)realloc(h, HEADER_SIZE + new_size);
    if (!new_h) {
        if (h->is_tmp) {
            list_push((block_header**)&owner->tmp_active, h);
        } else {
            list_push((block_header**)&owner->all_blocks, h);
        }
        return NULL;
    }

    new_h->size = new_size;
    new_h->owner = owner;

    if (new_h->is_tmp) {
        list_push((block_header**)&owner->tmp_active, new_h);
    } else {
        list_push((block_header**)&owner->all_blocks, new_h);
    }
    return ptr_from_header(new_h);
}

void fds_free_impl(fds_allocator *a, void *ptr) {
    if (!ptr) return;

    block_header *h = header_from_ptr(ptr);
    if (!h) return;

    // Автоматичний захист: шукаємо власника через заголовок
    fds_allocator *owner = h->owner ? h->owner : (a ? a : fds_allocator_current());
    if (!owner) return;

    if (h->is_tmp) {
        list_remove((block_header**)&owner->tmp_active, h);
    } else {
        list_remove((block_header**)&owner->all_blocks, h);
        if (owner->live_blocks_count > 0) owner->live_blocks_count--;
#ifdef DEBUG_MEM
        owner->stats_free_count++;
        if (owner->stats_current_allocated >= h->size) {
            owner->stats_current_allocated -= h->size;
        } else {
            owner->stats_current_allocated = 0;
        }
        owner->stats_total_freed += h->size;
#endif
    }
    raw_free_block(h);
}

void *fds_alloc_tmp_impl(fds_allocator *a, size_t size) {
    if (!a) a = fds_allocator_current();
    if (!a) return NULL;

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
        h = raw_alloc_block(a, size);
        if (!h) return NULL;
    } else {
        h->size = size;
        h->owner = a;
    }

    h->is_tmp = 1;
    list_push((block_header**)&a->tmp_active, h);
    return ptr_from_header(h);
}

void *fds_alloc_permanent_impl(fds_allocator *a, size_t size) {
    return alloc_internal(a, size, NULL, 0, 1);
}

#ifdef DEBUG_MEM
void *fds_alloc_impl_tracked(fds_allocator *a, size_t size, const char *file, int line) {
    return alloc_internal(a, size, file, line, 0);
}

void *fds_calloc_impl_tracked(fds_allocator *a, size_t num, size_t size, const char *file, int line) {
    if (num != 0 && size > SIZE_MAX / num) return NULL;
    size_t total = num * size;
    void *ptr = alloc_internal(a, total, file, line, 0);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *fds_realloc_impl_tracked(fds_allocator *a, void *ptr, size_t new_size, const char *file, int line) {
    if (ptr == NULL) return alloc_internal(a, new_size, file, line, 0);
    if (new_size == 0) {
        fds_free_impl(a, ptr);
        return NULL;
    }

    block_header *h = header_from_ptr(ptr);
    if (!h) return NULL;

    fds_allocator *owner = h->owner ? h->owner : (a ? a : fds_allocator_current());
    if (!owner) return NULL;

    size_t old_size = h->size;
    if (h->is_tmp) {
        list_remove((block_header**)&owner->tmp_active, h);
    } else {
        list_remove((block_header**)&owner->all_blocks, h);
    }

    block_header *new_h = (block_header*)realloc(h, HEADER_SIZE + new_size);
    if (!new_h) {
        if (h->is_tmp) {
            list_push((block_header**)&owner->tmp_active, h);
        } else {
            list_push((block_header**)&owner->all_blocks, h);
        }
        return NULL;
    }

    new_h->size = new_size;
    new_h->owner = owner;
    new_h->file = file;
    new_h->line = line;

    if (new_h->is_tmp) {
        list_push((block_header**)&owner->tmp_active, new_h);
    } else {
        list_push((block_header**)&owner->all_blocks, new_h);
        owner->stats_realloc_count++;
        if (new_size > old_size) {
            owner->stats_current_allocated += (new_size - old_size);
            owner->stats_total_allocated += (new_size - old_size);
        } else {
            if (owner->stats_current_allocated >= (old_size - new_size)) {
                owner->stats_current_allocated -= (old_size - new_size);
            } else {
                owner->stats_current_allocated = 0;
            }
            owner->stats_total_freed += (old_size - new_size);
        }
        update_peak(owner);
    }

    return ptr_from_header(new_h);
}

void *fds_alloc_tmp_impl_tracked(fds_allocator *a, size_t size, const char *file, int line) {
    if (!a) a = fds_allocator_current();
    if (!a) return NULL;

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
        h = raw_alloc_block(a, size);
        if (!h) return NULL;
        new_block_created = 1;
    } else {
        h->size = size;
        h->owner = a;
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

void *fds_alloc_permanent_impl_tracked(fds_allocator *a, size_t size, const char *file, int line) {
    return alloc_internal(a, size, file, line, 1);
}
#endif

fds_allocator *fds_allocator_create(void) {
    fds_allocator *a = (fds_allocator*)calloc(1, sizeof(fds_allocator));
    if (!a) return NULL;

    a->alloc_fn = fds_alloc_impl;
    a->calloc_fn = fds_calloc_impl;
    a->realloc_fn = fds_realloc_impl;
    a->free_fn = fds_free_impl;
    a->alloc_tmp_fn = fds_alloc_tmp_impl;
    a->alloc_permanent_fn = fds_alloc_permanent_impl;

    return a;
}

void fds_allocator_destroy(fds_allocator *a) {
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

#define FDS_MAX_ALLOC_STACK 16

static _Thread_local fds_allocator *tls_current_allocator = NULL;
static _Thread_local fds_allocator *tls_alloc_stack[FDS_MAX_ALLOC_STACK];
static _Thread_local int tls_alloc_stack_depth = 0;

fds_allocator *fds_allocator_current(void) {
    if (tls_current_allocator == NULL) {
        static _Thread_local fds_allocator *default_allocator = NULL;
        if (default_allocator == NULL) {
            default_allocator = fds_allocator_create();
        }
        return default_allocator;
    }
    return tls_current_allocator;
}

void fds_allocator_push(fds_allocator *a) {
    if (tls_alloc_stack_depth < FDS_MAX_ALLOC_STACK) {
        tls_alloc_stack[tls_alloc_stack_depth++] = tls_current_allocator;
    }
    tls_current_allocator = a;
}

void fds_allocator_pop(void) {
    if (tls_alloc_stack_depth > 0) {
        tls_current_allocator = tls_alloc_stack[--tls_alloc_stack_depth];
    } else {
        tls_current_allocator = NULL;
    }
}

size_t fds_allocator_live_blocks_count(fds_allocator *a) {
    if (!a) a = fds_allocator_current();
    return a ? a->live_blocks_count : 0;
}

#ifdef DEBUG_MEM
void fds_allocator_get_stats(fds_allocator *a, fds_allocator_stats *out_stats) {
    if (!a) a = fds_allocator_current();
    if (!a || !out_stats) return;

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

void fds_allocator_print_stats(fds_allocator *a) {
    if (!a) a = fds_allocator_current();
    if (!a) return;
    fds_allocator_stats s;
    fds_allocator_get_stats(a, &s);

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

#endif // FDS_ALLOCATOR_IMPLEMENTATION