#define FDS_IMPLEMENTATION
#include "fds.h"   // виправлена версія заголовка
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(expr) do {                              \
    tests_run++;                                     \
    if (expr) {                                      \
        tests_passed++;                              \
        printf("  PASS: %s\n", #expr);               \
    } else {                                         \
        printf("  FAIL: %s (%s:%d)\n", #expr,        \
               __FILE__, __LINE__);                  \
    }                                                \
} while(0)

#define TEST_STREQ(a, b) do {                        \
    const char *_a = (a);                            \
    const char *_b = (b);                            \
    TEST((_a != NULL && _b != NULL &&                 \
          strcmp(_a, _b) == 0) ||                    \
         (_a == NULL && _b == NULL));                \
} while(0)

/* ------------------------------------------------------------------ */
/* Допоміжна функція для виведення SV (безпечно, як ASCII)            */
void print_sv(const char *label, SV sv) {
    printf("%s = '%.*s' (len=%zu)\n", label, (int)sv.count, sv.data, sv.count);
}

/* ------------------------------------------------------------------ */
int main(void) {
    printf("=== Тестування String Builder (SB) ===\n");

    /* sb_new / sb_append / sb_to_cstr */
    SB sb = sb_new();
    sb_append(&sb, "Hello");
    sb_append_char(&sb, ' ');
    sb_append(&sb, "World");
    TEST_STREQ(sb_to_cstr(&sb), "Hello World");

    /* sb_append_n */
    sb_free(&sb);
    sb = sb_new();
    sb_append_n(&sb, "data", 3);   // бере лише "dat"
    TEST_STREQ(sb_to_cstr(&sb), "dat");

    /* sb_appendf */
    sb_appendf(&sb, " %d %s", 42, "answer");
    TEST_STREQ(sb_to_cstr(&sb), "dat 42 answer");

    /* sb_clone */
    SB clone = sb_clone(&sb);
    TEST_STREQ(sb_to_cstr(&clone), "dat 42 answer");
    sb_free(&clone);

    /* sb_from_cstr */
    SB sb2 = sb_from_cstr("initial");
    TEST_STREQ(sb_to_cstr(&sb2), "initial");
    sb_free(&sb2);

    /* sb_append_sv */
    SB sb3 = sb_new();
    SV hello_sv = sv_from_cstr("Hello ");
    SV world_sv = sv_from_cstr("World!");
    sb_append_sv(&sb3, hello_sv);
    sb_append_sv(&sb3, world_sv);
    TEST_STREQ(sb_to_cstr(&sb3), "Hello World!");
    sb_free(&sb3);

    /* sb_reserve / sb_reserve_extra (ex sb_alloc) */
    SB sb4 = sb_new();
    sb_reserve(&sb4, 128);
    TEST(sb4.capacity >= 128);
    sb_reserve_extra(&sb4, 50);
    TEST(sb4.capacity >= 178);
    sb_free(&sb4);

    /* sb_append_null (не змінює count, тільки ставить '\0') */
    SB sb5 = sb_new();
    sb_append(&sb5, "test");
    sb_append_null(&sb5);
    TEST(sb5.count == 4);
    TEST(sb5.items[4] == '\0');
    sb_free(&sb5);

    sb_free(&sb);   /* завершуємо sb з "dat 42 answer" */

    printf("\n=== Тестування String View (SV) ===\n");

    /* sv_from_cstr / sv_eq / sv_eq_cstr */
    SV a = sv_from_cstr("Hello");
    SV b = sv_from_cstr("Hello");
    SV c = sv_from_cstr("HellO");
    TEST(sv_eq(a, b));
    TEST(!sv_eq(a, c));
    TEST(sv_eq_cstr(a, "Hello"));
    TEST(!sv_eq_cstr(a, "hello"));

    /* sv_new (порожній) */
    SV empty = sv_new();
    TEST(empty.count == 0);
    TEST(empty.data == NULL);
    TEST(sv_eq(empty, sv_new()));

    /* sv_from_parts */
    const char *text = "example";
    SV part = sv_from_parts(text, 3);
    TEST(sv_eq_cstr(part, "exa"));

    /* sv_from_sb */
    SB tmp_sb = sb_from_cstr("from_sb");
    SV from_sb = sv_from_sb(&tmp_sb);
    TEST(sv_eq_cstr(from_sb, "from_sb"));
    sb_free(&tmp_sb);

    /* sv_remove_prefix / sv_remove_suffix */
    SV sv1 = sv_from_cstr("prefix_data");
    sv_remove_prefix(&sv1, 7);   // залишається "data"
    TEST(sv_eq_cstr(sv1, "data"));
    sv_remove_suffix(&sv1, 2);   // "da"
    TEST(sv_eq_cstr(sv1, "da"));

    /* sv_trim_left / sv_trim_right / sv_trim */
    SV sv2 = sv_from_cstr("  \t trim me \n ");
    sv_trim(&sv2);
    TEST(sv_eq_cstr(sv2, "trim me"));

    /* sv_find_char / sv_rfind_char */
    SV find_sv = sv_from_cstr("abracadabra");
    size_t pos = sv_find_char(find_sv, 'c');
    TEST(pos == 4);
    pos = sv_rfind_char(find_sv, 'a');
    TEST(pos == 10);
    pos = sv_find_char(find_sv, 'z');
    TEST(pos == SIZE_MAX);

    /* sv_starts_with / sv_ends_with */
    SV hay = sv_from_cstr("start_middle_end");
    TEST(sv_starts_with(hay, sv_from_cstr("start")));
    TEST(!sv_starts_with(hay, sv_from_cstr("art")));
    TEST(sv_ends_with(hay, sv_from_cstr("end")));
    TEST(!sv_ends_with(hay, sv_from_cstr("mid")));
    TEST(sv_starts_with_char(hay, 's'));
    TEST(sv_ends_with_char(hay, 'd'));
    TEST(!sv_starts_with_char(hay, 'x'));

    /* sv_consume_char / sv_consume */
    SV cons = sv_from_cstr("prefix:value");
    TEST(sv_consume_char(&cons, 'p'));   // 'p' з'їдено
    TEST(sv_eq_cstr(cons, "refix:value"));
    TEST(!sv_consume_char(&cons, 'z'));
    TEST(sv_consume(&cons, sv_from_cstr("refix:")));
    TEST(sv_eq_cstr(cons, "value"));
    TEST(!sv_consume(&cons, sv_from_cstr("xxx")));

    /* sv_split_left */
    SV left_src = sv_from_cstr("one,two,three");
    SV left1 = sv_split_left(&left_src, ',');
    TEST(sv_eq_cstr(left1, "one"));
    TEST(sv_eq_cstr(left_src, "two,three"));
    SV left2 = sv_split_left(&left_src, ',');
    TEST(sv_eq_cstr(left2, "two"));
    TEST(sv_eq_cstr(left_src, "three"));
    SV left3 = sv_split_left(&left_src, ',');   // роздільника немає
    TEST(sv_eq_cstr(left3, "three"));
    TEST(left_src.count == 0);                 // залишок порожній

    /* sv_split_right (виправлена версія) */
    SV right_src = sv_from_cstr("dir/sub/file.txt");
    SV right_part = sv_split_right(&right_src, '/');
    TEST(sv_eq_cstr(right_part, "file.txt"));    // права частина
    TEST(sv_eq_cstr(right_src, "dir/sub"));      // ліва частина
    SV right_part2 = sv_split_right(&right_src, '/');
    TEST(sv_eq_cstr(right_part2, "sub"));
    TEST(sv_eq_cstr(right_src, "dir"));
    SV right_part3 = sv_split_right(&right_src, '/'); // немає роздільника
    TEST(sv_eq_cstr(right_part3, ""));          // повернуто порожній SV
    TEST(sv_eq_cstr(right_src, "dir"));         // оригінал не змінився

    /* sv_slice */
    SV slice_sv = sv_from_cstr("0123456789");
    sv_slice(&slice_sv, 3, 7);
    TEST(sv_eq_cstr(slice_sv, "3456"));

    /* sv_at */
    SV at_sv = sv_from_cstr("ABCD");
    TEST(sv_at(at_sv, 0) == 'A');
    TEST(sv_at(at_sv, 3) == 'D');
    // TEST(sv_at(at_sv, 4) == ?)  // призведе до abort, тому не викликаємо

    /* sv_to_cstr (SV) */
    SV to_cstr_sv = sv_from_cstr("copy me");
    char *cstr = sv_to_cstr(to_cstr_sv);
    TEST_STREQ(cstr, "copy me");
    free(cstr);

    printf("\n=== Демонстрація проблем з UTF-8 ===\n");
    /* Показуємо, що функції працюють на рівні байтів, а не символів */
    const char *ukr = "Привіт";  // 6 символів, 12 байт (UTF-8 кирилиця)
    SV utf_sv = sv_from_cstr(ukr);
    printf("UTF-8 рядок: '%s'\n", ukr);
    printf("Кількість байт: %zu (очікується 12)\n", utf_sv.count);
    printf("sv_at(0) повертає байт 0x%02X, а не символ 'П'\n",
           (unsigned char)sv_at(utf_sv, 0));
    /* sv_remove_prefix на 3 байти зламає перший символ */
    SV broken = utf_sv;
    sv_remove_prefix(&broken, 3);
    printf("Після видалення 3 байт залишок = '%.*s' (невалідний UTF-8)\n",
           (int)broken.count, broken.data);
    printf("Це демонструє, що бібліотека НЕ ПІДТРИМУЄ UTF-8.\n");

    /* Підсумок */
    printf("\n=== Результати тестування ===\n");
    printf("Виконано тестів: %d\n", tests_run);
    printf("Пройдено успішно: %d\n", tests_passed);
    printf("Провалено: %d\n", tests_run - tests_passed);
    if (tests_passed == tests_run) {
        printf(">>> Усі тести пройдено успішно! <<<\n");
    } else {
        printf(">>> Деякі тести провалились! <<<\n");
    }

    return (tests_passed == tests_run) ? 0 : 1;
}