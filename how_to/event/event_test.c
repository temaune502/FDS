#include <stdio.h>
#include <string.h>

/*
    Важливо:
    FDS_EVENT_IMPL має бути визначений ДО підключення fds_event.h.
    Якщо fds.h сам підключає fds_event.h, це теж спрацює.

    gcc -std=c23 -Wall -Wextra -Werror -pedantic src\event_test.c -o test_fds_event.exe
*/
#define FDS_IMPL
#define FDS_EVENT_IMPL
#include "fds.h"
#include "fds_event.h"

#define TEST_CHECK(condition)                                             \
    do {                                                                   \
        if (!(condition)) {                                                \
            printf("    FAILED: %s (%s:%d)\n",                             \
                   #condition, __FILE__, __LINE__);                       \
            return false;                                                  \
        }                                                                  \
    } while (0)

#define TEST_RUN(name)                                                     \
    do {                                                                   \
        printf("[TEST] %s\n", #name);                                      \
        if (!(name())) {                                                   \
            printf("  -> FAILED\n\n");                                    \
            ++g_failed;                                                    \
        } else {                                                           \
            printf("  -> OK\n\n");                                         \
            ++g_passed;                                                    \
        }                                                                  \
    } while (0)

static int g_passed = 0;
static int g_failed = 0;


/* -------------------------------------------------------------
   Helpers
   ------------------------------------------------------------- */

static bool event_equal(const FdsEvent *a, const FdsEvent *b)
{
    if (a->type != b->type)
        return false;

    if (a->timestamp != b->timestamp)
        return false;

    /*
        Усі тестові події створюються через фабрики, які
        обнуляють структуру перед заповненням.
    */
    return memcmp(&a->as, &b->as, sizeof(a->as)) == 0;
}

static FdsEvent test_event(u32 id)
{
    return fds_event_make_i32(
        0x1000u + id,
        0x100000000ULL + id,
        (s32)(id * 10 + 7)
    );
}


/*
    Простий детермінований PRNG для stress-test.
    Ніякої залежності від rand().
*/
static u32 rng_state = 0x12345678u;

static u32 rng_next(void)
{
    u32 x = rng_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    rng_state = x;
    return x;
}


/* -------------------------------------------------------------
   Initialization / state
   ------------------------------------------------------------- */

static bool test_init(void)
{
    FdsEvent buffer[4];

    FdsEventQueue q = fds_event_queue_init(buffer, 4);

    TEST_CHECK(q.buffer == buffer);
    TEST_CHECK(q.capacity == 4);
    TEST_CHECK(q.head == 0);
    TEST_CHECK(q.tail == 0);
    TEST_CHECK(q.count == 0);

    TEST_CHECK(fds_event_is_empty(&q));
    TEST_CHECK(!fds_event_is_full(&q));
    TEST_CHECK(fds_event_count(&q) == 0);
    TEST_CHECK(fds_event_capacity(&q) == 4);
    TEST_CHECK(fds_event_remaining(&q) == 4);

    return true;
}


/* -------------------------------------------------------------
   Event factories
   ------------------------------------------------------------- */

static bool test_factories(void)
{
    FdsEvent e;

    e = fds_event_make(1, 1234);

    TEST_CHECK(e.type == 1);
    TEST_CHECK(e.timestamp == 1234);

    for (usize i = 0; i < 4; ++i)
        TEST_CHECK(e.as.u32[i] == 0);

    e = fds_event_make_i32(10, 100, -123);

    TEST_CHECK(e.type == 10);
    TEST_CHECK(e.timestamp == 100);
    TEST_CHECK(e.as.i32[0] == -123);
    TEST_CHECK(e.as.i32[1] == 0);
    TEST_CHECK(e.as.i32[2] == 0);
    TEST_CHECK(e.as.i32[3] == 0);

    e = fds_event_make_u32(11, 101, 0xDEADBEEFu);

    TEST_CHECK(e.type == 11);
    TEST_CHECK(e.timestamp == 101);
    TEST_CHECK(e.as.u32[0] == 0xDEADBEEFu);
    TEST_CHECK(e.as.u32[1] == 0);
    TEST_CHECK(e.as.u32[2] == 0);
    TEST_CHECK(e.as.u32[3] == 0);

    e = fds_event_make_f32(12, 102, 3.1415926f);

    TEST_CHECK(e.type == 12);
    TEST_CHECK(e.timestamp == 102);
    TEST_CHECK(e.as.f32[0] == 3.1415926f);
    TEST_CHECK(e.as.u32[1] == 0);
    TEST_CHECK(e.as.u32[2] == 0);
    TEST_CHECK(e.as.u32[3] == 0);

    int value = 123;

    e = fds_event_make_ptr(13, 103, &value);

    TEST_CHECK(e.type == 13);
    TEST_CHECK(e.timestamp == 103);
    TEST_CHECK(e.as.ptr == &value);

    {
        s32 source[4] = { 1, 2, 3, 4 };

        e = fds_event_make_i32v(20, 200, source);

        TEST_CHECK(e.type == 20);
        TEST_CHECK(e.timestamp == 200);

        for (usize i = 0; i < 4; ++i)
            TEST_CHECK(e.as.i32[i] == source[i]);

        source[0] = 99;
        TEST_CHECK(e.as.i32[0] == 1);
    }

    {
        u32 source[4] = {
            0x11111111u,
            0x22222222u,
            0x33333333u,
            0x44444444u
        };

        e = fds_event_make_u32v(21, 201, source);

        for (usize i = 0; i < 4; ++i)
            TEST_CHECK(e.as.u32[i] == source[i]);
    }

    {
        f32 source[4] = {
            1.0f,
            2.0f,
            3.0f,
            4.0f
        };

        e = fds_event_make_f32v(22, 202, source);

        for (usize i = 0; i < 4; ++i)
            TEST_CHECK(e.as.f32[i] == source[i]);
    }

    {
        u64 source[2] = {
            0x1122334455667788ULL,
            0x8877665544332211ULL
        };

        e = fds_event_make_u64v(23, 203, source);

        for (usize i = 0; i < 2; ++i)
            TEST_CHECK(e.as.u64[i] == source[i]);
    }

    return true;
}


/* -------------------------------------------------------------
   Basic push / poll
   ------------------------------------------------------------- */

static bool test_push_poll(void)
{
    FdsEvent buffer[4];
    FdsEventQueue q = fds_event_queue_init(buffer, 4);

    FdsEvent a = test_event(1);
    FdsEvent b = test_event(2);
    FdsEvent c = test_event(3);
    FdsEvent d = test_event(4);
    FdsEvent e = test_event(5);

    TEST_CHECK(fds_event_push(&q, a));
    TEST_CHECK(fds_event_push(&q, b));
    TEST_CHECK(fds_event_push(&q, c));
    TEST_CHECK(fds_event_push(&q, d));

    TEST_CHECK(!fds_event_push(&q, e));

    TEST_CHECK(fds_event_is_full(&q));
    TEST_CHECK(fds_event_count(&q) == 4);
    TEST_CHECK(fds_event_remaining(&q) == 0);

    FdsEvent out;

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &a));

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &b));

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &c));

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &d));

    TEST_CHECK(!fds_event_poll(&q, &out));
    TEST_CHECK(fds_event_is_empty(&q));
    TEST_CHECK(fds_event_count(&q) == 0);

    return true;
}


/* -------------------------------------------------------------
   Peek
   ------------------------------------------------------------- */

static bool test_peek(void)
{
    FdsEvent buffer[4];
    FdsEventQueue q = fds_event_queue_init(buffer, 4);

    FdsEvent events[3] = {
        test_event(10),
        test_event(11),
        test_event(12)
    };

    TEST_CHECK(fds_event_push_many(&q, events, 3) == 3);

    usize old_count = q.count;
    usize old_head = q.head;
    usize old_tail = q.tail;

    FdsEvent out;

    TEST_CHECK(fds_event_peek(&q, &out));
    TEST_CHECK(event_equal(&out, &events[0]));

    /*
        peek не має змінювати стан.
    */
    TEST_CHECK(q.count == old_count);
    TEST_CHECK(q.head == old_head);
    TEST_CHECK(q.tail == old_tail);

    FdsEvent peeked[8];

    usize n = fds_event_peek_many(&q, peeked, 8);

    TEST_CHECK(n == 3);
    TEST_CHECK(q.count == old_count);
    TEST_CHECK(q.head == old_head);
    TEST_CHECK(q.tail == old_tail);

    for (usize i = 0; i < 3; ++i)
        TEST_CHECK(event_equal(&peeked[i], &events[i]));

    TEST_CHECK(fds_event_peek_many(&q, peeked, 0) == 0);
    TEST_CHECK(fds_event_peek_many(&q, peeked, 1) == 1);

    FdsEvent empty_buffer[2];
    FdsEventQueue empty_q = fds_event_queue_init(empty_buffer, 2);

    TEST_CHECK(!fds_event_peek(&empty_q, &out));
    TEST_CHECK(fds_event_peek_many(&empty_q, peeked, 8) == 0);

    return true;
}


/* -------------------------------------------------------------
   Wrap-around
   ------------------------------------------------------------- */

static bool test_wraparound(void)
{
    FdsEvent buffer[4];
    FdsEventQueue q = fds_event_queue_init(buffer, 4);

    FdsEvent events[6];

    for (u32 i = 0; i < 6; ++i)
        events[i] = test_event(100 + i);

    /*
        [0 1 2 3]
        poll 0,1
        head/tail тепер знаходяться всередині буфера.
        push 4,5 примусить head перейти через кінець.
    */
    TEST_CHECK(fds_event_push(&q, events[0]));
    TEST_CHECK(fds_event_push(&q, events[1]));
    TEST_CHECK(fds_event_push(&q, events[2]));
    TEST_CHECK(fds_event_push(&q, events[3]));

    FdsEvent out;

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &events[0]));

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &events[1]));

    TEST_CHECK(fds_event_push(&q, events[4]));
    TEST_CHECK(fds_event_push(&q, events[5]));

    TEST_CHECK(fds_event_count(&q) == 4);
    TEST_CHECK(fds_event_is_full(&q));

    FdsEvent expected[4] = {
        events[2],
        events[3],
        events[4],
        events[5]
    };

    FdsEvent peeked[4];

    TEST_CHECK(fds_event_peek_many(&q, peeked, 4) == 4);

    for (usize i = 0; i < 4; ++i)
        TEST_CHECK(event_equal(&peeked[i], &expected[i]));

    TEST_CHECK(fds_event_poll_many(&q, peeked, 4) == 4);

    for (usize i = 0; i < 4; ++i)
        TEST_CHECK(event_equal(&peeked[i], &expected[i]));

    TEST_CHECK(fds_event_is_empty(&q));

    return true;
}


/* -------------------------------------------------------------
   push_many / poll_many
   ------------------------------------------------------------- */

static bool test_many(void)
{
    FdsEvent buffer[5];
    FdsEventQueue q = fds_event_queue_init(buffer, 5);

    FdsEvent events[8];

    for (u32 i = 0; i < 8; ++i)
        events[i] = test_event(200 + i);

    TEST_CHECK(fds_event_push_many(&q, events, 3) == 3);
    TEST_CHECK(fds_event_count(&q) == 3);

    TEST_CHECK(fds_event_push_many(&q, events + 3, 8) == 2);
    TEST_CHECK(fds_event_count(&q) == 5);
    TEST_CHECK(fds_event_is_full(&q));

    FdsEvent out[8];

    TEST_CHECK(fds_event_poll_many(&q, out, 2) == 2);
    TEST_CHECK(event_equal(&out[0], &events[0]));
    TEST_CHECK(event_equal(&out[1], &events[1]));

    TEST_CHECK(fds_event_count(&q) == 3);

    /*
        Тепер у черзі:
        2, 3, 4
    */

    TEST_CHECK(fds_event_push_many(&q, events + 5, 3) == 2);
    TEST_CHECK(fds_event_count(&q) == 5);

    {
        FdsEvent expected[5] = {
            events[2],
            events[3],
            events[4],
            events[5],
            events[6]
        };

        TEST_CHECK(fds_event_peek_many(&q, out, 5) == 5);

        for (usize i = 0; i < 5; ++i)
            TEST_CHECK(event_equal(&out[i], &expected[i]));
    }

    TEST_CHECK(fds_event_poll_many(&q, out, 100) == 5);
    TEST_CHECK(fds_event_is_empty(&q));

    TEST_CHECK(fds_event_push_many(&q, events, 0) == 0);
    TEST_CHECK(fds_event_poll_many(&q, out, 0) == 0);
    TEST_CHECK(fds_event_peek_many(&q, out, 0) == 0);

    return true;
}


/* -------------------------------------------------------------
   discard / discard_many
   ------------------------------------------------------------- */

static bool test_discard(void)
{
    FdsEvent buffer[5];
    FdsEventQueue q = fds_event_queue_init(buffer, 5);

    FdsEvent events[5];

    for (u32 i = 0; i < 5; ++i)
        events[i] = test_event(300 + i);

    TEST_CHECK(fds_event_push_many(&q, events, 5) == 5);

    TEST_CHECK(fds_event_discard(&q));
    TEST_CHECK(fds_event_count(&q) == 4);

    {
        FdsEvent out;
        TEST_CHECK(fds_event_peek(&q, &out));
        TEST_CHECK(event_equal(&out, &events[1]));
    }

    TEST_CHECK(fds_event_discard_many(&q, 2) == 2);
    TEST_CHECK(fds_event_count(&q) == 2);

    {
        FdsEvent out;
        TEST_CHECK(fds_event_peek(&q, &out));
        TEST_CHECK(event_equal(&out, &events[3]));
    }

    TEST_CHECK(fds_event_discard_many(&q, 100) == 2);
    TEST_CHECK(fds_event_is_empty(&q));

    TEST_CHECK(!fds_event_discard(&q));
    TEST_CHECK(fds_event_discard_many(&q, 0) == 0);

    return true;
}


/* -------------------------------------------------------------
   Clear
   ------------------------------------------------------------- */

static bool test_clear(void)
{
    FdsEvent buffer[8];
    FdsEventQueue q = fds_event_queue_init(buffer, 8);

    FdsEvent events[6];

    for (u32 i = 0; i < 6; ++i)
        events[i] = test_event(400 + i);

    TEST_CHECK(fds_event_push_many(&q, events, 6) == 6);

    FdsEvent out;

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(fds_event_poll(&q, &out));

    TEST_CHECK(fds_event_push(&q, events[0]));
    TEST_CHECK(fds_event_push(&q, events[1]));

    /*
        Тут уже гарантовано зміщені head/tail.
    */
    TEST_CHECK(q.count != 0);

    fds_event_clear(&q);

    TEST_CHECK(q.head == 0);
    TEST_CHECK(q.tail == 0);
    TEST_CHECK(q.count == 0);

    TEST_CHECK(fds_event_is_empty(&q));
    TEST_CHECK(fds_event_remaining(&q) == 8);

    TEST_CHECK(fds_event_push(&q, events[5]));

    TEST_CHECK(fds_event_poll(&q, &out));
    TEST_CHECK(event_equal(&out, &events[5]));

    return true;
}


/* -------------------------------------------------------------
   Capacity == 1
   ------------------------------------------------------------- */

static bool test_capacity_one(void)
{
    FdsEvent buffer[1];
    FdsEventQueue q = fds_event_queue_init(buffer, 1);

    for (u32 i = 0; i < 100; ++i)
    {
        FdsEvent in = test_event(500 + i);
        FdsEvent out;

        TEST_CHECK(fds_event_is_empty(&q));
        TEST_CHECK(fds_event_push(&q, in));
        TEST_CHECK(fds_event_is_full(&q));

        TEST_CHECK(!fds_event_push(&q, test_event(10000 + i)));

        TEST_CHECK(fds_event_peek(&q, &out));
        TEST_CHECK(event_equal(&out, &in));

        TEST_CHECK(fds_event_poll(&q, &out));
        TEST_CHECK(event_equal(&out, &in));

        TEST_CHECK(fds_event_is_empty(&q));
    }

    return true;
}


/* -------------------------------------------------------------
   Stress-test
   ------------------------------------------------------------- */

#define STRESS_CAPACITY 17
#define STRESS_OPERATIONS 100000

static bool validate_queue(
    FdsEventQueue *q,
    FdsEvent *reference,
    usize reference_count
)
{
    TEST_CHECK(fds_event_count(q) == reference_count);
    TEST_CHECK(fds_event_remaining(q) ==
               STRESS_CAPACITY - reference_count);

    TEST_CHECK(fds_event_is_empty(q) == (reference_count == 0));
    TEST_CHECK(fds_event_is_full(q) == (reference_count == STRESS_CAPACITY));

    FdsEvent snapshot[STRESS_CAPACITY];

    usize n = fds_event_peek_many(
        q,
        snapshot,
        STRESS_CAPACITY
    );

    TEST_CHECK(n == reference_count);

    for (usize i = 0; i < reference_count; ++i)
        TEST_CHECK(event_equal(&snapshot[i], &reference[i]));

    return true;
}

static bool test_stress(void)
{
    FdsEvent buffer[STRESS_CAPACITY];
    FdsEventQueue q =
        fds_event_queue_init(buffer, STRESS_CAPACITY);

    FdsEvent reference[STRESS_CAPACITY];
    usize reference_count = 0;

    rng_state = 0x12345678u;

    for (usize iteration = 0;
         iteration < STRESS_OPERATIONS;
         ++iteration)
    {
        u32 operation = rng_next() % 8;

        switch (operation)
        {
            /* push */
            case 0:
            {
                FdsEvent event = test_event(
                    (u32)iteration + 100000
                );

                bool expected =
                    reference_count < STRESS_CAPACITY;

                bool actual =
                    fds_event_push(&q, event);

                TEST_CHECK(actual == expected);

                if (expected)
                {
                    reference[reference_count++] = event;
                }

                break;
            }

            /* push_many */
            case 1:
            {
                FdsEvent input[8];

                usize amount = rng_next() % 8;

                for (usize i = 0; i < amount; ++i)
                {
                    input[i] = test_event(
                        (u32)iteration * 8u + (u32)i + 200000
                    );
                }

                usize available =
                    STRESS_CAPACITY - reference_count;

                usize expected =
                    amount < available ? amount : available;

                usize actual =
                    fds_event_push_many(
                        &q,
                        input,
                        amount
                    );

                TEST_CHECK(actual == expected);

                for (usize i = 0; i < expected; ++i)
                    reference[reference_count++] = input[i];

                break;
            }

            /* poll */
            case 2:
            {
                FdsEvent actual_event;
                bool actual =
                    fds_event_poll(&q, &actual_event);

                bool expected = reference_count != 0;

                TEST_CHECK(actual == expected);

                if (expected)
                {
                    TEST_CHECK(
                        event_equal(
                            &actual_event,
                            &reference[0]
                        )
                    );

                    memmove(
                        reference,
                        reference + 1,
                        (reference_count - 1) *
                            sizeof(reference[0])
                    );

                    --reference_count;
                }

                break;
            }

            /* poll_many */
            case 3:
            {
                FdsEvent output[8];

                usize amount = rng_next() % 8;

                usize expected =
                    amount < reference_count
                        ? amount
                        : reference_count;

                usize actual =
                    fds_event_poll_many(
                        &q,
                        output,
                        amount
                    );

                TEST_CHECK(actual == expected);

                for (usize i = 0; i < expected; ++i)
                    TEST_CHECK(
                        event_equal(
                            &output[i],
                            &reference[i]
                        )
                    );

                if (expected != 0)
                {
                    memmove(
                        reference,
                        reference + expected,
                        (reference_count - expected) *
                            sizeof(reference[0])
                    );

                    reference_count -= expected;
                }

                break;
            }

            /* peek */
            case 4:
            {
                FdsEvent output;
                bool actual =
                    fds_event_peek(&q, &output);

                bool expected = reference_count != 0;

                TEST_CHECK(actual == expected);

                if (expected)
                {
                    TEST_CHECK(
                        event_equal(
                            &output,
                            &reference[0]
                        )
                    );
                }

                break;
            }

            /* peek_many */
            case 5:
            {
                FdsEvent output[8];

                usize amount = rng_next() % 8;

                usize expected =
                    amount < reference_count
                        ? amount
                        : reference_count;

                usize old_count = reference_count;

                usize actual =
                    fds_event_peek_many(
                        &q,
                        output,
                        amount
                    );

                TEST_CHECK(actual == expected);
                TEST_CHECK(fds_event_count(&q) == old_count);

                for (usize i = 0; i < expected; ++i)
                    TEST_CHECK(
                        event_equal(
                            &output[i],
                            &reference[i]
                        )
                    );

                break;
            }

            /* discard */
            case 6:
            {
                bool actual =
                    fds_event_discard(&q);

                bool expected = reference_count != 0;

                TEST_CHECK(actual == expected);

                if (expected)
                {
                    memmove(
                        reference,
                        reference + 1,
                        (reference_count - 1) *
                            sizeof(reference[0])
                    );

                    --reference_count;
                }

                break;
            }

            /* discard_many / occasional clear */
            case 7:
            {
                /*
                    Приблизно кожні 64 операції тестуємо clear.
                */
                if ((rng_next() & 63u) == 0)
                {
                    fds_event_clear(&q);

                    reference_count = 0;
                }
                else
                {
                    usize amount = rng_next() % 8;

                    usize expected =
                        amount < reference_count
                            ? amount
                            : reference_count;

                    usize actual =
                        fds_event_discard_many(
                            &q,
                            amount
                        );

                    TEST_CHECK(actual == expected);

                    if (expected != 0)
                    {
                        memmove(
                            reference,
                            reference + expected,
                            (reference_count - expected) *
                                sizeof(reference[0])
                        );

                        reference_count -= expected;
                    }
                }

                break;
            }
        }

        /*
            Перевіряємо чергу після КОЖНОЇ операції.
            Тобто stress-test не просто генерує навантаження,
            а кожного разу порівнює FDS із еталонною чергою.
        */
        if (!validate_queue(
                &q,
                reference,
                reference_count))
        {
            printf(
                "    Stress failure at iteration %zu\n",
                iteration
            );

            return false;
        }
    }

    return true;
}


/* -------------------------------------------------------------
   Main
   ------------------------------------------------------------- */

int main(void)
{
    printf("=== FDS Event tests ===\n\n");

    TEST_RUN(test_init);
    TEST_RUN(test_factories);
    TEST_RUN(test_push_poll);
    TEST_RUN(test_peek);
    TEST_RUN(test_wraparound);
    TEST_RUN(test_many);
    TEST_RUN(test_discard);
    TEST_RUN(test_clear);
    TEST_RUN(test_capacity_one);
    TEST_RUN(test_stress);

    printf("================================\n");
    printf("Passed: %d\n", g_passed);
    printf("Failed: %d\n", g_failed);
    printf("================================\n");

    return g_failed != 0 ? 1 : 0;
}