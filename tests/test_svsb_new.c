#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FDS_IMPLEMENTATION
#include "fds.h"

/* Допоміжна функція для порівняння SV зі звичайним рядком */
static int sv_equals_cstr(SV sv, const char *str) {
    size_t len = strlen(str);
    if (sv.count != len) return 0;
    if (len == 0) return 1;
    return memcmp(sv.data, str, len) == 0;
}

/* ================================================================
 * Тести для String View (SV)
 * ================================================================ */
static void test_sv(void) {
    /* Порожні SV */
    SV empty1 = sv_new();
    SV empty2 = sv_from_cstr("");
    SV empty3 = sv_from_parts(NULL, 0);

    assert(sv_eq(empty1, empty2));
    assert(sv_eq(empty2, empty3));
    assert(sv_eq_cstr(empty1, ""));
    assert(sv_eq_cstr(empty2, ""));

    /* Базове порівняння */
    SV hello = sv_from_cstr("hello");
    SV hello2 = sv_from_parts("hello", 5);
    SV world = sv_from_cstr("world");

    assert(sv_eq(hello, hello2));
    assert(!sv_eq(hello, world));
    assert(sv_eq_cstr(hello, "hello"));
    assert(!sv_eq_cstr(hello, "world"));

    /* Префікси / суфікси */
    assert(sv_starts_with(hello, sv_from_cstr("hel")));
    assert(!sv_starts_with(hello, sv_from_cstr("lo")));
    assert(sv_starts_with(hello, sv_new()));   /* порожній префікс завжди на початку */
    assert(sv_ends_with(hello, sv_from_cstr("lo")));
    assert(!sv_ends_with(hello, sv_from_cstr("he")));
    assert(sv_ends_with(hello, sv_new()));     /* порожній суфікс завжди в кінці */
    assert(sv_starts_with_char(hello, 'h'));
    assert(!sv_starts_with_char(hello, 'e'));
    assert(sv_ends_with_char(hello, 'o'));
    assert(!sv_ends_with_char(hello, 'l'));

    /* Пошук символів */
    assert(sv_find_char(hello, 'l') == 2);
    assert(sv_find_char(hello, 'z') == SIZE_MAX);
    assert(sv_rfind_char(hello, 'l') == 3);
    assert(sv_rfind_char(hello, 'z') == SIZE_MAX);

    /* Розділення */
    SV csv = sv_from_cstr("apple,banana,pear");
    SV part = sv_split_left(&csv, ',');
    assert(sv_equals_cstr(part, "apple"));
    assert(sv_equals_cstr(csv, "banana,pear"));

    csv = sv_from_cstr("apple,banana,pear");
    SV right = sv_split_right(&csv, ',');
    assert(sv_equals_cstr(csv, "apple,banana"));
    assert(sv_equals_cstr(right, "pear"));

    /* Розділення без роздільника */
    SV single = sv_from_cstr("single");
    SV left = sv_split_left(&single, ',');
    assert(sv_eq(left, sv_from_cstr("single")));
    assert(single.count == 0);   /* залишок порожній */

    single = sv_from_cstr("single");
    right = sv_split_right(&single, ',');
    assert(sv_eq(single, sv_from_cstr("single")));
    assert(sv_eq(right, sv_new()));   /* права частина порожня */

    /* Обрізка пробілів */
    SV spaced = sv_from_cstr("  \t hello \n ");
    sv_trim(&spaced);
    assert(sv_equals_cstr(spaced, "hello"));

    /* Зріз (slice) */
    SV slice = sv_from_cstr("abcdef");
    sv_slice(&slice, 1, 4);
    assert(sv_equals_cstr(slice, "bcd"));

    /* sv_at */
    assert(sv_at(slice, 0) == 'b');
    assert(sv_at(slice, 2) == 'd');

    /* Споживання (consume) */
    SV prefix = sv_from_cstr("abc");
    assert(sv_consume(&slice, prefix) == 0);  /* slice зараз "bcd" */
    assert(sv_consume_char(&slice, 'b'));
    assert(sv_equals_cstr(slice, "cd"));

    /* Перевірка, що порожній SV не викликає падіння в порівняннях */
    SV empty = sv_new();
    SV nonempty = sv_from_cstr("x");
    assert(!sv_starts_with(empty, nonempty));
    assert(sv_starts_with(nonempty, empty));
    assert(!sv_ends_with(empty, nonempty));
    assert(sv_ends_with(nonempty, empty));
    assert(!sv_eq(empty, nonempty));
    assert(sv_eq(empty, empty));
}

/* ================================================================
 * Тести для String Builder (SB)
 * ================================================================ */
static void test_sb(void) {
    /* Порожній конструктор */
    SB sb = sb_new();
    assert(sb.count == 0);
    assert(sb.capacity >= SB_INITIAL_CAPACITY);
    assert(strcmp(sb.items, "") == 0);

    /* Додавання рядків */
    sb_append(&sb, "Hello");
    assert(sb.count == 5);
    assert(strcmp(sb.items, "Hello") == 0);

    sb_append_char(&sb, ' ');
    assert(strcmp(sb.items, "Hello ") == 0);

    sb_append(&sb, "World!");
    assert(strcmp(sb.items, "Hello World!") == 0);
    assert(sb.count == 12);

    /* Додавання через SV */
    SV sv = sv_from_cstr(" more");
    sb_append_sv(&sb, sv);
    assert(strcmp(sb.items, "Hello World! more") == 0);

    /* Клонування */
    SB clone = sb_clone(&sb);
    assert(clone.count == sb.count);
    assert(strcmp(clone.items, sb.items) == 0);
    sb_free(&clone);

    /* Резервування */
    size_t old_cap = sb.capacity;
    sb_reserve(&sb, 256);
    assert(sb.capacity >= 256);
    assert(strcmp(sb.items, "Hello World! more") == 0);

    /* sb_reserve_extra */
    old_cap = sb.capacity;
    sb_reserve_extra(&sb, 100);
    assert(sb.capacity == old_cap + 100);
    assert(strcmp(sb.items, "Hello World! more") == 0);

    /* sb_appendf */
    sb_appendf(&sb, " [%d + %d = %d]", 2, 3, 5);
    assert(strcmp(sb.items, "Hello World! more [2 + 3 = 5]") == 0);

    /* sb_to_cstr */
    char *cstr = sb_to_cstr(&sb);
    assert(strcmp(cstr, sb.items) == 0);

    /* sb_from_cstr */
    SB sb2 = sb_from_cstr("Test");
    assert(strcmp(sb2.items, "Test") == 0);
    sb_free(&sb2);

    /* sb_append_n */
    sb_append_n(&sb, "!!", 2);
    assert(strcmp(sb.items, "Hello World! more [2 + 3 = 5]!!") == 0);

    /* Очищення */
    sb_free(&sb);
    assert(sb.items == NULL && sb.count == 0 && sb.capacity == 0);
}

int main(void) {
    printf("Running tests...\n");
    test_sv();
    test_sb();
    printf("All tests passed!\n");
    return 0;
}