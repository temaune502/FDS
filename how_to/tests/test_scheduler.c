#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define FDS_SCHEDULER_IMPLEMENTATION
#include "fds_scheduler.h"

typedef struct TestCounter {
    unsigned calls;
} TestCounter;

static void count_callback(FdsScheduler *scheduler, FdsScheduleId schedule, void *user_data)
{
    TestCounter *counter = (TestCounter *)user_data;
    (void)scheduler;
    (void)schedule;
    ++counter->calls;
}

typedef struct SelfCancelData {
    unsigned calls;
} SelfCancelData;

static void cancel_self_callback(FdsScheduler *scheduler, FdsScheduleId schedule, void *user_data)
{
    SelfCancelData *data = (SelfCancelData *)user_data;
    ++data->calls;
    assert(fds_scheduler_cancel(scheduler, schedule));
}

typedef struct AddFromCallbackData {
    unsigned calls;
    FdsScheduleId child_timer;
    TestCounter child_counter;
} AddFromCallbackData;

static void child_callback(FdsScheduler *scheduler, FdsScheduleId schedule, void *user_data)
{
    TestCounter *counter = (TestCounter *)user_data;
    (void)scheduler;
    (void)schedule;
    ++counter->calls;
}

static void add_from_callback(FdsScheduler *scheduler, FdsScheduleId schedule, void *user_data)
{
    AddFromCallbackData *data = (AddFromCallbackData *)user_data;
    (void)schedule;
    ++data->calls;
    data->child_timer = fds_scheduler_after(scheduler, 0, child_callback, &data->child_counter);
    assert(data->child_timer != 0);
}

static void test_manual_scheduler(void)
{
    FdsScheduler scheduler;
    TestCounter after_count = {0};
    TestCounter every_count = {0};
    SelfCancelData self_cancel = {0};
    AddFromCallbackData add_data = {0};
    FdsScheduleId after_id;
    FdsScheduleId every_id;
    FdsScheduleId self_id;
    FdsScheduleId add_id;

    assert(fds_scheduler_init(&scheduler));

    after_id = fds_scheduler_after(&scheduler, 100, count_callback, &after_count);
    every_id = fds_scheduler_every(&scheduler, 50, count_callback, &every_count);
    assert(after_id != 0 && every_id != 0);
    assert(fds_scheduler_every(&scheduler, 0, count_callback, &every_count) == 0);

    fds_scheduler_update(&scheduler, 49);
    assert(after_count.calls == 0 && every_count.calls == 0);

    fds_scheduler_update(&scheduler, 1);
    assert(after_count.calls == 0 && every_count.calls == 1);

    fds_scheduler_update(&scheduler, 49);
    assert(after_count.calls == 0 && every_count.calls == 1);

    fds_scheduler_update(&scheduler, 1);
    assert(after_count.calls == 1 && every_count.calls == 2);
    assert(!fds_scheduler_cancel(&scheduler, after_id));

    assert(fds_scheduler_cancel(&scheduler, every_id));
    fds_scheduler_update(&scheduler, 1000);
    assert(every_count.calls == 2);

    /* Large time jumps dispatch a repeating timer once and skip missed runs. */
    every_id = fds_scheduler_every(&scheduler, 50, count_callback, &every_count);
    assert(every_id != 0);
    fds_scheduler_update(&scheduler, 260);
    assert(every_count.calls == 3);
    fds_scheduler_update(&scheduler, 39);
    assert(every_count.calls == 3);
    fds_scheduler_update(&scheduler, 1);
    assert(every_count.calls == 4);
    assert(fds_scheduler_cancel(&scheduler, every_id));

    self_id = fds_scheduler_every(&scheduler, 10, cancel_self_callback, &self_cancel);
    assert(self_id != 0);
    fds_scheduler_update(&scheduler, 10);
    fds_scheduler_update(&scheduler, 10);
    assert(self_cancel.calls == 1);

    add_id = fds_scheduler_after(&scheduler, 0, add_from_callback, &add_data);
    assert(add_id != 0);
    fds_scheduler_update(&scheduler, 0);
    assert(add_data.calls == 1);
    assert(add_data.child_timer != 0);
    /* A timer created during update is intentionally deferred to the next update. */
    fds_scheduler_update(&scheduler, 0);
    assert(add_data.calls == 1);
    assert(add_data.child_counter.calls == 1);

    assert(!fds_scheduler_cancel(&scheduler, UINT64_MAX));
    fds_scheduler_destroy(&scheduler);
    puts("manual scheduler tests: OK");
}

#ifdef FDS_SCHEDULER_THREADS
static atomic_uint worker_timer_calls;

static void threaded_callback(FdsScheduler *scheduler, FdsScheduleId schedule, void *user_data)
{
    (void)scheduler;
    (void)schedule;
    (void)user_data;
    atomic_fetch_add_explicit(&worker_timer_calls, 1u, memory_order_relaxed);
}

static void sleep_ms(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000u);
    req.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&req, &req) != 0) {
    }
#endif
}

static void test_worker_scheduler(void)
{
    FdsScheduler scheduler;
    FdsScheduleId schedule;

    atomic_store(&worker_timer_calls, 0u);

    assert(fds_scheduler_init(&scheduler));
    assert(fds_scheduler_start_worker(&scheduler, 2));
    assert(!fds_scheduler_start_worker(&scheduler, 2));

    schedule = fds_scheduler_every(&scheduler, 5, threaded_callback, NULL);
    assert(schedule != 0);
    sleep_ms(70);
    assert(atomic_load_explicit(&worker_timer_calls, memory_order_relaxed) >= 3u);
    assert(fds_scheduler_cancel(&scheduler, schedule));
    sleep_ms(15);
    assert(fds_scheduler_stop_worker(&scheduler));
    assert(!fds_scheduler_worker_running(&scheduler));

    assert(atomic_load(&worker_timer_calls) >= 3u);
    fds_scheduler_destroy(&scheduler);
    puts("worker scheduler tests: OK");
}

#endif

int main(void)
{
    test_manual_scheduler();
#ifdef FDS_SCHEDULER_THREADS
    test_worker_scheduler();
#endif
    puts("all tests passed");
    return 0;
}
