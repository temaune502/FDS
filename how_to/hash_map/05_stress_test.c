#define FDS_IMPL
#include "fds.h"
#include "fds_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TEST_SIZE 500
#define KEY_LEN 24
#define VAL_LEN 32

// Допоміжна функція для генерації випадкових рядків (імітація реальних токенів/ID)
static void generate_random_string(char *s, size_t len) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
    for (size_t i = 0; i < len; ++i) {
        s[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    s[len] = '\0';
}

// Функція для розрахунку мілісекунд
static double get_time_ms(clock_t start, clock_t end) {
    return ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
}

int main(void) {
    fds_log(FINFO, "=== БОЙОВИЙ СТРЕС-ТЕСТ ХЕШ-МАПИ FDS ===");
    fds_log(FINFO, "Кількість елементів: %d\n", TEST_SIZE);

    // 1. Алокація пам'яті для тестових даних (на купі, щоб уникнути переповнення стека)
    char (*keys)[KEY_LEN + 1] = malloc(TEST_SIZE * sizeof(*keys));
    char (*vals)[VAL_LEN + 1] = malloc(TEST_SIZE * sizeof(*vals));
    
    FDS_ASSERT(keys != NULL && vals != NULL, "Не вистачає RAM для тестових даних");

    srand(1337); // Фіксований seed для відтворюваності тесту
    fds_log(FINFO, "Генерація тестових даних...");
    for (int i = 0; i < TEST_SIZE; ++i) {
        generate_random_string(keys[i], KEY_LEN);
        generate_random_string(vals[i], VAL_LEN);
    }

    FdsMap map = fds_map_create(1024); // Спеціально беремо малу початкову ємність, щоб форсувати рехашинг
    clock_t start, end;

    // =========================================================================
    // ФАЗА 1: ВСТАВКА (INSERT)
    // =========================================================================
    start = clock();
    for (int i = 0; i < TEST_SIZE; ++i) {
        fds_map_set_str(&map, keys[i], vals[i]);
    }
    end = clock();
    fds_log(FINFO, "[Фаза 1] Вставка %d елементів: %.2f мс (Ємність мапи: %zu, Елементів: %zu)", 
            TEST_SIZE, get_time_ms(start, end), map.capacity, map.count);

    // =========================================================================
    // ФАЗА 2: ЧИТАННЯ ТА ВЕРИФІКАЦІЯ (LOOKUP)
    // =========================================================================
    start = clock();
    int errors = 0;
    for (int i = 0; i < TEST_SIZE; ++i) {
        FdsBytesView result;
        if (!fds_map_get_bv(&map, fds_bv_from_cstr(keys[i]), &result)) {
            errors++;
            continue;
        }
        // Перевірка на пошкодження даних
        if (strncmp((const char*)result.data, vals[i], result.size) != 0) {
            errors++;
        }
    }
    end = clock();
    fds_log(FINFO, "[Фаза 2] Читання та звірка %d елементів: %.2f мс (Помилок: %d)", 
            TEST_SIZE, get_time_ms(start, end), errors);
    FDS_ASSERT(errors == 0, "СТРЕС-ТЕСТ ПРОВАЛЕНО: Втрата або пошкодження даних!");

    // =========================================================================
    // ФАЗА 3: ОНОВЛЕННЯ ІСНУЮЧИХ КЛЮЧІВ (UPDATE)
    // =========================================================================
    start = clock();
    int update_count = TEST_SIZE / 10; // Оновлюємо 10%
    for (int i = 0; i < update_count; ++i) {
        fds_map_set_str(&map, keys[i], "UPDATED_VALUE_DATA_XXX");
    }
    end = clock();
    fds_log(FINFO, "[Фаза 3] Оновлення %d елементів: %.2f мс", update_count, get_time_ms(start, end));

    // Верифікація оновлення
    FdsBytesView updated_val;
    fds_map_get_bv(&map, fds_bv_from_cstr(keys[0]), &updated_val);
    FDS_ASSERT(strncmp((const char*)updated_val.data, "UPDATED", 7) == 0, "Оновлення не спрацювало!");

    // =========================================================================
    // ФАЗА 4: ВИДАЛЕННЯ (REMOVE)
    // =========================================================================
    start = clock();
    int remove_count = TEST_SIZE / 2; // Видаляємо половину
    for (int i = 0; i < remove_count; ++i) {
        fds_map_remove_bv(&map, fds_bv_from_cstr(keys[i]));
    }
    end = clock();
    fds_log(FINFO, "[Фаза 4] Видалення %d елементів: %.2f мс (Залишилось елементів: %zu)", 
            remove_count, get_time_ms(start, end), map.count);
            
    FDS_ASSERT(map.count == (size_t)(TEST_SIZE - remove_count), "Неправильний лічильник після видалення!");

    // Перевірка, що видалені ключі дійсно недоступні
    bool still_exists = fds_map_contains_bv(&map, fds_bv_from_cstr(keys[0]));
    FDS_ASSERT(!still_exists, "Видалений ключ досі знаходиться в мапі!");

    // Перевірка, що НЕвидалені ключі досі доступні (перевірка цілісності ланцюжків після зсуву)
    bool unremoved_exists = fds_map_contains_bv(&map, fds_bv_from_cstr(keys[TEST_SIZE - 1]));
    FDS_ASSERT(unremoved_exists, "Алгоритм видалення зламав доступ до існуючих ключів!");

    // =========================================================================
    // ОЧИЩЕННЯ
    // =========================================================================
    fds_map_destroy(&map);
    free(keys);
    free(vals);
    
    fds_log(FINFO, "=== СТРЕС-ТЕСТ УСПІШНО ПРОЙДЕНО ===");
    return 0;
}