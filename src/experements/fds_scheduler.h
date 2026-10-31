#ifndef FDS_SCHEDULER_H
#define FDS_SCHEDULER_H

/*
    fds_scheduler.h - compact single-header scheduler for delayed and recurring callbacks in C

    In exactly one C/C++ translation unit:
        #define FDS_SCHEDULER_IMPLEMENTATION
        #include "fds_scheduler.h"

    Optional worker mode (define consistently before including in all translation
    units that use this header):
        #define FDS_SCHEDULER_THREADS

    Allocation can be customized before including the implementation:
        #define FDS_SCHEDULER_MALLOC(size) ...
        #define FDS_SCHEDULER_FREE(ptr) ...

    Threaded mode uses Win32 threads on Windows and pthreads on POSIX systems.
    Without FDS_SCHEDULER_THREADS, no platform thread header or thread library
    is required.
*/

#if defined(FDS_SCHEDULER_THREADS) && !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(FDS_SCHEDULER_THREADS) && defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(FDS_SCHEDULER_THREADS)
    #include <pthread.h>
    #include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FdsScheduler FdsScheduler;
typedef uint64_t FdsScheduleId;

/* One callback type for all scheduled work, whether one-shot or recurring. */
typedef void (*FdsSchedulerCallback)(
    FdsScheduler *scheduler,
    FdsScheduleId schedule,
    void *user_data
);

typedef struct FdsSchedulerEntry FdsSchedulerEntry;

struct FdsScheduler {
    FdsSchedulerEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
    uint64_t now_ms;
    uint64_t update_serial;
    FdsScheduleId next_schedule_id;
    bool updating;
    bool initialized;

#ifdef FDS_SCHEDULER_THREADS
    bool worker_created;
    bool worker_running;
    bool worker_stop_requested;
    bool worker_starting;
    uint32_t worker_tick_ms;
#ifdef _WIN32
    SRWLOCK mutex;
    HANDLE worker_handle;
    DWORD worker_id;
#else
    pthread_mutex_t mutex;
    pthread_t worker_handle;
#endif
#endif
};

/* Initializes an empty scheduler. The object must not already be initialized. */
bool fds_scheduler_init(FdsScheduler *scheduler);

/* Stops and joins a threaded worker, then releases timer storage. Do not call
   from a callback; in non-threaded mode destruction during update is ignored. */
void fds_scheduler_destroy(FdsScheduler *scheduler);

/* Runs once after delay_ms. Returns 0 on failure. A zero delay is allowed. */
FdsScheduleId fds_scheduler_after(
    FdsScheduler *scheduler,
    uint64_t delay_ms,
    FdsSchedulerCallback callback,
    void *user_data
);

/* Runs periodically. interval_ms must be greater than zero. */
FdsScheduleId fds_scheduler_every(
    FdsScheduler *scheduler,
    uint64_t interval_ms,
    FdsSchedulerCallback callback,
    void *user_data
);

/* Cancels a pending schedule. Returns false if the ID is invalid or inactive. */
bool fds_scheduler_cancel(FdsScheduler *scheduler, FdsScheduleId schedule);

/* Advances logical time by delta_ms and dispatches each due schedule at most once. */
void fds_scheduler_update(FdsScheduler *scheduler, uint64_t delta_ms);

#ifdef FDS_SCHEDULER_THREADS
/*
    Starts a worker that advances the scheduler using a monotonic clock.
    Scheduled callbacks execute on that worker, not on the caller's thread.
    tick_ms is clamped to at least 1 ms.

    Use the worker for background work, not for directly mutating game/render
    state. In a game loop, prefer fds_scheduler_update() on the main thread.
    While the worker has been started, do not call fds_scheduler_update().
    Scheduling and cancellation remain thread-safe in threaded builds.

    stop_worker() requests stop and joins the worker. If called from a callback,
    it only requests stop; call it again from another thread to join. Do not
    destroy the scheduler from a callback.
*/
bool fds_scheduler_start_worker(FdsScheduler *scheduler, uint32_t tick_ms);

/* Requests stop and joins the worker. Safe to call when no worker exists. */
bool fds_scheduler_stop_worker(FdsScheduler *scheduler);

/* Returns whether the worker is currently running. */
bool fds_scheduler_worker_running(FdsScheduler *scheduler);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FDS_SCHEDULER_H */

/* =========================== IMPLEMENTATION ============================ */
#if defined(FDS_SCHEDULER_IMPLEMENTATION) && !defined(FDS_SCHEDULER_IMPLEMENTED)
#define FDS_SCHEDULER_IMPLEMENTED

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(FDS_SCHEDULER_MALLOC) != defined(FDS_SCHEDULER_FREE)
    #error "Define both FDS_SCHEDULER_MALLOC and FDS_SCHEDULER_FREE, or neither."
#endif

#if !defined(FDS_SCHEDULER_MALLOC) && !defined(FDS_SCHEDULER_FREE)
    #if defined(FDS_ALLOC) && defined(FDS_FREE)
        #define FDS_SCHEDULER_MALLOC(size) FDS_ALLOC(size)
        #define FDS_SCHEDULER_FREE(ptr) FDS_FREE(ptr)
    #elif defined(FDS_MALLOC) && defined(FDS_FREE)
        #define FDS_SCHEDULER_MALLOC(size) FDS_MALLOC(size)
        #define FDS_SCHEDULER_FREE(ptr) FDS_FREE(ptr)
    #else
        #define FDS_SCHEDULER_MALLOC(size) malloc(size)
        #define FDS_SCHEDULER_FREE(ptr) free(ptr)
    #endif
#endif

struct FdsSchedulerEntry {
    FdsScheduleId id;
    uint64_t due_ms;
    uint64_t interval_ms;
    uint64_t born_serial;
    uint64_t dispatched_serial;
    FdsSchedulerCallback callback;
    void *user_data;
    bool active;
    bool repeating;
};

static uint64_t fds_scheduler_internal_add_sat_u64(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

#ifdef FDS_SCHEDULER_THREADS
static void fds_scheduler_internal_lock(FdsScheduler *scheduler)
{
#ifdef _WIN32
    AcquireSRWLockExclusive(&scheduler->mutex);
#else
    (void)pthread_mutex_lock(&scheduler->mutex);
#endif
}

static void fds_scheduler_internal_unlock(FdsScheduler *scheduler)
{
#ifdef _WIN32
    ReleaseSRWLockExclusive(&scheduler->mutex);
#else
    (void)pthread_mutex_unlock(&scheduler->mutex);
#endif
}
#else
static void fds_scheduler_internal_lock(FdsScheduler *scheduler)
{
    (void)scheduler;
}
static void fds_scheduler_internal_unlock(FdsScheduler *scheduler)
{
    (void)scheduler;
}
#endif

static bool fds_scheduler_internal_reserve(FdsScheduler *scheduler, size_t wanted)
{
    size_t capacity;
    FdsSchedulerEntry *entries;

    if (wanted <= scheduler->entry_capacity)
        return true;

    capacity = scheduler->entry_capacity ? scheduler->entry_capacity : 8;
    while (capacity < wanted) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*entries))
        return false;

    entries = (FdsSchedulerEntry *)FDS_SCHEDULER_MALLOC(capacity * sizeof(*entries));
    if (!entries)
        return false;

    if (scheduler->entry_count)
        memcpy(entries, scheduler->entries,
               scheduler->entry_count * sizeof(*entries));
    FDS_SCHEDULER_FREE(scheduler->entries);
    scheduler->entries = entries;
    scheduler->entry_capacity = capacity;
    return true;
}

static FdsSchedulerEntry *fds_scheduler_internal_find(
    FdsScheduler *scheduler,
    FdsScheduleId id
)
{
    size_t i;
    for (i = 0; i < scheduler->entry_count; ++i) {
        if (scheduler->entries[i].active && scheduler->entries[i].id == id)
            return &scheduler->entries[i];
    }
    return NULL;
}

/* Drop inactive slots so historical peak usage does not slow every update. */
static void fds_scheduler_internal_compact(FdsScheduler *scheduler)
{
    size_t read_index;
    size_t write_index = 0;

    for (read_index = 0; read_index < scheduler->entry_count; ++read_index) {
        if (!scheduler->entries[read_index].active)
            continue;
        if (write_index != read_index)
            scheduler->entries[write_index] = scheduler->entries[read_index];
        ++write_index;
    }
    scheduler->entry_count = write_index;
}

static FdsScheduleId fds_scheduler_internal_add(
    FdsScheduler *scheduler,
    uint64_t delay_ms,
    uint64_t interval_ms,
    bool repeating,
    FdsSchedulerCallback callback,
    void *user_data
)
{
    size_t i;
    FdsSchedulerEntry *timer = NULL;
    FdsScheduleId id;

    if (!scheduler || !scheduler->initialized || !callback)
        return 0;
    if (repeating && interval_ms == 0)
        return 0;
    if (scheduler->next_schedule_id == 0)
        return 0; /* The ID space has been exhausted. */

    for (i = 0; i < scheduler->entry_count; ++i) {
        if (!scheduler->entries[i].active) {
            timer = &scheduler->entries[i];
            break;
        }
    }

    if (!timer) {
        if (!fds_scheduler_internal_reserve(scheduler, scheduler->entry_count + 1))
            return 0;
        timer = &scheduler->entries[scheduler->entry_count++];
    }

    id = scheduler->next_schedule_id++;
    timer->id = id;
    timer->due_ms = fds_scheduler_internal_add_sat_u64(scheduler->now_ms, delay_ms);
    timer->interval_ms = interval_ms;
    timer->born_serial = scheduler->update_serial;
    timer->dispatched_serial = 0;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->active = true;
    timer->repeating = repeating;
    return id;
}

bool fds_scheduler_init(FdsScheduler *scheduler)
{
    if (!scheduler)
        return false;

    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->next_schedule_id = 1;
#ifdef FDS_SCHEDULER_THREADS
#ifdef _WIN32
    InitializeSRWLock(&scheduler->mutex);
#else
    if (pthread_mutex_init(&scheduler->mutex, NULL) != 0)
        return false;
#endif
#endif
    scheduler->initialized = true;
    return true;
}

void fds_scheduler_destroy(FdsScheduler *scheduler)
{
    if (!scheduler || !scheduler->initialized)
        return;
#ifdef FDS_SCHEDULER_THREADS
    /* Destroying from the worker would free state still used by that worker. */
    fds_scheduler_internal_lock(scheduler);
    if (scheduler->worker_created) {
#ifdef _WIN32
        bool called_from_worker = GetCurrentThreadId() == scheduler->worker_id;
#else
        bool called_from_worker = pthread_equal(pthread_self(), scheduler->worker_handle) != 0;
#endif
        fds_scheduler_internal_unlock(scheduler);
        if (called_from_worker)
            return;
    } else {
        fds_scheduler_internal_unlock(scheduler);
    }
    (void)fds_scheduler_stop_worker(scheduler);
#else
    fds_scheduler_internal_lock(scheduler);
    if (scheduler->updating) {
        fds_scheduler_internal_unlock(scheduler);
        return;
    }
    fds_scheduler_internal_unlock(scheduler);
#endif
    fds_scheduler_internal_lock(scheduler);
    FDS_SCHEDULER_FREE(scheduler->entries);
    scheduler->entries = NULL;
    scheduler->entry_count = 0;
    scheduler->entry_capacity = 0;
    scheduler->initialized = false;
    fds_scheduler_internal_unlock(scheduler);
#ifdef FDS_SCHEDULER_THREADS
#ifndef _WIN32
    (void)pthread_mutex_destroy(&scheduler->mutex);
#endif
#endif
}

FdsScheduleId fds_scheduler_after(
    FdsScheduler *scheduler,
    uint64_t delay_ms,
    FdsSchedulerCallback callback,
    void *user_data
)
{
    FdsScheduleId id;
    if (!scheduler || !scheduler->initialized)
        return 0;
    fds_scheduler_internal_lock(scheduler);
    id = fds_scheduler_internal_add(scheduler, delay_ms, 0, false, callback, user_data);
    fds_scheduler_internal_unlock(scheduler);
    return id;
}

FdsScheduleId fds_scheduler_every(
    FdsScheduler *scheduler,
    uint64_t interval_ms,
    FdsSchedulerCallback callback,
    void *user_data
)
{
    FdsScheduleId id;
    if (!scheduler || !scheduler->initialized || interval_ms == 0)
        return 0;
    fds_scheduler_internal_lock(scheduler);
    id = fds_scheduler_internal_add(
        scheduler, interval_ms, interval_ms, true, callback, user_data
    );
    fds_scheduler_internal_unlock(scheduler);
    return id;
}

bool fds_scheduler_cancel(FdsScheduler *scheduler, FdsScheduleId schedule_id)
{
    FdsSchedulerEntry *timer;
    bool cancelled = false;

    if (!scheduler || !scheduler->initialized || schedule_id == 0)
        return false;

    fds_scheduler_internal_lock(scheduler);
    timer = fds_scheduler_internal_find(scheduler, schedule_id);
    if (timer) {
        timer->active = false;
        timer->callback = NULL;
        timer->user_data = NULL;
        cancelled = true;
        if (!scheduler->updating)
            fds_scheduler_internal_compact(scheduler);
    }
    fds_scheduler_internal_unlock(scheduler);
    return cancelled;
}

static void fds_scheduler_internal_update(
    FdsScheduler *scheduler,
    uint64_t delta_ms,
    bool from_worker
)
{
    uint64_t serial;

    if (!scheduler || !scheduler->initialized)
        return;

    fds_scheduler_internal_lock(scheduler);
#ifdef FDS_SCHEDULER_THREADS
    if ((scheduler->worker_created && !from_worker) || scheduler->updating ||
        !scheduler->initialized) {
#else
    (void)from_worker;
    if (scheduler->updating || !scheduler->initialized) {
#endif
        fds_scheduler_internal_unlock(scheduler);
        return;
    }

    scheduler->updating = true;
    scheduler->now_ms = fds_scheduler_internal_add_sat_u64(scheduler->now_ms, delta_ms);
    ++scheduler->update_serial;
    if (scheduler->update_serial == 0) {
        size_t i;
        scheduler->update_serial = 1;
        for (i = 0; i < scheduler->entry_count; ++i) {
            scheduler->entries[i].born_serial = 0;
            scheduler->entries[i].dispatched_serial = 0;
        }
    }
    serial = scheduler->update_serial;
    fds_scheduler_internal_unlock(scheduler);

    for (;;) {
        FdsSchedulerCallback callback = NULL;
        void *user_data = NULL;
        FdsScheduleId id = 0;
        size_t i;
        FdsSchedulerEntry *chosen = NULL;

        fds_scheduler_internal_lock(scheduler);
        for (i = 0; i < scheduler->entry_count; ++i) {
            FdsSchedulerEntry *timer = &scheduler->entries[i];

            if (!timer->active || timer->due_ms > scheduler->now_ms ||
                timer->born_serial == serial || timer->dispatched_serial == serial)
                continue;

            /* Stable dispatch order: earlier deadline first, then lower schedule ID. */
            if (!chosen || timer->due_ms < chosen->due_ms ||
                (timer->due_ms == chosen->due_ms && timer->id < chosen->id))
                chosen = timer;
        }

        if (chosen) {
            chosen->dispatched_serial = serial;
            callback = chosen->callback;
            user_data = chosen->user_data;
            id = chosen->id;

            if (!chosen->repeating) {
                chosen->active = false;
                chosen->callback = NULL;
                chosen->user_data = NULL;
            } else {
                uint64_t missed = (scheduler->now_ms - chosen->due_ms) /
                                  chosen->interval_ms + 1;
                uint64_t remaining = UINT64_MAX - chosen->due_ms;
                if (missed > remaining / chosen->interval_ms) {
                    /* No representable future deadline remains. */
                    chosen->active = false;
                } else {
                    chosen->due_ms += missed * chosen->interval_ms;
                }
            }
        }
        fds_scheduler_internal_unlock(scheduler);

        if (!callback)
            break;
        callback(scheduler, id, user_data);
    }

    fds_scheduler_internal_lock(scheduler);
    fds_scheduler_internal_compact(scheduler);
    scheduler->updating = false;
    fds_scheduler_internal_unlock(scheduler);
}

void fds_scheduler_update(FdsScheduler *scheduler, uint64_t delta_ms)
{
    fds_scheduler_internal_update(scheduler, delta_ms, false);
}

#ifdef FDS_SCHEDULER_THREADS
static uint64_t fds_scheduler_internal_monotonic_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    uint64_t whole;
    uint64_t remainder;

    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart <= 0)
        return (uint64_t)GetTickCount64();
    whole = (uint64_t)(counter.QuadPart / frequency.QuadPart);
    remainder = (uint64_t)(counter.QuadPart % frequency.QuadPart);
    return whole * 1000u + remainder * 1000u / (uint64_t)frequency.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

static void fds_scheduler_internal_sleep_ms(uint32_t ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec request;
    request.tv_sec = (time_t)(ms / 1000u);
    request.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&request, &request) != 0) {
        /* Continue after EINTR. */
    }
#endif
}

static bool fds_scheduler_internal_stop_requested(FdsScheduler *scheduler)
{
    bool stop;
    fds_scheduler_internal_lock(scheduler);
    stop = scheduler->worker_stop_requested;
    fds_scheduler_internal_unlock(scheduler);
    return stop;
}

#ifdef _WIN32
static DWORD WINAPI fds_scheduler_internal_worker_proc(LPVOID arg)
#else
static void *fds_scheduler_internal_worker_proc(void *arg)
#endif
{
    FdsScheduler *scheduler = (FdsScheduler *)arg;
    uint64_t previous_ms;
    uint64_t next_update_ms;
    uint32_t tick_ms;

    /* Wait until start_worker has stored the platform thread handle/state. */
    fds_scheduler_internal_lock(scheduler);
    while (scheduler->worker_starting) {
        fds_scheduler_internal_unlock(scheduler);
        fds_scheduler_internal_sleep_ms(1);
        fds_scheduler_internal_lock(scheduler);
    }
    scheduler->worker_running = true;
    tick_ms = scheduler->worker_tick_ms;
    fds_scheduler_internal_unlock(scheduler);

    previous_ms = fds_scheduler_internal_monotonic_ms();
    next_update_ms = fds_scheduler_internal_add_sat_u64(previous_ms, tick_ms);
    while (!fds_scheduler_internal_stop_requested(scheduler)) {
        uint64_t now_ms = fds_scheduler_internal_monotonic_ms();

        if (now_ms < next_update_ms) {
            uint64_t wait_ms = next_update_ms - now_ms;
            /* Poll in short slices so stop_worker does not wait for a huge tick. */
            fds_scheduler_internal_sleep_ms(wait_ms > 10u ? 10u : (uint32_t)wait_ms);
            continue;
        }

        fds_scheduler_internal_update(scheduler, now_ms >= previous_ms ? now_ms - previous_ms : 0, true);
        previous_ms = now_ms;
        next_update_ms = fds_scheduler_internal_add_sat_u64(now_ms, tick_ms);
    }

    fds_scheduler_internal_lock(scheduler);
    scheduler->worker_running = false;
    fds_scheduler_internal_unlock(scheduler);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

bool fds_scheduler_start_worker(FdsScheduler *scheduler, uint32_t tick_ms)
{
    if (!scheduler || !scheduler->initialized)
        return false;
    if (tick_ms == 0)
        tick_ms = 1;

    fds_scheduler_internal_lock(scheduler);
    if (scheduler->worker_created || scheduler->worker_starting) {
        fds_scheduler_internal_unlock(scheduler);
        return false;
    }
    scheduler->worker_stop_requested = false;
    scheduler->worker_running = false;
    scheduler->worker_starting = true;
    scheduler->worker_tick_ms = tick_ms;
    fds_scheduler_internal_unlock(scheduler);

#ifdef _WIN32
    {
        DWORD worker_id = 0;
        HANDLE handle = CreateThread(
            NULL, 0, fds_scheduler_internal_worker_proc, scheduler, 0, &worker_id
        );
        if (!handle) {
            fds_scheduler_internal_lock(scheduler);
            scheduler->worker_starting = false;
            fds_scheduler_internal_unlock(scheduler);
            return false;
        }
        fds_scheduler_internal_lock(scheduler);
        scheduler->worker_handle = handle;
        scheduler->worker_id = worker_id;
        scheduler->worker_created = true;
        scheduler->worker_starting = false;
        fds_scheduler_internal_unlock(scheduler);
    }
#else
    {
        pthread_t thread;
        if (pthread_create(&thread, NULL, fds_scheduler_internal_worker_proc, scheduler) != 0) {
            fds_scheduler_internal_lock(scheduler);
            scheduler->worker_starting = false;
            fds_scheduler_internal_unlock(scheduler);
            return false;
        }
        fds_scheduler_internal_lock(scheduler);
        scheduler->worker_handle = thread;
        scheduler->worker_created = true;
        scheduler->worker_starting = false;
        fds_scheduler_internal_unlock(scheduler);
    }
#endif
    return true;
}

bool fds_scheduler_stop_worker(FdsScheduler *scheduler)
{
#ifdef _WIN32
    HANDLE worker_handle;
    DWORD worker_id;
#else
    pthread_t worker_handle;
#endif

    if (!scheduler || !scheduler->initialized)
        return false;

    fds_scheduler_internal_lock(scheduler);
    if (!scheduler->worker_created) {
        fds_scheduler_internal_unlock(scheduler);
        return true;
    }
    scheduler->worker_stop_requested = true;
#ifdef _WIN32
    worker_handle = scheduler->worker_handle;
    worker_id = scheduler->worker_id;
    if (GetCurrentThreadId() == worker_id) {
        fds_scheduler_internal_unlock(scheduler);
        return true;
    }
#else
    worker_handle = scheduler->worker_handle;
    if (pthread_equal(pthread_self(), worker_handle)) {
        fds_scheduler_internal_unlock(scheduler);
        return true;
    }
#endif
    fds_scheduler_internal_unlock(scheduler);

#ifdef _WIN32
    if (WaitForSingleObject(worker_handle, INFINITE) != WAIT_OBJECT_0)
        return false;
    CloseHandle(worker_handle);
#else
    if (pthread_join(worker_handle, NULL) != 0)
        return false;
#endif

    fds_scheduler_internal_lock(scheduler);
    scheduler->worker_created = false;
    scheduler->worker_running = false;
    scheduler->worker_stop_requested = false;
    scheduler->worker_starting = false;
#ifdef _WIN32
    scheduler->worker_handle = NULL;
    scheduler->worker_id = 0;
#endif
    fds_scheduler_internal_unlock(scheduler);
    return true;
}

bool fds_scheduler_worker_running(FdsScheduler *scheduler)
{
    bool running;
    if (!scheduler || !scheduler->initialized)
        return false;
    fds_scheduler_internal_lock(scheduler);
    running = scheduler->worker_running;
    fds_scheduler_internal_unlock(scheduler);
    return running;
}
#endif /* FDS_SCHEDULER_THREADS */

#endif /* FDS_SCHEDULER_IMPLEMENTATION */
