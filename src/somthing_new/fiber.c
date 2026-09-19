// #define FDS_FIBER_IMPLEMENTATION
// #include "fds_fiber.h"
// #include <stdio.h>
// #include <stdlib.h>

// fds_fiber main_fiber;
// fds_fiber worker_fiber;

// void worker_func(void* arg) {
//     const char* name = (const char*)arg;
//     printf("[%s] Step 1: Start fiber\n", name);
    
//     // Повертаємо керування в головний процес
//     fds_fiber_switch(&worker_fiber, &main_fiber);
    
//     printf("[%s] Step 2: Resume excutino\n", name);
// }

// int main(void) {
//     fds_fiber_init_main(&main_fiber);

//     // Виділення стеку (наприклад, 64 Кб)
//     size_t stack_size = 64 * 1024;
//     void* stack_mem = malloc(stack_size);

//     worker_fiber = fds_fiber_create(stack_mem, stack_size, worker_func, "Worker-1", &main_fiber);

//     printf("[Main] Change to Worker\n");
//     fds_fiber_switch(&main_fiber, &worker_fiber);

//     printf("[Main] Return in Main, do additional job...\n");

//     printf("[Main] Go to Worker\n");
//     fds_fiber_switch(&main_fiber, &worker_fiber);

//     printf("[Main] End! Worker End: %s\n", worker_fiber.is_completed ? "yes" : "no");

//     free(stack_mem);
//     return 0;
// }



#define FDS_FIBER_IMPLEMENTATION
#include "fds_fiber.h"
#include <stdio.h>
#include <stdlib.h>

fds_fiber_sched g_sched;

void task_worker(void* arg) {
    int id = (int)(uintptr_t)arg;
    
    for (int step = 1; step <= 3; ++step) {
        printf("  [Task %d] Крок %d/3\n", id, step);
        
        // Віддаємо квант часу іншим корутинам у черзі
        fds_fiber_yield(&g_sched);
    }
    
    printf("  [Task %d] Завершено!\n", id);
}

int main(void) {
    fds_fiber_sched_init(&g_sched);

    const int FIBER_COUNT = 3;
    const size_t STACK_SIZE = 32 * 1024;

    fds_fiber fibers[FIBER_COUNT];

    printf("[Main] Додаємо %d файбери до планувальника...\n", FIBER_COUNT);

    for (int i = 0; i < FIBER_COUNT; ++i) {
        void* stack = malloc(STACK_SIZE);
        fibers[i] = fds_fiber_create(stack, STACK_SIZE, task_worker, (void*)(uintptr_t)(i + 1), NULL);
        fds_fiber_sched_add(&g_sched, &fibers[i]);
    }

    printf("[Main] Запуск планувальника (Round-Robin)...\n\n");
    fds_fiber_sched_run(&g_sched);

    printf("\n[Main] Усі файбери завершили роботу!\n");

    for (int i = 0; i < FIBER_COUNT; ++i) {
        free(fibers[i].stack_base);
    }

    return 0;
}