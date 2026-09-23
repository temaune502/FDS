// #ifndef FDS_FIBER_H
// #define FDS_FIBER_H

// #include <stddef.h>
// #include <stdint.h>
// #include <stdbool.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

// typedef void (*fds_fiber_fn)(void* arg);

// typedef struct fds_fiber {
//     void* rsp;               // Поточний вказівник стеку (ОБОВ'ЯЗКОВО ПЕРШЕ ПОЛЕ для offset 0 в ASM)
//     void* stack_base;        // Базовий вказівник на пам'ять стеку
//     size_t stack_size;       // Розмір стеку у байтах
//     bool is_completed;       // Прапорець завершення виконання
//     fds_fiber_fn entry_fn;   // Функція файбера
//     void* arg;               // Аргумент функції
//     struct fds_fiber* sched; // Файбер, куди повернутися після завершення (планувальник)
// } fds_fiber;

// // Ініціалізація головного потоку як основного файбера
// void fds_fiber_init_main(fds_fiber* main_fiber);

// // Створення нового файбера з пам'яті (наприклад, виділеної через fds_arena)
// fds_fiber fds_fiber_create(void* stack_mem, size_t stack_size, fds_fiber_fn fn, void* arg, fds_fiber* sched_fiber);

// // Низькорівневе перемикання контексту між файберами
// void fds_fiber_switch(fds_fiber* from, fds_fiber* to);

// #ifdef __cplusplus
// }
// #endif

// #endif // FDS_FIBER_H

// // ============================================================================
// // РЕАЛІЗАЦІЯ (FDS_FIBER_IMPLEMENTATION)
// // ============================================================================
// #ifdef FDS_FIBER_IMPLEMENTATION

// // Кадр збережених регістрів на стеку для x86_64 (Об'єднання Win64 та System V ABI)
// typedef struct fds_fiber_frame {
//     uint64_t r15;
//     uint64_t r14;
//     uint64_t r13;
//     uint64_t r12;
//     uint64_t rsi;
//     uint64_t rdi;
//     uint64_t rbp;
//     uint64_t rbx;
//     uint64_t rip; // Адреса повернення (точка входу трампліна)
// } fds_fiber_frame;

// // Поточний активний файбер для обробки трампліна
// static _Thread_local fds_fiber* g_fds_current_fiber = NULL;

// static void fds_fiber_trampoline(void) {
//     fds_fiber* cur = g_fds_current_fiber;
//     if (cur && cur->entry_fn) {
//         cur->entry_fn(cur->arg);
//     }
    
//     if (cur) {
//         cur->is_completed = true;
//         if (cur->sched) {
//             fds_fiber_switch(cur, cur->sched);
//         }
//     }
// }

// void fds_fiber_init_main(fds_fiber* main_fiber) {
//     main_fiber->rsp = NULL;
//     main_fiber->stack_base = NULL;
//     main_fiber->stack_size = 0;
//     main_fiber->is_completed = false;
//     main_fiber->entry_fn = NULL;
//     main_fiber->arg = NULL;
//     main_fiber->sched = NULL;
//     g_fds_current_fiber = main_fiber;
// }

// fds_fiber fds_fiber_create(void* stack_mem, size_t stack_size, fds_fiber_fn fn, void* arg, fds_fiber* sched_fiber) {
//     fds_fiber f = {0};
//     f.stack_base = stack_mem;
//     f.stack_size = stack_size;
//     f.entry_fn = fn;
//     f.arg = arg;
//     f.sched = sched_fiber;
//     f.is_completed = false;

//     // Стек росте донизу: знаходимо вершину пам'яті
//     uintptr_t stack_top = (uintptr_t)stack_mem + stack_size;

//     // 16-байтове вирівнювання стеку під вимоги x86_64 ABI
//     stack_top &= ~15ULL;

//     // Виділяємо місце під початковий кадр регістрів
//     stack_top -= sizeof(fds_fiber_frame);

//     // Залишаємо додаткові 8 байт для коректного вирівнювання після ret / call
//     stack_top -= 8;

//     fds_fiber_frame* frame = (fds_fiber_frame*)stack_top;
//     frame->r15 = 0;
//     frame->r14 = 0;
//     frame->r13 = 0;
//     frame->r12 = 0;
//     frame->rsi = 0;
//     frame->rdi = 0;
//     frame->rbp = 0;
//     frame->rbx = 0;
//     frame->rip = (uint64_t)fds_fiber_trampoline;

//     f.rsp = (void*)frame;
//     return f;
// }

// __attribute__((noinline))
// void fds_fiber_switch(fds_fiber* from, fds_fiber* to) {
//     g_fds_current_fiber = to;

// #if defined(__x86_64__) || defined(_M_X64)
//     __asm__ __volatile__(
//         // 1. Зберігаємо Callee-Saved регістри файбера "from" на його стек
//         "pushq %%rbx\n\t"
//         "pushq %%rbp\n\t"
//         "pushq %%rdi\n\t"
//         "pushq %%rsi\n\t"
//         "pushq %%r12\n\t"
//         "pushq %%r13\n\t"
//         "pushq %%r14\n\t"
//         "pushq %%r15\n\t"

//         // 2. Зберігаємо поточний RSP у from->rsp (поле на зміщенні 0)
//         "movq %%rsp, (%0)\n\t"

//         // 3. Завантажуємо RSP файбера "to" з to->rsp
//         "movq (%1), %%rsp\n\t"

//         // 4. Відновлюємо Callee-Saved регістри файбера "to"
//         "popq %%r15\n\t"
//         "popq %%r14\n\t"
//         "popq %%r13\n\t"
//         "popq %%r12\n\t"
//         "popq %%rsi\n\t"
//         "popq %%rdi\n\t"
//         "popq %%rbp\n\t"
//         "popq %%rbx\n\t"

//         // 5. Перехід за адресою на вершині стеку (ret робить pop rip)
//         "ret\n\t"
//         :
//         : "r"(from), "r"(to)
//         : "memory"
//     );
// #else
//     #error "fds_fiber підтримує лише архітектуру x86_64!"
// #endif
// }

// #endif // FDS_FIBER_IMPLEMENTATION


#ifndef FDS_FIBER_H
#define FDS_FIBER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDS_FIBER_MAX_QUEUE
#define FDS_FIBER_MAX_QUEUE 256
#endif

typedef void (*fds_fiber_fn)(void* arg);

typedef struct fds_fiber {
    void* rsp;               // Поточний вказівник стеку (ОБОВ'ЯЗКОВО ПЕРШЕ ПОЛЕ для offset 0 в ASM)
    void* stack_base;        // Пам'ять стеку
    size_t stack_size;       // Розмір стеку
    bool is_completed;       // Прапорець завершення
    fds_fiber_fn entry_fn;   // Точка входу
    void* arg;               // Аргумент
    struct fds_fiber* sched; // Файбер планувальника
} fds_fiber;

// Структура кооперативного Round-Robin планувальника
typedef struct fds_fiber_sched {
    fds_fiber main_fiber;                     // Контекст головного циклу планувальника
    fds_fiber* queue[FDS_FIBER_MAX_QUEUE];   // Кільцева черга готовності
    size_t head;
    size_t tail;
    size_t count;
    fds_fiber* current;                      // Поточний активний файбер
} fds_fiber_sched;

// --- ОСНОВНІ ФУНКЦІЇ FIBER ---
void fds_fiber_init_main(fds_fiber* main_fiber);
fds_fiber fds_fiber_create(void* stack_mem, size_t stack_size, fds_fiber_fn fn, void* arg, fds_fiber* sched_fiber);
void fds_fiber_switch(fds_fiber* from, fds_fiber* to);

// --- ФУНКЦІЇ ПЛАНУВАЛЬНИКА ---
void fds_fiber_sched_init(fds_fiber_sched* sched);
bool fds_fiber_sched_add(fds_fiber_sched* sched, fds_fiber* fiber);
void fds_fiber_yield(fds_fiber_sched* sched);
void fds_fiber_sched_run(fds_fiber_sched* sched);

#ifdef __cplusplus
}
#endif

#endif // FDS_FIBER_H

// ============================================================================
// РЕАЛІЗАЦІЯ (FDS_FIBER_IMPLEMENTATION)
// ============================================================================
#ifdef FDS_FIBER_IMPLEMENTATION

typedef struct fds_fiber_frame {
    uint64_t r15, r14, r13, r12;
    uint64_t rsi, rdi, rbp, rbx;
    uint64_t rip;
} fds_fiber_frame;

static _Thread_local fds_fiber* g_fds_current_fiber = NULL;

static void fds_fiber_trampoline(void) {
    fds_fiber* cur = g_fds_current_fiber;
    if (cur && cur->entry_fn) {
        cur->entry_fn(cur->arg);
    }
    
    if (cur) {
        cur->is_completed = true;
        if (cur->sched) {
            fds_fiber_switch(cur, cur->sched);
        }
    }
}

void fds_fiber_init_main(fds_fiber* main_fiber) {
    main_fiber->rsp = NULL;
    main_fiber->stack_base = NULL;
    main_fiber->stack_size = 0;
    main_fiber->is_completed = false;
    main_fiber->entry_fn = NULL;
    main_fiber->arg = NULL;
    main_fiber->sched = NULL;
    g_fds_current_fiber = main_fiber;
}

fds_fiber fds_fiber_create(void* stack_mem, size_t stack_size, fds_fiber_fn fn, void* arg, fds_fiber* sched_fiber) {
    fds_fiber f = {0};
    f.stack_base = stack_mem;
    f.stack_size = stack_size;
    f.entry_fn = fn;
    f.arg = arg;
    f.sched = sched_fiber;
    f.is_completed = false;

    uintptr_t stack_top = (uintptr_t)stack_mem + stack_size;
    stack_top &= ~15ULL;
    stack_top -= sizeof(fds_fiber_frame);
    stack_top -= 8; // Вирівнювання кадру

    fds_fiber_frame* frame = (fds_fiber_frame*)stack_top;
    frame->r15 = 0; frame->r14 = 0; frame->r13 = 0; frame->r12 = 0;
    frame->rsi = 0; frame->rdi = 0; frame->rbp = 0; frame->rbx = 0;
    frame->rip = (uint64_t)fds_fiber_trampoline;

    f.rsp = (void*)frame;
    return f;
}

__attribute__((noinline))
void fds_fiber_switch(fds_fiber* from, fds_fiber* to) {
    g_fds_current_fiber = to;

#if defined(__x86_64__) || defined(_M_X64)
    __asm__ __volatile__(
        "pushq %%rbx\n\t"
        "pushq %%rbp\n\t"
        "pushq %%rdi\n\t"
        "pushq %%rsi\n\t"
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"

        "movq %%rsp, (%0)\n\t"
        "movq (%1), %%rsp\n\t"

        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%rsi\n\t"
        "popq %%rdi\n\t"
        "popq %%rbp\n\t"
        "popq %%rbx\n\t"

        "ret\n\t"
        :
        : "r"(from), "r"(to)
        : "memory"
    );
#else
    #error "fds_fiber підтримує лише x86_64"
#endif
}

// --- РЕАЛІЗАЦІЯ ПЛАНУВАЛЬНИКА ---

void fds_fiber_sched_init(fds_fiber_sched* sched) {
    sched->head = 0;
    sched->tail = 0;
    sched->count = 0;
    sched->current = NULL;
    fds_fiber_init_main(&sched->main_fiber);
}

bool fds_fiber_sched_add(fds_fiber_sched* sched, fds_fiber* fiber) {
    if (sched->count >= FDS_FIBER_MAX_QUEUE) return false;

    // Вказуємо, що при завершенні файбер повинен повернутися до планувальника
    fiber->sched = &sched->main_fiber;

    sched->queue[sched->tail] = fiber;
    sched->tail = (sched->tail + 1) % FDS_FIBER_MAX_QUEUE;
    sched->count++;
    return true;
}

void fds_fiber_yield(fds_fiber_sched* sched) {
    if (!sched || !sched->current) return;

    fds_fiber* yielding_fiber = sched->current;

    // Якщо файбер ще не завершений — повертаємо його в кінець черги
    if (!yielding_fiber->is_completed) {
        fds_fiber_sched_add(sched, yielding_fiber);
    }

    // Переключаємося назад у головний цикл планувальника
    fds_fiber_switch(yielding_fiber, &sched->main_fiber);
}

void fds_fiber_sched_run(fds_fiber_sched* sched) {
    while (sched->count > 0) {
        // Витягуємо наступний файбер з голови черги
        fds_fiber* next = sched->queue[sched->head];
        sched->head = (sched->head + 1) % FDS_FIBER_MAX_QUEUE;
        sched->count--;

        if (next->is_completed) continue;

        sched->current = next;
        
        // Передаємо виконання файберу від імені планувальника
        fds_fiber_switch(&sched->main_fiber, next);
    }
    
    sched->current = NULL;
}

#endif // FDS_FIBER_IMPLEMENTATION