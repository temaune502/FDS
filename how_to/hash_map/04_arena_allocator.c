#define FDS_IMPL
#include "fds.h"
#include "fds_hash.h"

int main(void) {
    // 1. Створюємо фіксовану аллокатор (наприклад, для кадру гри або обробки одного запиту)
    fds_allocator* arena = fds_allocator_create();

    // 2. Передаємо вказівник на арену при створенні хеш-мапи.
    // Тепер ВСІ нові бакети, рехашинг та дублікати ключів/значень виділяються з Арени!
    FdsMap arena_map = fds_map_create_a(arena, 32);

    // 3. Додаємо елементи без остраху за витоки пам'яті
    for (int i = 0; i < 100; ++i) {
        char key_buf[32];
        char val_buf[32];
        snprintf(key_buf, sizeof(key_buf), "entity_id_%d", i);
        snprintf(val_buf, sizeof(val_buf), "pos_x_%d_y_%d", i * 2, i * 3);

        fds_map_set_str(&arena_map, key_buf, val_buf);
    }

    fds_log(FINFO, "Успішно збережено %zu елементів в Арені", arena_map.count);

    // 4. Пошук у карті Арени
    FdsBytesView ent_val;
    if (fds_map_get_sv(&arena_map, sv_from_cstr("entity_id_50"), &ent_val)) {
        fds_log(FINFO, "entity_id_50 = " SV_FMT, (int)ent_val.size, ent_val.data);
    }

    // 5. Очищення мапи не потрібне окремо!
    // Достатньо зробити скидання всієї арени за один такт O(1):
    fds_allocator_destroy(arena);
    fds_log(FINFO, "Арену скинуто. Вся пам'ять мапи очищена за 1 інструкцію.");

    return 0;
}