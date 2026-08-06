#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>   // стандартний assert.h

#define FDS_IMPLEMENTATION
#include "fds.h"


typedef struct {
    int x;
    int y;
} Vector2d;

typedef struct {
    Vector2d *items;
    size_t count;
    size_t capacity;
} DynamicArray;
void print_array(DynamicArray da) {
    printf("count=%zu capacity=%zu : [", da.count, da.capacity);
    for (size_t i = 0; i < da.count; i++) {
        printf("(%d,%d)", da.items[i].x, da.items[i].y);
        if (i + 1 != da.count)
            printf(", ");
    }
    printf("]\n");
}

// ============================================================
// Демонстраційний main
// ============================================================
int main(void) {
    DynamicArray da = {0};

    printf("========== EMPTY ==========\n");
    assert(da_empty(&da));

    printf("========== PUSH ==========\n");
    for (int i = 0; i < 10; i++) {
        da_push(&da, ((Vector2d){ i, i * 10 }));
    }
    print_array(da);
    assert(da.count == 10);
    assert(!da_empty(&da));

    printf("\n========== FRONT / BACK ==========\n");
    assert(da_front(&da).x == 0);
    assert(da_front(&da).y == 0);
    assert(da_back(&da).x == 9);
    assert(da_back(&da).y == 90);
    printf("front = (%d,%d)\n", da_front(&da).x, da_front(&da).y);
    printf("back  = (%d,%d)\n", da_back(&da).x, da_back(&da).y);

    printf("\n========== FOREACH ==========\n");
    da_foreach(Vector2d, it, &da) {
        printf("(%d,%d) ", it->x, it->y);
    }
    printf("\n");

    printf("\n========== FOREACH REVERSE ==========\n");
    da_foreach_reverse(Vector2d, it, &da) {
        printf("(%d,%d) ", it->x, it->y);
    }
    printf("\n");

    printf("\n========== INSERT ==========\n");
    da_insert(&da, 5, ((Vector2d){ 100, 200 }));
    print_array(da);
    assert(da.count == 11);
    assert(da.items[5].x == 100);
    assert(da.items[5].y == 200);

    printf("\n========== REMOVE ==========\n");
    da_remove(&da, 5);
    print_array(da);
    assert(da.count == 10);
    assert(da.items[5].x == 5);

    printf("\n========== SWAP REMOVE ==========\n");
    Vector2d last = da_back(&da);
    da_swap_remove(&da, 2);
    print_array(da);
    assert(da.count == 9);
    assert(da.items[2].x == last.x);
    assert(da.items[2].y == last.y);

    printf("\n========== POP ==========\n");
    Vector2d value = da_pop(&da);
    printf("pop = (%d,%d)\n", value.x, value.y);
    print_array(da);
    assert(da.count == 8);

    printf("\n========== RESERVE ==========\n");
    size_t old_capacity = da.capacity;
    da_reserve(&da, 64);
    print_array(da);
    assert(da.capacity >= 64);
    assert(da.capacity >= old_capacity);

    printf("\n========== RESIZE ==========\n");
    da_resize(&da, 20);
    print_array(da);
    assert(da.count == 20);
    // Зайві елементи тепер гарантовано нулі
    for (size_t i = 8; i < 20; i++) {
        assert(da.items[i].x == 0 && da.items[i].y == 0);
    }

    printf("\n========== APPEND ==========\n");
    Vector2d extra[] = {
        {1000, 1000},
        {2000, 2000},
        {3000, 3000}
    };
    da_append(&da, extra, 3);
    print_array(da);
    assert(da.count == 23);
    assert(da.items[20].x == 1000);
    assert(da.items[21].x == 2000);
    assert(da.items[22].x == 3000);

    printf("\n========== CLONE ==========\n");
    DynamicArray copy = {0};
    da_clone(&copy, &da);
    print_array(copy);
    assert(copy.count == da.count);
    assert(copy.capacity >= da.count);
    assert(memcmp(copy.items, da.items, da.count * sizeof(Vector2d)) == 0);

    printf("\n========== CLEAR ==========\n");
    da_clear(&da);
    print_array(da);
    assert(da.count == 0);
    assert(da.capacity != 0);

    printf("\n========== FREE ==========\n");
    da_free(&da);
    assert(da.items == NULL);
    assert(da.count == 0);
    assert(da.capacity == 0);

    da_free(&copy);

    printf("\n=================================\n");
    printf("All DynamicArray tests passed!\n");
    printf("=================================\n");

    return 0;
}