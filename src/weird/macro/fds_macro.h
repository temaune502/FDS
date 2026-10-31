#ifndef FDS_MACRO_H
#define FDS_MACRO_H

/* Define this before including other system headers when using strict C modes. */
#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#if defined(__linux__)
#include <ucontext.h>
#else
#error "fds_macro.h: coroutine backend is currently implemented for Windows and Linux only."
#endif
#endif

#ifndef FDS_MACRO_DEFAULT_STACK_SIZE
#define FDS_MACRO_DEFAULT_STACK_SIZE (64u * 1024u)
#endif

#ifndef FDS_MACRO_DEFAULT_FPS
#define FDS_MACRO_DEFAULT_FPS 60
#endif

#ifndef FDS_MACRO_MAX_FPS
#define FDS_MACRO_MAX_FPS 1000
#endif

typedef enum {
    FDS_MOUSE_LEFT = 0,
    FDS_MOUSE_RIGHT,
    FDS_MOUSE_MIDDLE
} FdsMouseButton;

typedef void (*FdsMacroTickFunc)(void *user_data, float dt);
typedef void (*FdsMacroHotkeyFunc)(void *user_data);
typedef void (*FdsMacroPauseFunc)(void *user_data, bool paused);
typedef void (*FdsMacroCoroutineFunc)(void *user_data);

typedef enum {
    FDS_MACRO_CO_READY = 0,
    FDS_MACRO_CO_WAITING,
    FDS_MACRO_CO_FINISHED
} FdsMacroCoroutineState;

typedef struct FdsMacroContext FdsMacroContext;
typedef struct FdsMacroCoroutine FdsMacroCoroutine;

typedef struct {
    int vk_code;
    bool is_pressed;
    FdsMacroHotkeyFunc callback;
} FdsMacroHotkey;

struct FdsMacroCoroutine {
    FdsMacroContext *owner;
    FdsMacroCoroutineFunc function;
    void *user_data;
    uint64_t wake_at_ns;
    size_t stack_size;
    FdsMacroCoroutineState state;
    FdsMacroCoroutine *next;
#ifdef _WIN32
    void *fiber;
#else
    ucontext_t context;
    void *stack;
#endif
};

struct FdsMacroContext {
    int target_fps;
    FdsMacroTickFunc tick_func;
    void *user_data;

    FdsMacroHotkey *hotkeys;
    size_t hotkey_count;

    FdsMacroCoroutine *coroutines;
    FdsMacroCoroutine *coroutines_tail;
    FdsMacroCoroutine *current_coroutine;

    FdsMacroPauseFunc pause_func;
    bool paused;
    uint64_t pause_started_ns;

    atomic_bool running;
#ifdef _WIN32
    void *scheduler_fiber;
    bool owns_scheduler_fiber;
#else
    ucontext_t scheduler_context;
#endif
};

/* One definition in exactly one translation unit: #define FDS_MACRO_IMPL first. */
#ifdef FDS_MACRO_IMPL
_Thread_local FdsMacroCoroutine *fds_macro_current_coroutine = NULL;
#else
extern _Thread_local FdsMacroCoroutine *fds_macro_current_coroutine;
#endif

/* Internal time helpers */
static inline uint64_t fds_macro__now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        return GetTickCount64() * UINT64_C(1000000);
    uint64_t ticks = (uint64_t)counter.QuadPart;
    uint64_t freq = (uint64_t)frequency.QuadPart;
    return (ticks / freq) * UINT64_C(1000000000) +
           ((ticks % freq) * UINT64_C(1000000000)) / freq;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return (uint64_t)time(NULL) * UINT64_C(1000000000);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
#endif
}

static inline void fds_macro__blocking_sleep(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L
    };
    struct timespec rem;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) req = rem;
#endif
}

static inline void fds_macro__switch_to_scheduler(FdsMacroCoroutine *co) {
    FdsMacroContext *ctx = co->owner;
    fds_macro_current_coroutine = NULL;
    ctx->current_coroutine = NULL;
#ifdef _WIN32
    SwitchToFiber(ctx->scheduler_fiber);
#else
    if (swapcontext(&co->context, &ctx->scheduler_context) == -1) {
        /* A context-switch failure is not recoverable, but restore bookkeeping. */
        fds_macro_current_coroutine = co;
        ctx->current_coroutine = co;
    }
#endif
}

static inline void fds_macro_yield(void) {
    FdsMacroCoroutine *co = fds_macro_current_coroutine;
    if (!co) return;

    co->state = FDS_MACRO_CO_READY;
    fds_macro__switch_to_scheduler(co);
    /* The scheduler restores current_coroutine before resuming this stack. */
}

/* A sleep inside a macro coroutine yields; outside one it blocks as usual. */
static inline void fds_macro_sleep(int ms) {
    if (ms <= 0) {
        if (fds_macro_current_coroutine) fds_macro_yield();
        return;
    }

    FdsMacroCoroutine *co = fds_macro_current_coroutine;
    if (!co) {
        fds_macro__blocking_sleep(ms);
        return;
    }

    uint64_t now = fds_macro__now_ns();
    uint64_t delay = (uint64_t)ms * UINT64_C(1000000);
    co->wake_at_ns = UINT64_MAX - now < delay ? UINT64_MAX : now + delay;
    co->state = FDS_MACRO_CO_WAITING;
    fds_macro__switch_to_scheduler(co);
}

/* Original input API: behavior intentionally unchanged. */
static inline void fds_macro_get_mouse_pos(int *x, int *y) {
#ifdef _WIN32
    POINT pt;
    GetCursorPos(&pt);
    if (x) *x = pt.x;
    if (y) *y = pt.y;
#else
    (void)x;
    (void)y;
#endif
}

static inline bool fds_macro_is_key_down(int vk_code) {
#ifdef _WIN32
    return (GetAsyncKeyState(vk_code) & 0x8000) != 0;
#else
    (void)vk_code;
    return false;
#endif
}

static inline void fds_macro_mouse_move_abs(int x, int y) {
#ifdef _WIN32
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = (x * 65535) / screen_w;
    input.mi.dy = (y * 65535) / screen_h;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &input, sizeof(INPUT));
#else
    (void)x;
    (void)y;
#endif
}

static inline void fds_macro_mouse_move_rel(int dx, int dy) {
#ifdef _WIN32
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
#else
    (void)dx;
    (void)dy;
#endif
}

static inline void fds_macro_mouse_button(FdsMouseButton btn, bool down) {
#ifdef _WIN32
    DWORD flags = 0;
    if (btn == FDS_MOUSE_LEFT) flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    else if (btn == FDS_MOUSE_RIGHT) flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    else if (btn == FDS_MOUSE_MIDDLE) flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(INPUT));
#else
    (void)btn;
    (void)down;
#endif
}

static inline void fds_macro_mouse_click(FdsMouseButton btn) {
    fds_macro_mouse_button(btn, true);
    fds_macro_sleep(50);
    fds_macro_mouse_button(btn, false);
}

static inline void fds_macro_key_state(uint8_t vk_code, bool down) {
#ifdef _WIN32
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk_code;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
#else
    (void)vk_code;
    (void)down;
#endif
}

static inline void fds_macro_key_press(uint8_t vk_code) {
    fds_macro_key_state(vk_code, true);
    fds_macro_sleep(50);
    fds_macro_key_state(vk_code, false);
}

static inline void fds_macro_init(
    FdsMacroContext *ctx,
    int target_fps,
    FdsMacroTickFunc tick_func,
    FdsMacroHotkey *hotkeys_array,
    size_t hotkey_count,
    void *user_data
) {
    if (!ctx) return;

    ctx->target_fps = target_fps > 0 ? target_fps : FDS_MACRO_DEFAULT_FPS;
    if (ctx->target_fps > FDS_MACRO_MAX_FPS) ctx->target_fps = FDS_MACRO_MAX_FPS;
    ctx->tick_func = tick_func;
    ctx->user_data = user_data;
    ctx->hotkeys = hotkeys_array;
    ctx->hotkey_count = hotkeys_array ? hotkey_count : 0;
    ctx->coroutines = NULL;
    ctx->coroutines_tail = NULL;
    ctx->current_coroutine = NULL;
    ctx->pause_func = NULL;
    ctx->paused = false;
    ctx->pause_started_ns = 0;
    atomic_init(&ctx->running, false);
#ifdef _WIN32
    ctx->scheduler_fiber = NULL;
    ctx->owns_scheduler_fiber = false;
#endif

    for (size_t i = 0; i < ctx->hotkey_count; ++i) {
        ctx->hotkeys[i].is_pressed = fds_macro_is_key_down(ctx->hotkeys[i].vk_code);
    }
}

static inline void fds_macro_set_pause_callback(FdsMacroContext *ctx, FdsMacroPauseFunc callback) {
    if (ctx) ctx->pause_func = callback;
}

static inline void fds_macro_set_paused(FdsMacroContext *ctx, bool paused) {
    if (!ctx || ctx->paused == paused) return;

    uint64_t now = fds_macro__now_ns();
    if (paused) {
        ctx->paused = true;
        ctx->pause_started_ns = now;
    } else {
        uint64_t elapsed = now >= ctx->pause_started_ns ? now - ctx->pause_started_ns : 0;
        for (FdsMacroCoroutine *co = ctx->coroutines; co; co = co->next) {
            if (co->state == FDS_MACRO_CO_WAITING && UINT64_MAX - co->wake_at_ns >= elapsed)
                co->wake_at_ns += elapsed;
        }
        ctx->paused = false;
        ctx->pause_started_ns = 0;
    }

    if (ctx->pause_func) ctx->pause_func(ctx->user_data, paused);
}

static inline void fds_macro_toggle_pause(FdsMacroContext *ctx) {
    if (ctx) fds_macro_set_paused(ctx, !ctx->paused);
}

#ifdef _WIN32
static VOID WINAPI fds_macro__fiber_entry(void *parameter) {
    FdsMacroCoroutine *co = (FdsMacroCoroutine *)parameter;
    co->function(co->user_data);
    co->state = FDS_MACRO_CO_FINISHED;
    fds_macro__switch_to_scheduler(co);
    /* A finished fiber must never be resumed. */
    abort();
}
#else
static void fds_macro__ucontext_entry(uint32_t lo, uint32_t hi) {
    uintptr_t raw = (uintptr_t)lo;
#if UINTPTR_MAX > UINT32_MAX
    raw |= (uintptr_t)hi << 32;
#else
    (void)hi;
#endif
    FdsMacroCoroutine *co = (FdsMacroCoroutine *)raw;
    co->function(co->user_data);
    co->state = FDS_MACRO_CO_FINISHED;
    fds_macro__switch_to_scheduler(co);
    abort();
}
#endif

static inline FdsMacroCoroutine *fds_macro_coroutine_create(
    FdsMacroContext *ctx,
    FdsMacroCoroutineFunc function,
    void *user_data,
    size_t stack_size
) {
    if (!ctx || !function || atomic_load_explicit(&ctx->running, memory_order_acquire)) return NULL;

    if (stack_size < FDS_MACRO_DEFAULT_STACK_SIZE) stack_size = FDS_MACRO_DEFAULT_STACK_SIZE;

    FdsMacroCoroutine *co = (FdsMacroCoroutine *)calloc(1, sizeof(*co));
    if (!co) return NULL;

    co->owner = ctx;
    co->function = function;
    co->user_data = user_data;
    co->stack_size = stack_size;
    co->state = FDS_MACRO_CO_READY;

#ifdef _WIN32
    co->fiber = CreateFiberEx(0, stack_size, FIBER_FLAG_FLOAT_SWITCH, fds_macro__fiber_entry, co);
    if (!co->fiber) {
        free(co);
        return NULL;
    }
#else
    co->stack = malloc(stack_size);
    if (!co->stack) {
        free(co);
        return NULL;
    }
    if (getcontext(&co->context) == -1) {
        free(co->stack);
        free(co);
        return NULL;
    }
    co->context.uc_stack.ss_sp = co->stack;
    co->context.uc_stack.ss_size = stack_size;
    co->context.uc_stack.ss_flags = 0;
    co->context.uc_link = NULL;

    uintptr_t raw = (uintptr_t)co;
    uint32_t lo = (uint32_t)(raw & UINT32_MAX);
#if UINTPTR_MAX > UINT32_MAX
    uint32_t hi = (uint32_t)(raw >> 32);
#else
    uint32_t hi = 0;
#endif
    makecontext(&co->context, (void (*)(void))fds_macro__ucontext_entry, 2, lo, hi);
#endif

    if (ctx->coroutines_tail) ctx->coroutines_tail->next = co;
    else ctx->coroutines = co;
    ctx->coroutines_tail = co;
    return co;
}

static inline void fds_macro_stop(FdsMacroContext *ctx) {
    if (ctx) atomic_store_explicit(&ctx->running, false, memory_order_release);
}

static inline void fds_macro_run(FdsMacroContext *ctx) {
    if (!ctx) return;

    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(
            &ctx->running, &expected, true,
            memory_order_acquire, memory_order_relaxed)) return;

#ifdef _WIN32
    void *fiber = ConvertThreadToFiber(NULL);
    if (fiber) {
        ctx->scheduler_fiber = fiber;
        ctx->owns_scheduler_fiber = true;
    } else if (GetLastError() == ERROR_ALREADY_FIBER) {
        ctx->scheduler_fiber = GetCurrentFiber();
        ctx->owns_scheduler_fiber = false;
    } else {
        atomic_store_explicit(&ctx->running, false, memory_order_release);
        return;
    }
#endif

    if (ctx->target_fps <= 0) ctx->target_fps = FDS_MACRO_DEFAULT_FPS;
    if (ctx->target_fps > FDS_MACRO_MAX_FPS) ctx->target_fps = FDS_MACRO_MAX_FPS;
    const uint64_t frame_ns = UINT64_C(1000000000) / (uint64_t)ctx->target_fps;
    uint64_t last_ns = fds_macro__now_ns();

    while (atomic_load_explicit(&ctx->running, memory_order_acquire)) {
        uint64_t frame_start = fds_macro__now_ns();
        float dt = (float)((double)(frame_start - last_ns) / 1000000000.0);
        last_ns = frame_start;

        for (size_t i = 0; i < ctx->hotkey_count; ++i) {
            if (!atomic_load_explicit(&ctx->running, memory_order_acquire)) break;
            FdsMacroHotkey *hk = &ctx->hotkeys[i];
            bool down = fds_macro_is_key_down(hk->vk_code);
            bool was_down = hk->is_pressed;
            hk->is_pressed = down;
            if (down && !was_down && hk->callback) hk->callback(ctx->user_data);
        }

        if (!atomic_load_explicit(&ctx->running, memory_order_acquire)) break;
        if (ctx->tick_func) ctx->tick_func(ctx->user_data, dt);

        if (!atomic_load_explicit(&ctx->running, memory_order_acquire)) break;

        if (!ctx->paused) {
            uint64_t now = fds_macro__now_ns();
            for (FdsMacroCoroutine *co = ctx->coroutines; co; co = co->next) {
                if (!atomic_load_explicit(&ctx->running, memory_order_acquire) || ctx->paused) break;
                if (co->state == FDS_MACRO_CO_FINISHED) continue;
                if (co->state == FDS_MACRO_CO_WAITING) {
                    if (now < co->wake_at_ns) continue;
                    co->state = FDS_MACRO_CO_READY;
                }

                ctx->current_coroutine = co;
                fds_macro_current_coroutine = co;
#ifdef _WIN32
                SwitchToFiber(co->fiber);
#else
                if (swapcontext(&ctx->scheduler_context, &co->context) == -1) {
                    co->state = FDS_MACRO_CO_FINISHED;
                }
#endif
                ctx->current_coroutine = NULL;
                fds_macro_current_coroutine = NULL;
                now = fds_macro__now_ns();
            }
        }

        uint64_t now = fds_macro__now_ns();
        uint64_t elapsed = now >= frame_start ? now - frame_start : 0;
        if (elapsed < frame_ns) {
            uint64_t remaining_ms = (frame_ns - elapsed) / UINT64_C(1000000);
            if (remaining_ms > 0 && remaining_ms <= INT32_MAX)
                fds_macro__blocking_sleep((int)remaining_ms);
        }
    }

    atomic_store_explicit(&ctx->running, false, memory_order_release);
#ifdef _WIN32
    if (ctx->owns_scheduler_fiber) ConvertFiberToThread();
    ctx->scheduler_fiber = NULL;
    ctx->owns_scheduler_fiber = false;
#endif
}

static inline void fds_macro_destroy(FdsMacroContext *ctx) {
    if (!ctx || atomic_load_explicit(&ctx->running, memory_order_acquire)) return;

    FdsMacroCoroutine *co = ctx->coroutines;
    while (co) {
        FdsMacroCoroutine *next = co->next;
#ifdef _WIN32
        if (co->fiber) DeleteFiber(co->fiber);
#else
        free(co->stack);
#endif
        free(co);
        co = next;
    }

    ctx->coroutines = NULL;
    ctx->coroutines_tail = NULL;
    ctx->current_coroutine = NULL;
}

#endif /* FDS_MACRO_H */
