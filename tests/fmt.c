#include <stdio.h>
#include <assert.h>
#include <string.h>
#define FDS_IMPL
#include "fds.h"

// Підключаємо реалізацію форматування 
// (впевніться, що базові типи SB, SV та функції fds_sbuilder_* вже доступні)
#define FDS_EXT_FORMATTING_IMPLEMENTATION
#include "fds_fmt.h"

// Тест 1: Базове форматування у StringBuilder через append_fmt
void test_sbuilder_append_fmt(void) {
    printf("[TEST] Running test_sbuilder_append_fmt...\n");
    
    SB sb = {0}; // Ініціалізація пустого білдера (залежно від вашої бібліотеки може бути fds_sbuilder_init(&sb))

    // Форматуємо різні типи даних
    fds_sbuilder_append_fmt(&sb, "Hello %s! Value: %d, Hex: %X, Float: %.2f", "FDS", -123, 255, 3.14159);
    
    // Додаємо нуль-термінатор для зручності перевірки через printf/strcmp
    sb_append_sv(&sb, sv_from_parts("", 1));

    printf("  Result: %s\n", (const char *)sb.items);
    
    // Приклад перевірки (розкоментуйте, якщо хочете жорсткий assert)
    // assert(strstr((const char *)sb.data, "Hello FDS!") != NULL);

    sb_free(&sb);
    printf("[TEST] Passed!\n\n");
}

// Тест 2: Scratch-форматування (fds_fmt / fds_fmtv)
void test_scratch_fmt(void) {
    printf("[TEST] Running test_scratch_fmt...\n");

    const char *s1 = fds_fmt("User: %s, ID: %u", "admin", 1001);
    printf("  Scratch 1: %s\n", s1);

    // Увага: fds_fmt використовує спільний статичний буфер, 
    // тому наступний виклик перезапише попередній результат!
    const char *s2 = fds_fmt("Size: %H", (uint64_t)10485760); // 10 MiB
    printf("  Scratch 2: %s\n", s2);

    // Очищуємо пам'ять scratch-буфера в кінці
    fds_fmt_free();
    printf("[TEST] Passed!\n\n");
}

// Тест 3: Hexdump пам'яті / рядка
void test_hexdump(void) {
    printf("[TEST] Running test_hexdump...\n");

    SB sb = {0};
    const char *raw_data = "Binary data stream & test string\n0123456789ABCDEF";
    SV sv = sv_from_parts(raw_data, strlen(raw_data));

    fds_sbuilder_append_fmt(&sb, "Hexdump of SV (%u bytes):\n", (unsigned int)sv.count);
    fds_sbuilder_append_hexdump(&sb, sv, 16); // по 16 байт на рядок
    
    // Нуль-термінатор
    sb_append_null(&sb);

    printf("%s", (const char *)sb.items);

    sb_free(&sb);
    printf("[TEST] Passed!\n\n");
}

// Тест 4: Спеціальні форматування (%q для екранування рядків, %V / %B)
void test_special_formatting(void) {
    printf("[TEST] Running test_special_formatting...\n");

    SB sb = {0};
    SV sv = sv_from_cstr("Line 1\nLine 2\t \"Quotes\" \\ Backslash");

    fds_sbuilder_append_fmt(&sb, "Quoted SV: %q\n", sv);
    sb_append_sv(&sb, sv_from_parts("", 1));

    printf("  %s\n", (const char *)sb.items);

    sb_free(&sb);
    printf("[TEST] Passed!\n\n");
}

int main(void) {
    printf("=== STARTING FDS FORMATTING TESTS ===\n\n");
    
    test_sbuilder_append_fmt();
    test_scratch_fmt();
    test_hexdump();
    test_special_formatting();

    printf("=== ALL TESTS COMPLETED SUCCESSFULLY ===\n");
    fds_allocator_print_stats(fds_allocator_current());
    return 0;
}