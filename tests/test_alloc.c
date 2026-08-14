
#define TEMP_ARENA_SIZE (32 * MB)
#define HEAP_SIZE    32*MB

#define FDS_IMPLEMENTATION
#include "fds.h"

int main(void)
{

    printf("=== Тестування алокатора ===\n");
    
    
    printf("%llu\n", sizeof(heap_mem));
    
    void *a = fds_malloc(1000);
    void *b = fds_malloc(1000);
    void *c = fds_malloc(1000);
    printf("a=%p, b=%p, c=%p\n", a, b, c);

    fds_free(b);
    printf("b звільнено\n");

    a = fds_realloc(a, 100);
    printf("realloc a до 100: %p, usable=%zu\n", a, fds_malloc_usable_size(a));

    fds_free(a);
    fds_free(c);

    void *big = fds_malloc(8 * 1024 * 1024 - 1024);
    if (big) {
        printf("Великий блок виділено: %p\n", big);
        memset(big, 0xAB, 8 * 1024 * 1024 - 1024);
        fds_free(big);
    } else {
        printf("Великий блок не вдався!\n");
    }

    /* Перевірка цілісності */
    if (heap_validate()) {
        printf("Купа цілісна.\n");
    } else {
        printf("Купа ПОШКОДЖЕНА!\n");
    }
    return 0;
}