#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"

#define FDS_EXT_EVENT_IMPL
#include "fds_ext_event.h"

// Типи подій підсистеми
typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY_DOWN,      // int32 (keycode)
    EVENT_MOUSE_MOVE,    // float (x pos)
    EVENT_WINDOW_RESIZE, // u32 (width, height)
    EVENT_LOG_MSG        // void* (pointer)
} CustomEventType;

int main(int argc, char **argv) {
    fds_cli_init(&argc, &argv);
    // 1. Виділяємо буфер під 16 подій прямо на стеку
    FdsEvent queue_buffer[16];
    FdsEventQueue queue = fds_event_queue_init(queue_buffer, 16);

    uint64_t current_frame = 1024; // Timestamp / Frame Index

    printf("=== 1. Заповнення черги подіями ===\n");

    // Пушимо події зручними фабріками
    fds_event_push(&queue, fds_event_make_i32(EVENT_KEY_DOWN, current_frame, 27)); // Escape
    fds_event_push(&queue, fds_event_make_f32(EVENT_MOUSE_MOVE, current_frame, 1920.0f));
    fds_event_push(&queue, fds_event_make_ptr(EVENT_LOG_MSG, current_frame, "Entity player spawned"));

    // Складена подія з кількома полями (Resize 1920x1080)
    FdsEvent resize_evt = fds_event_make(EVENT_WINDOW_RESIZE, current_frame);
    resize_evt.as.u32[0] = 1920;
    resize_evt.as.u32[1] = 1080;
    fds_event_push(&queue, resize_evt);

    // Інспекція стану черги
    printf("Кількість подій: %zu / %zu\n", fds_event_count(&queue), fds_event_capacity(&queue));
    printf("Залишилося вільного місця: %zu\n\n", fds_event_remaining(&queue));

    // 2. Batch Processing у Main Loop (poll_many)
    printf("=== 2. Обробка пачкою через fds_event_poll_many ===\n");

    FdsEvent frame_events[8];
    size_t count = fds_event_poll_many(&queue, frame_events, 8);

    for (size_t i = 0; i < count; ++i) {
        FdsEvent *e = &frame_events[i];

        switch (e->type) {
            case EVENT_KEY_DOWN:
                printf("[Frame %llu] Key Down: %d\n", e->timestamp, e->as.i32[0]);
                break;

            case EVENT_MOUSE_MOVE:
                printf("[Frame %llu] Mouse X: %.1f\n", e->timestamp, e->as.f32[0]);
                break;

            case EVENT_LOG_MSG:
                printf("[Frame %llu] Log: %s\n", e->timestamp, (const char *)e->as.ptr);
                break;

            case EVENT_WINDOW_RESIZE:
                printf("[Frame %llu] Resize: %ux%u\n", e->timestamp, e->as.u32[0], e->as.u32[1]);
                break;

            default:
                break;
        }
    }

    printf("\nЗалишилось подій після виклику poll_many: %zu\n\n", fds_event_count(&queue));

    // 3. Демонстрація скидання застарілих подій (discard_many)
    printf("=== 3. Пропуск (discard) небажаних подій ===\n");

    fds_event_push(&queue, fds_event_make_i32(EVENT_KEY_DOWN, current_frame + 1, 32)); // Space
    fds_event_push(&queue, fds_event_make_i32(EVENT_KEY_DOWN, current_frame + 1, 13)); // Enter

    printf("Подій у черзі до discard: %zu\n", fds_event_count(&queue));
    size_t discarded = fds_event_discard_many(&queue, 2);
    printf("Скинуто подій: %zu\n", discarded);
    printf("Подій у черзі після discard: %zu\n", fds_event_count(&queue));

    return 0;
}