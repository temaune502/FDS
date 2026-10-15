#ifndef FDS_EVENT_H
#define FDS_EVENT_H

typedef u32 FdsEventType;

typedef struct
{
    FdsEventType type;
    u64 timestamp;

    union
    {
        s32 i32[4];
        u32 u32[4];
        f32 f32[4];
        u64 u64[2];
        void *ptr;
    } as;
} FdsEvent;

typedef struct
{
    FdsEvent *buffer;
    usize capacity;
    usize head;
    usize tail;
    usize count;
} FdsEventQueue;

/* Event queue */
FdsEventQueue fds_event_queue_init(FdsEvent *buffer, usize capacity);
void fds_event_clear(FdsEventQueue *q);

bool fds_event_push(FdsEventQueue *q, FdsEvent event);
usize fds_event_push_many(FdsEventQueue *q, const FdsEvent *events, usize count);

bool fds_event_poll(FdsEventQueue *q, FdsEvent *out_event);
usize fds_event_poll_many(FdsEventQueue *q, FdsEvent *events, usize capacity);

bool fds_event_peek(const FdsEventQueue *q, FdsEvent *out_event);
usize fds_event_peek_many(const FdsEventQueue *q, FdsEvent *events, usize capacity);

bool fds_event_discard(FdsEventQueue *q);
usize fds_event_discard_many(FdsEventQueue *q, usize count);

bool fds_event_is_empty(const FdsEventQueue *q);
bool fds_event_is_full(const FdsEventQueue *q);
usize fds_event_count(const FdsEventQueue *q);
usize fds_event_capacity(const FdsEventQueue *q);
usize fds_event_remaining(const FdsEventQueue *q);

/* Event factories */
FdsEvent fds_event_make(FdsEventType type, u64 timestamp);
FdsEvent fds_event_make_i32(FdsEventType type, u64 timestamp, s32 value);
FdsEvent fds_event_make_u32(FdsEventType type, u64 timestamp, u32 value);
FdsEvent fds_event_make_f32(FdsEventType type, u64 timestamp, f32 value);
FdsEvent fds_event_make_ptr(FdsEventType type, u64 timestamp, void *ptr);
FdsEvent fds_event_make_i32v(FdsEventType type, u64 timestamp, const s32 value[4]);
FdsEvent fds_event_make_u32v(FdsEventType type, u64 timestamp, const u32 value[4]);
FdsEvent fds_event_make_f32v(FdsEventType type, u64 timestamp, const f32 value[4]);
FdsEvent fds_event_make_u64v(FdsEventType type, u64 timestamp, const u64 value[2]);

#ifdef FDS_EVENT_IMPL

static usize fds_event_advance(usize position, usize amount, usize capacity)
{
    if (amount >= capacity - position)
        return amount - (capacity - position);

    return position + amount;
}

FdsEventQueue fds_event_queue_init(FdsEvent *buffer, usize capacity)
{
    FDS_ASSERT(buffer != NULL, "Event buffer pointer cannot be NULL");
    FDS_ASSERT(capacity > 0, "Queue capacity must be greater than 0");

    FdsEventQueue q zeroe;
    q.buffer = buffer;
    q.capacity = capacity;

    return q;
}

void fds_event_clear(FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool fds_event_push(FdsEventQueue *q, FdsEvent event)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    if (q->count == q->capacity)
        return false;

    q->buffer[q->head] = event;
    q->head++;

    if (q->head == q->capacity)
        q->head = 0;

    q->count++;
    return true;
}

usize fds_event_push_many(FdsEventQueue *q, const FdsEvent *events, usize count)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    if (count == 0)
        return 0;

    FDS_ASSERT(events != NULL, "Input events buffer is NULL");

    usize available = q->capacity - q->count;
    usize to_write = count < available ? count : available;

    for (usize i = 0; i < to_write; ++i)
    {
        q->buffer[q->head] = events[i];
        q->head++;

        if (q->head == q->capacity)
            q->head = 0;
    }

    q->count += to_write;
    return to_write;
}

bool fds_event_poll(FdsEventQueue *q, FdsEvent *out_event)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");
    FDS_ASSERT(out_event != NULL, "Output event pointer is NULL");

    if (q->count == 0)
        return false;

    *out_event = q->buffer[q->tail];

    q->tail++;
    if (q->tail == q->capacity)
        q->tail = 0;

    q->count--;
    return true;
}

usize fds_event_poll_many(FdsEventQueue *q, FdsEvent *events, usize capacity)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    if (capacity == 0)
        return 0;

    FDS_ASSERT(events != NULL, "Output events buffer is NULL");

    usize to_read = capacity < q->count ? capacity : q->count;

    for (usize i = 0; i < to_read; ++i)
    {
        events[i] = q->buffer[q->tail];
        q->tail++;

        if (q->tail == q->capacity)
            q->tail = 0;
    }

    q->count -= to_read;
    return to_read;
}

bool fds_event_peek(const FdsEventQueue *q, FdsEvent *out_event)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");
    FDS_ASSERT(out_event != NULL, "Output event pointer is NULL");

    if (q->count == 0)
        return false;

    *out_event = q->buffer[q->tail];
    return true;
}

usize fds_event_peek_many(const FdsEventQueue *q, FdsEvent *events, usize capacity)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    if (capacity == 0)
        return 0;

    FDS_ASSERT(events != NULL, "Output events buffer is NULL");

    usize to_read = capacity < q->count ? capacity : q->count;
    usize index = q->tail;

    for (usize i = 0; i < to_read; ++i)
    {
        events[i] = q->buffer[index];
        index++;

        if (index == q->capacity)
            index = 0;
    }

    return to_read;
}

bool fds_event_discard(FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    if (q->count == 0)
        return false;

    q->tail++;
    if (q->tail == q->capacity)
        q->tail = 0;

    q->count--;
    return true;
}

usize fds_event_discard_many(FdsEventQueue *q, usize count)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    usize to_discard = count < q->count ? count : q->count;

    if (to_discard == 0)
        return 0;

    q->tail = fds_event_advance(q->tail, to_discard, q->capacity);
    q->count -= to_discard;

    return to_discard;
}

bool fds_event_is_empty(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->count == 0;
}

bool fds_event_is_full(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->count == q->capacity;
}

usize fds_event_count(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->count;
}

usize fds_event_capacity(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->capacity;
}

usize fds_event_remaining(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->capacity - q->count;
}

FdsEvent fds_event_make(FdsEventType type, u64 timestamp)
{
    FdsEvent e zeroe;
    e.type = type;
    e.timestamp = timestamp;
    return e;
}

FdsEvent fds_event_make_i32(FdsEventType type, u64 timestamp, s32 value)
{
    FdsEvent e = fds_event_make(type, timestamp);
    e.as.i32[0] = value;
    return e;
}

FdsEvent fds_event_make_u32(FdsEventType type, u64 timestamp, u32 value)
{
    FdsEvent e = fds_event_make(type, timestamp);
    e.as.u32[0] = value;
    return e;
}

FdsEvent fds_event_make_f32(FdsEventType type, u64 timestamp, f32 value)
{
    FdsEvent e = fds_event_make(type, timestamp);
    e.as.f32[0] = value;
    return e;
}

FdsEvent fds_event_make_ptr(FdsEventType type, u64 timestamp, void *ptr)
{
    FdsEvent e = fds_event_make(type, timestamp);
    e.as.ptr = ptr;
    return e;
}

FdsEvent fds_event_make_i32v(FdsEventType type, u64 timestamp, const s32 value[4])
{
    FDS_ASSERT(value != NULL, "Event i32 array is NULL");

    FdsEvent e = fds_event_make(type, timestamp);

    for (usize i = 0; i < 4; ++i)
        e.as.i32[i] = value[i];

    return e;
}

FdsEvent fds_event_make_u32v(FdsEventType type, u64 timestamp, const u32 value[4])
{
    FDS_ASSERT(value != NULL, "Event u32 array is NULL");

    FdsEvent e = fds_event_make(type, timestamp);

    for (usize i = 0; i < 4; ++i)
        e.as.u32[i] = value[i];

    return e;
}

FdsEvent fds_event_make_f32v(FdsEventType type, u64 timestamp, const f32 value[4])
{
    FDS_ASSERT(value != NULL, "Event f32 array is NULL");

    FdsEvent e = fds_event_make(type, timestamp);

    for (usize i = 0; i < 4; ++i)
        e.as.f32[i] = value[i];

    return e;
}

FdsEvent fds_event_make_u64v(FdsEventType type, u64 timestamp, const u64 value[2])
{
    FDS_ASSERT(value != NULL, "Event u64 array is NULL");

    FdsEvent e = fds_event_make(type, timestamp);

    for (usize i = 0; i < 2; ++i)
        e.as.u64[i] = value[i];

    return e;
}

#endif

#endif