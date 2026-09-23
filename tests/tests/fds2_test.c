#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#define DEBUG_MEM

#define FDS2_ALLOCATOR_IMPLEMENTATION
#include "fds2.h"

#define NUM_BLOCKS 20000
#define NUM_THREADS 8
#define THREAD_OPS 10000

typedef struct {
    uint8_t *ptr;
    size_t size;
    uint8_t pattern;
} alloc_slot_t;

// Допоміжна функція генерації псевдовипадкових чисел
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

// 1. Стрес-тест фрагментації та цілісності даних
void test_heavy_churn() {
    printf("[1/4] Running Heavy Churn & Integrity Test (%d ops)...\n", NUM_BLOCKS);
    // fds2_allocator *a = fds2_allocator_create();
    // fds2_allocator_push(a);

    alloc_slot_t *slots = (alloc_slot_t*)calloc(NUM_BLOCKS, sizeof(alloc_slot_t));
    uint32_t rng = 123456789;

    for (int i = 0; i < NUM_BLOCKS; i++) {
        int index = xorshift32(&rng) % NUM_BLOCKS;

        if (slots[index].ptr != NULL) {
            // Перевіряємо, чи не пошкодилися дані перед звільненням
            for (size_t k = 0; k < slots[index].size; k++) {
                assert(slots[index].ptr[k] == slots[index].pattern);
            }
            fds2_free(slots[index].ptr);
            slots[index].ptr = NULL;
        } else {
            size_t sz = (xorshift32(&rng) % 4096) + 1;
            uint8_t pattern = (uint8_t)(xorshift32(&rng) & 0xFF);
            
            uint8_t *p = (uint8_t*)fds2_alloc(sz);
            assert(p != NULL);
            assert(((uintptr_t)p % 16) == 0); // Перевірка 16-байтового вирівнювання

            memset(p, pattern, sz);
            slots[index].ptr = p;
            slots[index].size = sz;
            slots[index].pattern = pattern;
        }
    }

    // Звільняємо всі залишки
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (slots[i].ptr) {
            for (size_t k = 0; k < slots[i].size; k++) {
                assert(slots[i].ptr[k] == slots[i].pattern);
            }
            fds2_free(slots[i].ptr);
        }
    }

    assert(fds2_allocator_live_blocks_count(fds2_allocator_current()) == 0);
    free(slots);
    // fds2_allocator_pop();
    // fds2_allocator_destroy(a);
    printf("     OK!\n");
}

// 2. Стрес-тест Realloc з розширенням і звуженням
void test_realloc_torture() {
    printf("[2/4] Running Realloc Torture Test...\n");
    // fds2_allocator *a = fds2_allocator_create();
    
    size_t sz = 8;
    uint8_t *ptr = (uint8_t*)fds2_alloc(sz);
    for (size_t i = 0; i < sz; i++) ptr[i] = (uint8_t)(i & 0xFF);

    // Розширюємо та звужуємо 500 разів
    for (int step = 0; step < 150; step++) {
        size_t new_sz = (step % 2 == 0) ? (sz * 2) : (sz / 2 + 1);
        if (new_sz == 0) new_sz = 1;

        ptr = (uint8_t*)fds2_realloc(ptr, new_sz);
        assert(ptr != NULL);

        // Перевіряємо збереження старих даних
        size_t min_sz = sz < new_sz ? sz : new_sz;
        for (size_t i = 0; i < min_sz; i++) {
            assert(ptr[i] == (uint8_t)(i & 0xFF));
        }

        // Заповнюємо нові дані
        for (size_t i = min_sz; i < new_sz; i++) {
            ptr[i] = (uint8_t)(i & 0xFF);
        }
        sz = new_sz;
    }

    fds2_free(ptr);
    // fds2_allocator_destroy(a);
    printf("     OK!\n");
}

// 3. Робоче навантаження для окремого потоку
void *thread_worker(void *arg) {
    int thread_id = *(int*)arg;
    uint32_t rng = 987654321 + thread_id * 1337;

    fds2_allocator *thread_alloc = fds2_allocator_create();
    fds2_allocator_push(thread_alloc);

    void **ptrs = (void**)calloc(1000, sizeof(void*));

    for (int i = 0; i < THREAD_OPS; i++) {
        int idx = xorshift32(&rng) % 1000;
        if (ptrs[idx]) {
            fds2_free(ptrs[idx]);
            ptrs[idx] = NULL;
        } else {
            size_t size = (xorshift32(&rng) % 2048) + 1;
            if (i % 5 == 0) {
                ptrs[idx] = fds2_alloc_tmp(size);
            } else {
                ptrs[idx] = fds2_alloc(size);
            }
            assert(ptrs[idx] != NULL);
        }
    }

    for (int i = 0; i < 1000; i++) {
        if (ptrs[i]) fds2_free(ptrs[i]);
    }

    free(ptrs);

    fds2_allocator_print_stats(NULL);

    fds2_allocator_pop();
    fds2_allocator_destroy(thread_alloc);
    return NULL;
}

// 4. Багатопотоковий стрес-тест
void test_multithreaded_stress() {
    printf("[3/4] Running Multithreaded Isolation Test (%d threads)...\n", NUM_THREADS);
    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        assert(pthread_create(&threads[i], NULL, thread_worker, &tids[i]) == 0);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("     OK!\n");
}

// 5. Перевірка роботи макросів із трекінгом витоків
void test_macro_leak_check() {
    printf("[4/4] Running Macro & Tracking Leak Sanity Check...\n");


    // Виділення без явного вказування алокатора (використовує push)

    void *p1 = fds2_alloc(100);
    void *p2 = fds2_calloc(10, 20);
    void *p3 = fds2_alloc_permanent(300);

    (void)p1; (void)p2; (void)p3;

    fds2_free(p1); // Звільняємо p1, а p2 "забуваємо" для демонстрації leak report
    
    fds2_allocator_pop();

#ifdef DEBUG_MEM
    printf("\n--- EXPECTED LEAK REPORT (1 intentional leak of 200 bytes) ---\n");
    fds2_allocator_destroy(fds2_allocator_current()); // Має відрапортувати про p2
    printf("--------------------------------------------------------------\n");
#else
    fds2_allocator_destroy(fds2_allocator_current());
#endif
}

int main() {
    printf("==================================================\n");
    printf("      FDS2 Allocator Heavy Stress Test Suite      \n");
    printf("==================================================\n\n");

    test_heavy_churn();
    test_realloc_torture();
    test_multithreaded_stress();
    test_macro_leak_check();
	
    fds2_allocator_print_stats(NULL);	

    printf("\nALL STRESS TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}