#define FDS_IMPLEMENTATION
#include "fds.h"

typedef struct { int x; int y; } Vector2;

int main(void) {
    Arena arena = arena_create(256);

    printf("=== Базове виділення ===\n");
    int *a = arena_new(&arena, int);
    *a = 42;
    printf("a = %d\n", *a);
    printf("used = %zu, reserved = %zu\n\n", arena_used(&arena), arena_reserved(&arena));

    printf("=== Велике виділення ===\n");
    char *big = arena_alloc(&arena, 1024);
    memset(big, 'B', 1024);
    printf("big is %s in arena\n", arena_contains(&arena, big) ? "inside" : "outside");
    printf("used = %zu\n\n", arena_used(&arena));

    printf("=== Стабільність вказівників ===\n");
    Vector2 *v = arena_new(&arena, Vector2);
    v->x = 10; v->y = 20;
    arena_alloc(&arena, 512);
    printf("v = (%d, %d)\n\n", v->x, v->y);

    printf("=== Маркер та відновлення ===\n");
    ArenaMark mark = arena_mark(&arena);
    int *temp = arena_array(&arena, int, 100);
    for (int i = 0; i < 100; i++) temp[i] = i;
    printf("used до відновлення: %zu\n", arena_used(&arena));
    arena_restore(&arena, mark);
    printf("used після відновлення: %zu\n\n", arena_used(&arena));

    printf("=== Область видимості (scope) ===\n");
    {
        ArenaScope scope = arena_scope_begin(&arena);
        char *msg = arena_strdup(&arena, "тимчасовий рядок");
        printf("%s\n", msg);
        arena_scope_end(&scope);
    }
    printf("used після scope: %zu\n\n", arena_used(&arena));

    printf("=== Резервування та commit ===\n");
    ArenaReservation res = arena_reserve(&arena, 64, 8);
    char *buf = arena_reservation_ptr(&res);
    memcpy(buf, "Hello, arena!", 13);
    // ніяких інших виділень між reserve та commit!
    arena_commit(&res, 13);
    printf("buffer: %s\n", buf);
    printf("used після commit: %zu\n\n", arena_used(&arena));

    printf("=== Очищення кешу ===\n");
    size_t before = arena_reserved(&arena);
    arena_trim_cache(&arena);
    printf("reserved до очищення: %zu, після: %zu\n", before, arena_reserved(&arena));

    printf("\n=== Статистика ===\n");
    ArenaStats stats = arena_stats(&arena);
    printf("used        = %zu\n", stats.used);
    printf("reserved    = %zu\n", stats.reserved);
    printf("chunks      = %zu\n", stats.chunks);
    printf("peak_used   = %zu\n", stats.peak_used);
    printf("peak_chunks = %zu\n", stats.peak_chunks);
    printf("allocations = %zu\n", stats.allocations);

    arena_free(&arena);
    return 0;
}