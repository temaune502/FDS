#ifndef FDS_EXT_COROUTINE_H
#define FDS_EXT_COROUTINE_H

#include "fds.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDS_COROUTINE_DEFAULT_STACK_SIZE
#define FDS_COROUTINE_DEFAULT_STACK_SIZE (64u * 1024u)
#endif

#ifndef FDS_COROUTINE_MIN_STACK_SIZE
#define FDS_COROUTINE_MIN_STACK_SIZE (16u * 1024u)
#endif

typedef struct FdsCoroutine FdsCoroutine;
typedef struct FdsCoroutineScheduler FdsCoroutineScheduler;

typedef void (*FdsCoroutineFn)(void *arg);

typedef enum FdsCoroutineState {
    FDS_COROUTINE_NEW = 0,
    FDS_COROUTINE_RUNNING,
    FDS_COROUTINE_SUSPENDED,
    FDS_COROUTINE_DONE
} FdsCoroutineState;

typedef struct FdsCoroutineContext {
    void *sp;
    u32 mxcsr;
    u16 x87_control;
    u16 reserved;
} FdsCoroutineContext;

struct FdsCoroutine {
    FdsCoroutineContext context;
    FdsCoroutineScheduler *scheduler;
    FdsCoroutineFn fn;
    void *arg;
    void *stack;
    usize stack_size;
    FdsCoroutineState state;
};

struct FdsCoroutineScheduler {
    FdsCoroutineContext context;
    FdsCoroutine **items;
    usize count;
    usize capacity;
    FdsCoroutine *current;
    bool running;
};

void fds_coroutine_scheduler_init(FdsCoroutineScheduler *scheduler);
bool fds_coroutine_scheduler_run(FdsCoroutineScheduler *scheduler);
bool fds_coroutine_scheduler_destroy(FdsCoroutineScheduler *scheduler);

FdsCoroutine *fds_coroutine_create(
    FdsCoroutineScheduler *scheduler,
    FdsCoroutineFn fn,
    void *arg,
    usize stack_size);

bool fds_coroutine_destroy(FdsCoroutine *co);
void fds_coroutine_yield(void);
bool fds_coroutine_is_done(const FdsCoroutine *co);

#ifdef __cplusplus
}
#endif

#if defined(FDS_EXT_IMPL) || defined(FDS_COROUTINE_IMPL)

#if !defined(__GNUC__) && !defined(__clang__)
#error "fds_ext_coroutine.h requires GCC or Clang with GNU-style global assembly"
#endif

#if !defined(__x86_64__) && !defined(_M_X64)
#error "fds_ext_coroutine.h currently supports AMD64 / x86-64 only"
#endif

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#if defined(__cplusplus)
#define FDS_CORO_THREAD_LOCAL thread_local
#else
#define FDS_CORO_THREAD_LOCAL _Thread_local
#endif

#if defined(__cplusplus)
static_assert(offsetof(FdsCoroutineContext, sp) == 0, "unexpected coroutine context layout");
static_assert(offsetof(FdsCoroutineContext, mxcsr) == 8, "unexpected coroutine context layout");
static_assert(offsetof(FdsCoroutineContext, x87_control) == 12, "unexpected coroutine context layout");
extern "C" {
#else
_Static_assert(offsetof(FdsCoroutineContext, sp) == 0, "unexpected coroutine context layout");
_Static_assert(offsetof(FdsCoroutineContext, mxcsr) == 8, "unexpected coroutine context layout");
_Static_assert(offsetof(FdsCoroutineContext, x87_control) == 12, "unexpected coroutine context layout");
#endif

static FDS_CORO_THREAD_LOCAL FdsCoroutine *fds__coroutine_current = NULL;

void fds__coroutine_context_switch(FdsCoroutineContext *from, FdsCoroutineContext *to);
void fds__coroutine_bootstrap(void);

#if defined(_WIN32)
__asm__(
    ".text\n"
    ".globl fds__coroutine_context_switch\n"
    "fds__coroutine_context_switch:\n"
    "stmxcsr 8(%rcx)\n"
    "fnstcw 12(%rcx)\n"
    "pushq %rbx\n"
    "pushq %rbp\n"
    "pushq %rdi\n"
    "pushq %rsi\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"
    "subq $160, %rsp\n"
    "movdqu %xmm6, 0(%rsp)\n"
    "movdqu %xmm7, 16(%rsp)\n"
    "movdqu %xmm8, 32(%rsp)\n"
    "movdqu %xmm9, 48(%rsp)\n"
    "movdqu %xmm10, 64(%rsp)\n"
    "movdqu %xmm11, 80(%rsp)\n"
    "movdqu %xmm12, 96(%rsp)\n"
    "movdqu %xmm13, 112(%rsp)\n"
    "movdqu %xmm14, 128(%rsp)\n"
    "movdqu %xmm15, 144(%rsp)\n"
    "movq %rsp, 0(%rcx)\n"
    "movq 0(%rdx), %rsp\n"
    "movdqu 0(%rsp), %xmm6\n"
    "movdqu 16(%rsp), %xmm7\n"
    "movdqu 32(%rsp), %xmm8\n"
    "movdqu 48(%rsp), %xmm9\n"
    "movdqu 64(%rsp), %xmm10\n"
    "movdqu 80(%rsp), %xmm11\n"
    "movdqu 96(%rsp), %xmm12\n"
    "movdqu 112(%rsp), %xmm13\n"
    "movdqu 128(%rsp), %xmm14\n"
    "movdqu 144(%rsp), %xmm15\n"
    "ldmxcsr 8(%rdx)\n"
    "fldcw 12(%rdx)\n"
    "addq $160, %rsp\n"
    "popq %r15\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %rsi\n"
    "popq %rdi\n"
    "popq %rbp\n"
    "popq %rbx\n"
    "ret\n"
    ".globl fds__coroutine_bootstrap\n"
    "fds__coroutine_bootstrap:\n"
    "movq %r12, %rcx\n"
    "subq $40, %rsp\n"
    "call fds__coroutine_entry\n"
    "ud2\n"
);
#else
__asm__(
    ".text\n"
    ".globl fds__coroutine_context_switch\n"
    "fds__coroutine_context_switch:\n"
    "stmxcsr 8(%rdi)\n"
    "fnstcw 12(%rdi)\n"
    "pushq %rbx\n"
    "pushq %rbp\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"
    "movq %rsp, 0(%rdi)\n"
    "movq 0(%rsi), %rsp\n"
    "ldmxcsr 8(%rsi)\n"
    "fldcw 12(%rsi)\n"
    "popq %r15\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %rbp\n"
    "popq %rbx\n"
    "ret\n"
    ".globl fds__coroutine_bootstrap\n"
    "fds__coroutine_bootstrap:\n"
    "movq %r12, %rdi\n"
    "subq $8, %rsp\n"
    "call fds__coroutine_entry\n"
    "ud2\n"
);
#endif

static void fds__coroutine_capture_fp(FdsCoroutineContext *context)
{
    __asm__ volatile("stmxcsr %0" : "=m"(context->mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(context->x87_control));
}

static bool fds__coroutine_scheduler_reserve(FdsCoroutineScheduler *scheduler, usize needed)
{
    if (needed <= scheduler->capacity)
        return true;

    usize new_capacity;
    if (scheduler->capacity == 0) {
        new_capacity = 8;
    } else {
        if (scheduler->capacity > SIZE_MAX / 2)
            return false;
        new_capacity = scheduler->capacity * 2;
    }

    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2)
            return false;
        new_capacity *= 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(*scheduler->items))
        return false;

    FdsCoroutine **items = (FdsCoroutine **)FDS_REALLOC(
        scheduler->items,
        new_capacity * sizeof(*scheduler->items));
    if (!items)
        return false;

    scheduler->items = items;
    scheduler->capacity = new_capacity;
    return true;
}

static bool fds__coroutine_make_context(FdsCoroutine *co)
{
    uintptr_t top = (uintptr_t)co->stack + co->stack_size;
    top &= ~(uintptr_t)15;

#if defined(_WIN32)
    /* XMM6-XMM15 (160 bytes), eight GPRs, return address, and Win64 shadow space. */
    uintptr_t sp = top - 272;
    memset((void *)sp, 0, 232);

    ((uintptr_t *)(sp + 160))[3] = (uintptr_t)co; /* R12 */
    *(uintptr_t *)(sp + 224) = (uintptr_t)fds__coroutine_bootstrap;
#else
    /* Six saved GPRs and a return address; entry RSP must be 8 mod 16. */
    uintptr_t sp = top - 64;
    memset((void *)sp, 0, 64);

    ((uintptr_t *)sp)[3] = (uintptr_t)co; /* R12 */
    ((uintptr_t *)sp)[6] = (uintptr_t)fds__coroutine_bootstrap;
#endif

    co->context.sp = (void *)sp;
    return true;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((used, noinline, noreturn))
#endif
void fds__coroutine_entry(FdsCoroutine *co)
{
    co->fn(co->arg);
    co->state = FDS_COROUTINE_DONE;

    FdsCoroutineScheduler *scheduler = co->scheduler;
    fds__coroutine_current = NULL;
    fds__coroutine_context_switch(&co->context, &scheduler->context);

    abort();
}

void fds_coroutine_scheduler_init(FdsCoroutineScheduler *scheduler)
{
    if (scheduler)
        memset(scheduler, 0, sizeof(*scheduler));
}

FdsCoroutine *fds_coroutine_create(
    FdsCoroutineScheduler *scheduler,
    FdsCoroutineFn fn,
    void *arg,
    usize stack_size)
{
    if (!scheduler || !fn || scheduler->count == SIZE_MAX)
        return NULL;

    if (stack_size == 0)
        stack_size = FDS_COROUTINE_DEFAULT_STACK_SIZE;
    if (stack_size < FDS_COROUTINE_MIN_STACK_SIZE || stack_size > SIZE_MAX - 15)
        return NULL;

    stack_size = (stack_size + 15) & ~(usize)15;

    FdsCoroutine *co = (FdsCoroutine *)FDS_CALLOC(1, sizeof(*co));
    if (!co)
        return NULL;

    co->stack = FDS_MALLOC(stack_size);
    if (!co->stack) {
        FDS_FREE(co);
        return NULL;
    }

    co->scheduler = scheduler;
    co->fn = fn;
    co->arg = arg;
    co->stack_size = stack_size;
    co->state = FDS_COROUTINE_NEW;
    fds__coroutine_capture_fp(&co->context);

    if (!fds__coroutine_make_context(co) ||
        !fds__coroutine_scheduler_reserve(scheduler, scheduler->count + 1)) {
        FDS_FREE(co->stack);
        FDS_FREE(co);
        return NULL;
    }

    scheduler->items[scheduler->count++] = co;
    return co;
}

bool fds_coroutine_scheduler_run(FdsCoroutineScheduler *scheduler)
{
    if (!scheduler || scheduler->running)
        return false;

    FdsCoroutine *previous_current = fds__coroutine_current;
    scheduler->running = true;
    usize count_at_start = scheduler->count;

    for (usize i = 0; i < count_at_start; ++i) {
        FdsCoroutine *co = scheduler->items[i];
        if (co->state == FDS_COROUTINE_DONE)
            continue;

        scheduler->current = co;
        co->state = FDS_COROUTINE_RUNNING;
        fds__coroutine_current = co;
        fds__coroutine_context_switch(&scheduler->context, &co->context);
        fds__coroutine_current = NULL;
        scheduler->current = NULL;
    }

    scheduler->running = false;
    scheduler->current = NULL;
    fds__coroutine_current = previous_current;
    return true;
}

void fds_coroutine_yield(void)
{
    FdsCoroutine *co = fds__coroutine_current;
    if (!co || !co->scheduler)
        return;

    FdsCoroutineScheduler *scheduler = co->scheduler;
    co->state = FDS_COROUTINE_SUSPENDED;
    fds__coroutine_current = NULL;
    fds__coroutine_context_switch(&co->context, &scheduler->context);
}

bool fds_coroutine_is_done(const FdsCoroutine *co)
{
    return co && co->state == FDS_COROUTINE_DONE;
}

bool fds_coroutine_destroy(FdsCoroutine *co)
{
    if (!co || !co->scheduler)
        return false;

    FdsCoroutineScheduler *scheduler = co->scheduler;
    if (scheduler->running || co->state == FDS_COROUTINE_RUNNING)
        return false;

    usize index = 0;
    while (index < scheduler->count && scheduler->items[index] != co)
        ++index;
    if (index == scheduler->count)
        return false;

    for (usize i = index + 1; i < scheduler->count; ++i)
        scheduler->items[i - 1] = scheduler->items[i];
    --scheduler->count;

    FDS_FREE(co->stack);
    FDS_FREE(co);
    return true;
}

bool fds_coroutine_scheduler_destroy(FdsCoroutineScheduler *scheduler)
{
    if (!scheduler || scheduler->running)
        return false;

    for (usize i = 0; i < scheduler->count; ++i) {
        FDS_FREE(scheduler->items[i]->stack);
        FDS_FREE(scheduler->items[i]);
    }
    FDS_FREE(scheduler->items);
    memset(scheduler, 0, sizeof(*scheduler));
    return true;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif

#undef FDS_CORO_THREAD_LOCAL
#endif /* FDS_EXT_IMPL || FDS_COROUTINE_IMPL */

#endif /* FDS_EXT_COROUTINE_H */
