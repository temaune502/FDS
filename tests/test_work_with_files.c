#define FDS_IMPLEMENTATION
#include "fds.h"
#include <stdio.h>
#include <assert.h>
void print_directory_contents(const char *path) {
    if (fds_dir_exists(path) != 0) {
        printf("Помилка: Папка '%s' не існує.\n", path);
        return;
    }

    FdsDirIter iter;
    if (fds_dir_iter_open(path, &iter) == 0) {
        SV name;
        int is_dir;
        
        printf("Вміст папки '%s':\n", path);
        
        while (fds_dir_iter_next(&iter, &name, &is_dir) == 0) {
            if (is_dir) {
                printf("[ПАПКА] " SV_FMT "\n", SV_ARGS(name));
            } else {
                printf("        " SV_FMT "\n", SV_ARGS(name));
                
                /* Якщо шукаємо щось конкретне */
                if (sv_eq_cstr(name, "config.ini")) {
                    printf("  -> ЗНАЙДЕНО ВАЖЛИВИЙ ФАЙЛ!\n");
                }
            }
        }
        fds_dir_iter_close(&iter); /* Обов'язково закриваємо! */
    }
}

/* Допоміжний макрос для гарного виводу результатів тестування */
#define TEST_RUN(name, expr) \
    do { \
        int res = (expr); \
        if (res == 0) { \
            printf("[УСПІХ] %s\n", name); \
        } else { \
            printf("[ПОМИЛКА] %s (повернуло %d)\n", name, res); \
        } \
    } while (0)

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    print_directory_contents(".");
    printf("=== Запуск тестування бібліотеки FDS ===\n\n");

    /* 1. Створення папки та перевірка її існування */
    printf("--- Тест: Директорії ---\n");
    TEST_RUN("Створення папки 'temp'", fds_dir_create("temp"));
    TEST_RUN("Перевірка існування папки 'temp'", fds_dir_exists("temp"));
    
    // Перевірка на неіснуючу папку (має повернути 1)
    if (fds_dir_exists("temp_fake") == 1) {
        printf("[УСПІХ] Неіснуюча папка правильно не знайдена\n");
    } else {
        printf("[ПОМИЛКА] Неіснуюча папка пройшла перевірку\n");
    }
    printf("\n");

    /* 2. Запис та дописування файлів через String View (SV) */
    printf("--- Тест: Запис файлів (SV) ---\n");
    SV text_sv1 = sv_from_cstr("Рядок 1: Привіт, світ!\n");
    SV text_sv2 = sv_from_cstr("Рядок 2: Додаємо через SV.\n");
    
    TEST_RUN("Створення файлу test_sv.txt", fds_file_write_sv("temp/test_sv.txt", text_sv1));
    TEST_RUN("Дописування у test_sv.txt", fds_file_append_sv("temp/test_sv.txt", text_sv2));
    printf("\n");

    /* 3. Запис та дописування файлів через String Builder (SB) */
    printf("--- Тест: Запис файлів (SB) ---\n");
    SB my_sb = sb_new();
    sb_append(&my_sb, "Key1=Value1\n");
    TEST_RUN("Створення файлу test_sb.txt", fds_file_write_sb("temp/test_sb.txt", &my_sb));
    sb_free(&my_sb);

    SB my_sb_append = sb_new();
    sb_append(&my_sb_append, "Key2=Value2\n");
    TEST_RUN("Дописування у test_sb.txt", fds_file_append_sb("temp/test_sb.txt", &my_sb_append));
    sb_free(&my_sb_append);
    printf("\n");

    /* 4. Перейменування та перевірка існування файлів */
    printf("--- Тест: Файлова система (Перейменування та Шляхи) ---\n");
    TEST_RUN("Перейменування test_sb.txt -> config.ini", fds_rename("temp/test_sb.txt", "temp/config.ini"));
    TEST_RUN("Перевірка існування config.ini", fds_file_exists("temp/config.ini"));
    
    SV ext;
    TEST_RUN("Витягування розширення з temp/config.ini", fds_path_extension(sv_from_cstr("temp/config.ini"), &ext));
    printf("        Розширення файлу: '" SV_FMT "'\n", SV_ARGS(ext));
    printf("\n");

    /* 5. Читання файлу у String Builder */
    printf("--- Тест: Читання файлу (SB) ---\n");
    SB read_sb = sb_new();
    TEST_RUN("Читання test_sv.txt у SB", fds_file_read_to_sb("temp/test_sv.txt", &read_sb));
    printf("Вміст test_sv.txt:\n%s", sb_to_cstr(&read_sb));
    sb_free(&read_sb);
    printf("\n");

    /* 6. Читання файлу в Арену та парсинг по рядках */
    printf("--- Тест: Читання в Арену та Парсинг (SV) ---\n");
    FixedArena my_arena = fixed_arena_create(4096);
    SV arena_sv;
    
    TEST_RUN("Читання config.ini в FixedArena", fds_file_read_to_arena("temp/config.ini", &my_arena, &arena_sv));
    
    printf("Читання рядків з config.ini:\n");
    SV line;
    int line_count = 0;
    while (fds_sv_next_line(&arena_sv, &line) == 0) {
        line_count++;
        printf("  [%d]: " SV_FMT "\n", line_count, SV_ARGS(line));
    }
    fixed_arena_free(&my_arena);
    printf("\n");

    /* 7. Ітерація по директорії */
    printf("--- Тест: Сканування Директорії ---\n");
    FdsDirIter iter;
    TEST_RUN("Відкриття папки 'temp' для ітерації", fds_dir_iter_open("temp", &iter));
    
    SV item_name;
    int is_dir;
    printf("Вміст 'temp/':\n");
    while (fds_dir_iter_next(&iter, &item_name, &is_dir) == 0) {
        if (is_dir) {
            printf("  [ПАПКА] " SV_FMT "\n", SV_ARGS(item_name));
        } else {
            printf("  [ФАЙЛ]  " SV_FMT "\n", SV_ARGS(item_name));
        }
    }
    fds_dir_iter_close(&iter);
    printf("Закриття ітератора папки.\n\n");
    printf("--- Тест: Видалення файлів та папок ---\n");
    
    TEST_RUN("Видалення файлу 'temp/test_sv.txt'", fds_file_delete("temp/test_sv.txt"));
    TEST_RUN("Видалення файлу 'temp/config.ini'", fds_file_delete("temp/config.ini"));
    TEST_RUN("Видалення файлу експеремент ",fds_file_delete("експеремент.txt"));
    
    /* Якщо спробувати видалити папку 'temp' до видалення файлів у ній,
     * тест має показати ПОМИЛКУ (1), що є правильною поведінкою ОС. 
     * Але оскільки ми щойно видалили всі файли, папка порожня. */
    TEST_RUN("Видалення порожньої папки 'temp'", fds_dir_delete("temp"));
    TEST_RUN("Видалення порожньої папки 'not_empty'", fds_dir_delete("not_empty"));
    
    /* Переконаємось, що папки справді більше немає */
    if (fds_dir_exists("temp") == 1) {
        printf("[УСПІХ] Перевірка: папка 'temp' успішно стерта з диска.\n\n");
    } else {
        printf("[ПОМИЛКА] Перевірка: папка 'temp' все ще існує!\n\n");
    }
    printf("=== Тестування успішно завершено! ===\n");
    return 0;
}