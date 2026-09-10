#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define FDS_EXT_CFG_IMPLEMENTATION
#include "fds_Onyx.h"


/* Вимірювання часу в секундах */
static double get_time_sec(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

/* Тест парсингу та доступу до значень */
static void test_basic_usage(void) {
    const char *src =
        "app_name = \"FdsEngine\";\n"
        "version = 2;\n"
        "scale = 1.5;\n"
        "debug = true;\n"
        "max_entities = 1000;\n"
        "screen_width = 1920;\n"
        "screen_height = 1080;\n"
        "total_pixels = screen_width * screen_height;\n"
        "greeting = \"Hello, \" + \"World!\";\n"
        "is_wide = screen_width > 1600 ? true : false;\n"
        "half_scale = scale / 2.0;\n"
        "double_version = version * 2;\n";

    fds_string_view source = {src, strlen(src)};
    Fds_Cfg_Error err = {0};
    Fds_Cfg_Doc *doc = fds_cfg_parse(source, &err);

    if (!doc) {
        fprintf(stderr, "Помилка парсингу: код %d, рядок %zu, колонка %zu: %s\n",
                err.code, err.line, err.column, err.msg ? err.msg : "невідомо");
        return;
    }

    printf("=== Базове використання ===\n");
    printf("app_name: %s\n", fds_cfg_get_string(doc, "app_name", "невідомо"));
    printf("version: %lld\n", (long long)fds_cfg_get_int(doc, "version", -1));
    printf("scale: %f\n", fds_cfg_get_float(doc, "scale", 0.0));
    printf("debug: %s\n", fds_cfg_get_bool(doc, "debug", false) ? "true" : "false");
    printf("max_entities: %lld\n", (long long)fds_cfg_get_int(doc, "max_entities", -1));
    printf("total_pixels: %lld\n", (long long)fds_cfg_get_int(doc, "total_pixels", -1));
    printf("greeting: %s\n", fds_cfg_get_string(doc, "greeting", ""));
    printf("is_wide: %s\n", fds_cfg_get_bool(doc, "is_wide", false) ? "true" : "false");
    printf("half_scale: %f\n", fds_cfg_get_float(doc, "half_scale", 0.0));
    printf("double_version: %lld\n", (long long)fds_cfg_get_int(doc, "double_version", -1));

    /* Перевірка наявності ключа */
    const char *missing_key = "nonexistent";
    if (fds_cfg_has_key(doc, missing_key)) {
        printf("Ключ '%s' знайдено\n", missing_key);
    } else {
        printf("Ключ '%s' відсутній (очікувано)\n", missing_key);
    }

    fds_cfg_free(doc);
}

/* Тест функцій (def) */
static void test_functions(void) {
    const char *src =
        "def add(a, b) = a + b;\n"
        "def mul(x, y) = x * y;\n"
        "def triple(z) = z * 3;\n"
        "value1 = add(2, 3);\n"
        "value2 = mul(4, 5);\n"
        "value3 = triple(7);\n"
        "combined = add(mul(2, 3), 10);\n";

    fds_string_view source = {src, strlen(src)};
    Fds_Cfg_Error err = {0};
    Fds_Cfg_Doc *doc = fds_cfg_parse(source, &err);

    if (!doc) {
        fprintf(stderr, "Помилка парсингу функцій: %s\n", err.msg ? err.msg : "невідомо");
        return;
    }

    printf("\n=== Функції ===\n");
    printf("value1: %lld\n", (long long)fds_cfg_get_int(doc, "value1", -1));
    printf("value2: %lld\n", (long long)fds_cfg_get_int(doc, "value2", -1));
    printf("value3: %lld\n", (long long)fds_cfg_get_int(doc, "value3", -1));
    printf("combined: %lld\n", (long long)fds_cfg_get_int(doc, "combined", -1));

    fds_cfg_free(doc);
}

/* Тест обробки помилок */
static void test_error_handling(void) {
    const char *src_div_zero = "x = 1 / 0;\n";
    fds_string_view source = {src_div_zero, strlen(src_div_zero)};
    Fds_Cfg_Error err = {0};
    Fds_Cfg_Doc *doc = fds_cfg_parse(source, &err);
    if (doc) {
        printf("\n=== Помилка: ділення на нуль ===\n");
        printf("Очікувалась помилка, але парсинг пройшов успішно (погано)\n");
        fds_cfg_free(doc);
    } else {
        printf("\n=== Помилка: ділення на нуль ===\n");
        printf("Код помилки: %d, рядок %zu, колонка %zu, повідомлення: %s\n",
               err.code, err.line, err.column, err.msg ? err.msg : "немає");
    }

    const char *src_undef = "y = undefined_var + 5;\n";
    source = (fds_string_view){src_undef, strlen(src_undef)};
    memset(&err, 0, sizeof(err));
    doc = fds_cfg_parse(source, &err);
    if (doc) {
        printf("\n=== Помилка: невизначена змінна ===\n");
        printf("Очікувалась помилка, але парсинг пройшов успішно (погано)\n");
        fds_cfg_free(doc);
    } else {
        printf("\n=== Помилка: невизначена змінна ===\n");
        printf("Код помилки: %d, рядок %zu, колонка %zu, повідомлення: %s\n",
               err.code, err.line, err.column, err.msg ? err.msg : "немає");
    }
}

static void test_conditions(void) {
    const char *src =
        "enabled = true;\n"
        "@if enabled {\n"
        "    selected = 42;\n"
        "} @else {\n"
        "    selected = -1;\n"
        "}\n"
        "@if false {\n"
        "    skipped = 1;\n"
        "} @else {\n"
        "    fallback = 7;\n"
        "}\n"
        "@assert selected == 42, \"selected branch is wrong\";\n";

    Fds_Cfg_Error err = {0};
    Fds_Cfg_Doc *doc = fds_cfg_parse((fds_string_view){src, strlen(src)}, &err);
    if (!doc) {
        fprintf(stderr, "Помилка умов: код %d: %s\n",
                err.code, err.msg ? err.msg : "невідомо");
        return;
    }

    printf("\n=== Умови ===\n");
    printf("selected: %lld\n", (long long)fds_cfg_get_int(doc, "selected", -1));
    printf("fallback: %lld\n", (long long)fds_cfg_get_int(doc, "fallback", -1));
    printf("skipped exists: %s\n", fds_cfg_has_key(doc, "skipped") ? "так" : "ні");
    fds_cfg_free(doc);

    const char *failed_assert = "@assert false, \"must fail\";\n";
    memset(&err, 0, sizeof(err));
    doc = fds_cfg_parse((fds_string_view){failed_assert, strlen(failed_assert)}, &err);
    printf("assert failure code: %d\n", doc ? FDS_CFG_OK : err.code);
    if (doc) fds_cfg_free(doc);
}

/* Бенчмарк швидкості доступу */
static void benchmark_access(void) {
    const char *src =
        "val_int = 123;\n"
        "val_float = 3.14;\n"
        "val_bool = true;\n"
        "val_string = \"benchmark\";\n";

    fds_string_view source = {src, strlen(src)};
    Fds_Cfg_Error err = {0};
    Fds_Cfg_Doc *doc = fds_cfg_parse(source, &err);
    if (!doc) {
        fprintf(stderr, "Помилка парсингу для бенчмарку: %s\n", err.msg ? err.msg : "невідомо");
        return;
    }

    const int iterations = 1000000;
    volatile int64_t sum = 0;
    volatile double fsum = 0.0;
    volatile bool bflag = false;
    volatile const char *str = NULL;

    printf("\n=== Бенчмарк доступу (1 000 000 запитів) ===\n");

    /* int */
    double start = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        sum += fds_cfg_get_int(doc, "val_int", 0);
    }
    double elapsed = get_time_sec() - start;
    printf("get_int: %.3f сек (%.1f млн запитів/сек)\n", elapsed, iterations / elapsed / 1e6);

    /* float */
    start = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        fsum += fds_cfg_get_float(doc, "val_float", 0.0);
    }
    elapsed = get_time_sec() - start;
    printf("get_float: %.3f сек (%.1f млн запитів/сек)\n", elapsed, iterations / elapsed / 1e6);

    /* bool */
    start = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        bflag = fds_cfg_get_bool(doc, "val_bool", false);
    }
    elapsed = get_time_sec() - start;
    printf("get_bool: %.3f сек (%.1f млн запитів/сек)\n", elapsed, iterations / elapsed / 1e6);

    /* string */
    start = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        str = fds_cfg_get_string(doc, "val_string", "");
    }
    elapsed = get_time_sec() - start;
    printf("get_string: %.3f сек (%.1f млн запитів/сек)\n", elapsed, iterations / elapsed / 1e6);

    /* Щоб компілятор не викинув результати */
    if (sum == 0 || fsum == 0.0 || !bflag || str == NULL) {
        printf("Сумарні значення використані для запобігання оптимізації: %lld, %f, %d, %s\n",
               (long long)sum, fsum, bflag, str);
    }

    fds_cfg_free(doc);
}

int main(void) {
    printf("Тест бібліотеки Fds Config\n");
    printf("============================\n");

    test_basic_usage();
    test_functions();
    test_error_handling();
    test_conditions();
    benchmark_access();

    return 0;
}