#define FDS_IMPL
#include "fds.h"
#include "fds_hash.h"

int main(void) {
    // 1. Ініціалізація мапи із початковою місткістю 16 елементів
    FdsMap config = fds_map_create(16);

    // 2. Додавання елементів (Set / Insert)
    fds_map_set_str(&config, "window_title", "My Game Engine");
    fds_map_set_str(&config, "window_width", "1280");
    fds_map_set_str(&config, "window_height", "720");
    fds_map_set_str(&config, "vsync", "true");

    // 3. Читання елементів (Get / Lookup)
    FdsBytesView width_bv;
    if (fds_map_get_sv(&config, sv_from_cstr("window_width"), &width_bv)) {
        fds_log(FINFO, "Ширина вікна: " SV_FMT, (int)width_bv.size, width_bv.data);
    } else {
        fds_log(FWARN, "Ключ 'window_width' не знайдено!");
    }

    // 4. Перевірка наявності ключа (Contains)
    bool has_vsync = fds_map_contains_bv(&config, fds_bv_from_cstr("vsync"));
    fds_log(FINFO, "Параметр vsync присутній: %s", has_vsync ? "так" : "ні");

    // 5. Оновлення значення (Перезапис існуючого ключа)
    fds_map_set_str(&config, "window_width", "1920");

    // 6. Повний обхід елементів хеш-мапи (Foreach)
    fds_log(FINFO, "--- Поточний конфіг (Кількість: %zu) ---", config.count);
    FdsMapItem *it = NULL;
    FDS_MAP_FOREACH(&config, it) {
        fds_log(FINFO, "  %-15.*s => %.*s (PSL: %d)",
                (int)it->key.size, it->key.data,
                (int)it->value.size, it->value.data,
                it->psl);
    }

    // 7. Видалення елемента (Remove)
    if (fds_map_remove_bv(&config, fds_bv_from_cstr("vsync"))) {
        fds_log(FINFO, "Параметр 'vsync' успішно видалено");
    }

    // 8. Очищення ресурсів
    fds_map_destroy(&config);
    return 0;
}