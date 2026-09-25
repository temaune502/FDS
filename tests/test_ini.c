#define FDS_IMPLEMENTATION
#include "fds.h"


int main2(void) {
    const char *test_filename = "engine_config.ini";
    
    // 1. Створюємо тестовий вміст (типові налаштування вікна та графіки)
    const char *ini_content = 
        "; Головні налаштування\n"
        "engine_name = Custom C3/Raylib Engine\n"
        "\n"
        "[Window]\n"
        "width = 1920\n"
        "height = 1080\n"
        "fullscreen = true\n"
        "vsync = 1\n"
        "\n"
        "[Graphics]\n"
        "# Налаштування апскейлу та рендеру+2\n"
        "upscale_method = AI_FSR\n"
        "msaa_level = 4\n";

    // Записуємо його у файл за допомогою твоєї fds_file_write_sv
    if (fds_file_write_sv(test_filename, sv_from_cstr(ini_content)) != 0) {
        fprintf(stderr, "Не вдалося створити тестовий файл!\n");
        return 1;
    }
    time_t times =  fds_get_file_mtime(test_filename);
    printf("%lld\n", times);
    // 2. Парсимо створений файл
    printf("--- Читання та парсинг файлу %s ---\n", test_filename);
    IniConfig config = ini_parse(test_filename);

    // 3. Виводимо весь розпарсений конфіг на екран
    ini_print(&config);

    // 4. Демонстрація пошуку та конвертації значень
    printf("\n--- Пошук значень ---\n");
    
    SV width_sv = ini_get(&config, "Window", "width");
    SV upscale_sv = ini_get(&config, "Graphics", "upscale_method");
    SV not_found_sv = ini_get(&config, "Window", "min_height");

    if (width_sv.count > 0) {
        // Для конвертації в число використовуємо sv_to_cstr
        // (пам'ятай, що sv_to_cstr робить malloc, тому треба робити free)
        char *width_cstr = sv_to_cstr(width_sv);
        int width_int = atoi(width_cstr);
        printf("Знайдено [Window] width: %d (як int)\n", width_int);
        free(width_cstr);
    }

    if (upscale_sv.count > 0) {
        // Для звичайного виводу достатньо макросів SV_FMT / SV_ARGS
        printf("Знайдено [Graphics] upscale_method: " SV_FMT "\n", SV_ARGS(upscale_sv));
    }

    if (not_found_sv.count == 0) {
        printf("Ключ [Window] min_height передбачувано не знайдено.\n");
    }

    // 5. Прибираємо за собою
    ini_free(&config);
    fds_file_delete(test_filename);

    printf("\nТест успішно завершено, пам'ять звільнено, файл видалено.\n");
    
    return 0;
}

int main(void) {
    const char *test_filename = "engine_config.ini";
    
    const char *ini_content = 
        "[Window]\n"
        "width = 1920\n"
        "height = 1080\n"
        "fullscreen = true\n"
        "\n"
        "[Graphics]\n"
        "upscale_method = AI_FSR\n"
        "gamma = 2.2\n";

    fds_file_write_sv(test_filename, sv_from_cstr(ini_content));
    IniConfig config = ini_parse(test_filename);

    // --- ЗРУЧНИЙ ДОСТУП ДО ДАНИХ ---

    // 1. Цілі числа з дефолтним значенням (якщо ключа немає, отримаємо 800)
    int width = ini_get_int(&config, "Window", "width", 800);
    int height = ini_get_int(&config, "Window", "height", 600);
    
    // 2. Булеві значення (розуміє true/false, 1/0, yes/no)
    int is_fullscreen = ini_get_bool(&config, "Window", "fullscreen", 0);
    
    // 3. Float-значення
    float gamma = ini_get_float(&config, "Graphics", "gamma", 1.0f);
    
    // 4. Робота з рядками через Temp Arena.
    // Нам НЕ треба робити free(upscale), воно живе в тимчасовій арені!
    char *upscale = ini_get_temp_cstr(&config, "Graphics", "upscale_method", "None");
    
    // Перевірка ключа, якого не існує (спрацює fallback)
    int max_fps = ini_get_int(&config, "Graphics", "max_fps", 60);

    // Виводимо результати
    printf("Роздільна здатність: %dx%d\n", width, height);
    printf("Повний екран: %s\n", is_fullscreen ? "Так" : "Ні");
    printf("Метод апскейлу: %s\n", upscale);
    printf("Гамма: %.2f\n", gamma);
    printf("Макс. FPS (дефолт): %d\n", max_fps);

    // Прибираємо за собою
    ini_free(&config);
    temp_arena_reset(); // Очищаємо тимчасову арену наприкінці кадру/ітерації
    fds_file_delete(test_filename);
    
    main2();

    return 0;
}