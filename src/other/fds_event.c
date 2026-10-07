/////////////////////////////////////////////
////               Event              ///////
/////////////////////////////////////////////
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

// Event fuctions Start================================================================================================================
    // Ініціалізація та очищення
    FdsEventQueue fds_event_queue_init(FdsEvent *buffer, usize capacity);
    void fds_event_clear(FdsEventQueue *q);

    // Операції запису та читання
    bool fds_event_push(FdsEventQueue *q, FdsEvent event);
    bool fds_event_poll(FdsEventQueue *q, FdsEvent *out_event);
    usize fds_event_poll_many(FdsEventQueue *q, FdsEvent *events, usize capacity);
    bool fds_event_peek(const FdsEventQueue *q, FdsEvent *out_event);

    // Пропуск / видалення подій
    bool fds_event_discard(FdsEventQueue *q);
    usize fds_event_discard_many(FdsEventQueue *q, usize count);

    // Інспекція стану
    bool fds_event_is_empty(const FdsEventQueue *q);
    bool fds_event_is_full(const FdsEventQueue *q);
    usize fds_event_count(const FdsEventQueue *q);
    usize fds_event_capacity(const FdsEventQueue *q);
    usize fds_event_remaining(const FdsEventQueue *q);

    // Фабричні функції (створення з обнуленням union)
    FdsEvent fds_event_make(FdsEventType type, u64 timestamp);
    FdsEvent fds_event_make_i32(FdsEventType type, u64 timestamp, s32 value);
    FdsEvent fds_event_make_u32(FdsEventType type, u64 timestamp, u32 value);
    FdsEvent fds_event_make_f32(FdsEventType type, u64 timestamp, f32 value);
    FdsEvent fds_event_make_ptr(FdsEventType type, u64 timestamp, void *ptr);
    // Event fuctions End================================================================================================================
// Event functions Start ================================================================================================================
    FdsEventQueue fds_event_queue_init(FdsEvent *buffer, usize capacity)
    {
        FDS_ASSERT(buffer != NULL, "Event buffer pointer cannot be NULL");
        FDS_ASSERT(capacity > 0, "Queue capacity must be greater than 0");

        FdsEventQueue q zeroe;
        q.buffer = buffer;
        q.capacity = capacity;
        q.head = 0;
        q.tail = 0;
        q.count = 0;
        return q;
    }

    void fds_event_clear(FdsEventQueue *q)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        q->head = 0;
        q->tail = 0;
        q->count = 0;
    }

    bool fds_event_push(FdsEventQueue *q, FdsEvent event)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
        FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

        if (q->count >= q->capacity)
        {
            return false;
        }

        q->buffer[q->head] = event;
        q->head = (q->head + 1) % q->capacity;
        q->count++;

        return true;
    }

    bool fds_event_poll(FdsEventQueue *q, FdsEvent *out_event)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
        FDS_ASSERT(out_event != NULL, "Output event pointer is NULL");

        if (q->count == 0)
        {
            return false;
        }

        *out_event = q->buffer[q->tail];
        q->tail = (q->tail + 1) % q->capacity;
        q->count--;

        return true;
    }

    usize fds_event_poll_many(FdsEventQueue *q, FdsEvent *events, usize capacity)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
        if (capacity == 0)
            return 0;
        FDS_ASSERT(events != NULL, "Output events buffer is NULL");

        usize to_read = (capacity < q->count) ? capacity : q->count;
        for (usize i = 0; i < to_read; ++i)
        {
            events[i] = q->buffer[q->tail];
            q->tail = (q->tail + 1) % q->capacity;
        }
        q->count -= to_read;

        return to_read;
    }

    bool fds_event_peek(const FdsEventQueue *q, FdsEvent *out_event)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
        FDS_ASSERT(out_event != NULL, "Output event pointer is NULL");

        if (q->count == 0)
        {
            return false;
        }

        *out_event = q->buffer[q->tail];
        return true;
    }

    bool fds_event_discard(FdsEventQueue *q)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");

        if (q->count == 0)
        {
            return false;
        }

        q->tail = (q->tail + 1) % q->capacity;
        q->count--;
        return true;
    }

    usize fds_event_discard_many(FdsEventQueue *q, usize count)
    {
        FDS_ASSERT(q != NULL, "Queue pointer is NULL");
        FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");

        usize to_discard = (count < q->count) ? count : q->count;
        if (to_discard > 0)
        {
            q->tail = (q->tail + to_discard) % q->capacity;
            q->count -= to_discard;
        }

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
        return q->count >= q->capacity;
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

    // --- Factory Functions ---

    FdsEvent fds_event_make(FdsEventType type, u64 timestamp)
    {
        FdsEvent e  zeroe;
        e.type = type; e.timestamp = timestamp;
        return e;
    }

    FdsEvent fds_event_make_i32(FdsEventType type, u64 timestamp, s32 value)
    {
        FdsEvent e  zeroe;
        e.type = type; e.timestamp = timestamp;
        e.as.i32[0] = value;
        return e;
    }

    FdsEvent fds_event_make_u32(FdsEventType type, u64 timestamp, u32 value)
    {
        FdsEvent e  zeroe;
        e.type = type; e.timestamp = timestamp;
        e.as.u32[0] = value;
        return e;
    }

    FdsEvent fds_event_make_f32(FdsEventType type, u64 timestamp, f32 value)
    {
        FdsEvent e  zeroe;
        e.type = type; e.timestamp = timestamp;
        e.as.f32[0] = value;
        return e;
    }

    FdsEvent fds_event_make_ptr(FdsEventType type, u64 timestamp, void *ptr)
    {
        FdsEvent e zeroe;
        e.type = type;
        e.timestamp = timestamp;
        e.as.ptr = ptr;
        return e;
    }
    // Event functions End ================================================================================================================