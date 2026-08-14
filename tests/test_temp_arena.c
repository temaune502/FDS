// stress_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdarg.h>


#define FDS_IMPLEMENTATION
#include "fds.h"

// ──── Допоміжні функції для багатопотокових тестів ────────────────
// Перевірка вирівнювання
static int is_aligned(const void *ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}
// ──── Однопотокові стрес-тести ────────────────────────────────────
void test_massive_allocations(void) {
    printf("  Single-thread: massive allocations ... ");
    FixedArena arena = fixed_arena_create(10 * MB);

    // Виділяємо 10 000 маленьких об'єктів
    for (int i = 0; i < 10000; i++) {
        int *p = fixed_arena_new(&arena, int);
        *p = i;
        assert(*p == i);
    }

    // Виділяємо 1000 блоків по 1 КБ
    for (int i = 0; i < 1000; i++) {
        char *buf = fixed_arena_alloc(&arena, KB);
        memset(buf, i & 0xFF, KB);
    }

    // Перевіряємо, що пам'ять не закінчилась
    assert(fixed_arena_available(&arena) > 0);
    fixed_arena_free(&arena);
    printf("OK\n");
}

void test_mark_restore_cycle(void) {
    printf("  Single-thread: mark/restore cycles ... ");
    FixedArena arena = fixed_arena_create(1 * MB);

    for (int cycle = 0; cycle < 1000; cycle++) {
        FixedArenaMark m = fixed_arena_mark(&arena);
        // Виділяємо випадкову кількість об'єктів
        int count = rand() % 100 + 1;
        for (int i = 0; i < count; i++) {
            double *d = fixed_arena_new(&arena, double);
            *d = (double)cycle / (i + 1);
        }
        // Відкочуємо
        fixed_arena_restore(&arena, m);
        assert(fixed_arena_used(&arena) == m); // Пам'ять повернулась до мітки
    }

    fixed_arena_free(&arena);
    printf("OK\n");
}

void test_temp_scope_stress(void) {
    printf("  Single-thread: TEMP_SCOPE stress ... ");
    // Цей тест використовує глобальну TempArena
    for (int i = 0; i < 5000; i++) {
        TEMP_SCOPE();
        char *str = TEMP_SPRINTF("Iteration %d, random %d", i, rand());
        assert(str != NULL);
        // Можна робити ще виділення
        int *arr = TEMP_ARRAY(int, 50);
        for (int j = 0; j < 50; j++) arr[j] = j;
        // Все буде відкочено автоматично
    }
    temp_arena_reset(); // Очистимо арену повністю
    printf("OK\n");
}

void test_alignment_stress(void) {
    printf("  Single-thread: alignment stress ... ");
    FixedArena arena = fixed_arena_create(4 * KB);

    // Різні типи з різним вирівнюванням
    char *c = fixed_arena_new(&arena, char);
    assert(is_aligned(c, alignof(char)));
    short *s = fixed_arena_new(&arena, short);
    assert(is_aligned(s, alignof(short)));
    int *i = fixed_arena_new(&arena, int);
    assert(is_aligned(i, alignof(int)));
    long *l = fixed_arena_new(&arena, long);
    assert(is_aligned(l, alignof(long)));
    float *f = fixed_arena_new(&arena, float);
    assert(is_aligned(f, alignof(float)));
    double *d = fixed_arena_new(&arena, double);
    assert(is_aligned(d, alignof(double)));
    void **vp = fixed_arena_new(&arena, void*);
    assert(is_aligned(vp, alignof(void*)));

    // Явне велике вирівнювання
    void *big = fixed_arena_alloc_align(&arena, 13, 64);
    assert(is_aligned(big, 64));

    fixed_arena_free(&arena);
    printf("OK\n");
}


// Дані, що передаються в потік
typedef struct {
    int thread_id;
    int num_operations;
} thread_data_t;

// Функція потоку, що працює з локальною FixedArena
void *thread_func_local_arena(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    // Кожен потік має власну арену
    FixedArena arena = fixed_arena_create(128 * KB);

    for (int i = 0; i < data->num_operations; i++) {
        // Виділяємо масив цілих
        int *arr = fixed_arena_array(&arena, int, 64);
        for (int j = 0; j < 64; j++) {
            arr[j] = data->thread_id * 1000 + j;
        }
        // Перевіряємо записані дані
        for (int j = 0; j < 64; j++) {
            assert(arr[j] == data->thread_id * 1000 + j);
        }
        // Кожні 100 ітерацій скидаємо арену
        if (i % 100 == 0) {
            fixed_arena_reset(&arena);
        }
    }

    fixed_arena_free(&arena);
    return NULL;
}

// Функція потоку, що використовує TempArena (thread-local)
void *thread_func_temp_arena(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    (void)data;

    for (int i = 0; i < data->num_operations; i++) {
        TEMP_SCOPE();
        // Форматуємо рядок з ID потоку
        char *msg = TEMP_SPRINTF("Thread %d, iter %d", data->thread_id, i);
        // Просто перевіряємо, що рядок не NULL
        assert(msg != NULL);
        // Виділяємо трохи пам'яті
        double *nums = TEMP_ARRAY(double, 32);
        for (int j = 0; j < 32; j++) {
            nums[j] = (double)(data->thread_id * i + j);
        }
        // Перевірка
        double sum = 0;
        for (int j = 0; j < 32; j++) sum += nums[j];
        assert(sum > 0 || sum == 0); // просто використання
    }
    // В кінці скидаємо thread-local арену
    temp_arena_reset();
    return NULL;
}

// ──── Багатопотокові тести ────────────────────────────────────────
void test_multithread_local_arenas(void) {
    printf("  Multi-thread: local FixedArena per thread ... ");
    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 5000;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = OPS_PER_THREAD;
        pthread_create(&threads[i], NULL, thread_func_local_arena, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("OK\n");
}

void test_multithread_temp_arena(void) {
    printf("  Multi-thread: TempArena (thread-local) ... ");
    const int NUM_THREADS = 12;
    const int OPS_PER_THREAD = 3000;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = OPS_PER_THREAD;
        pthread_create(&threads[i], NULL, thread_func_temp_arena, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("OK\n");
}

// ──── Головна функція ────────────────────────────────────────────
int main(void) {
    printf("=== Stress tests for FixedArena & TempArena ===\n");

    // Однопотокові
    test_massive_allocations();
    test_mark_restore_cycle();
    test_temp_scope_stress();
    test_alignment_stress();

    // Багатопотокові
    test_multithread_local_arenas();
    test_multithread_temp_arena();

    printf("\nAll stress tests passed successfully!\n");
    temp_arena_destroy();
    return 0;
}