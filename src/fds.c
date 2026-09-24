#define DEBUG_MEM
#define FDS_IMPL
#include "fds.h"

// #include "fds_hash.h"

// int main(void) {
//     // 1. Демонстрація швидкого хешування WyHash та захищеного SipHash
//     SV filename = sv_from_cstr("config.ini");
//     uint64_t fast_hash = fds_hash_sv(filename, 1337);
//     fds_log(FINFO, "WyHash для config.ini: 0x%llx", (unsigned long long)fast_hash);

//     // 2. Демонстрація криптографічного SHA-256
//     uint8_t hash_out[32];
//     fds_sha256("fds_framework", 13, hash_out);
    
//     printf("[INFO] SHA-256: ");
//     for (int i = 0; i < 32; i++) printf("%02x", hash_out[i]);
//     printf("\n");

//     // 3. Робота з Robin Hood Hash Map
//     fds_allocator* alloca = fds_allocator_create();
//     fds_allocator_push(alloca);
//     FdsMap map = fds_map_create(16);

//     // Вставка ключ-значень за допомогою SV або C-рядків
//     fds_map_set_str(&map, "server_ip", "192.168.1.1");
//     fds_map_set_str(&map, "port", "8080");
//     fds_map_set_sv(&map, sv_from_cstr("protocol"), sv_from_cstr("HTTPS"));

//     // Пошук у хеш-мапі
//     FdsBytesView result;
//     if (fds_map_get_sv(&map, sv_from_cstr("server_ip"), &result)) {
//         fds_log(FINFO, "Знайдено server_ip: " SV_FMT, (int)result.size, result.data);
//     }
//     fds_allocator_pop();
//     // Обхід хеш-мапи макросом FDS_MAP_FOREACH
//     FdsMapItem *it = NULL;
//     fds_log(FINFO, "--- Всі елементи в мапі ---");
//     FDS_MAP_FOREACH(&map, it) {
//         fds_log(FINFO, "Key: " SV_FMT " => Val: " SV_FMT " (PSL: %d)",
//                 (int)it->key.size, it->key.data,
//                 (int)it->value.size, it->value.data,
//                 it->psl);
//     }

//     // Видалення та очищення
//     fds_map_remove_bv(&map, fds_bv_from_cstr("port"));
//     fds_map_destroy(&map);

//     fds_allocator_print_stats(alloca);
//     fds_allocator_destroy(alloca);
//     return 0;
// }


void test_allocator_nesting(void) {
    printf("[TEST] Перевірка вкладеності алокаторів (Push/Pop)...\n");

    fds_allocator *a1 = fds_allocator_create();
    fds_allocator *a2 = fds_allocator_create();
    fds_allocator *a3 = fds_allocator_create();

    // 1. Входимо в контекст A1
    fds_allocator_push(a1);
    assert(fds_allocator_current() == a1);
    
    void *ptr_a1 = fds_alloc(100);
    assert(fds_allocator_live_blocks_count(a1) == 1);
    assert(fds_allocator_live_blocks_count(a2) == 0);

    // 2. Входимо у вкладений контекст A2
    fds_allocator_push(a2);
    assert(fds_allocator_current() == a2);

    void *ptr_a2_1 = fds_alloc(200);
    void *ptr_a2_2 = fds_alloc(300);
    assert(fds_allocator_live_blocks_count(a2) == 2);
    assert(fds_allocator_live_blocks_count(a1) == 1); // A1 не змінився

    // 3. Глибока вкладеність: входимо в A3
    fds_allocator_push(a3);
    assert(fds_allocator_current() == a3);

    void *ptr_a3 = fds_alloc(50);
    assert(fds_allocator_live_blocks_count(a3) == 1);

    // 4. Виходимо з A3 назад у A2
    fds_allocator_pop();
    assert(fds_allocator_current() == a2);

    void *ptr_a2_3 = fds_alloc(400);
    assert(fds_allocator_live_blocks_count(a2) == 3);

    // 5. Виходимо з A2 назад у A1
    fds_allocator_pop();
    assert(fds_allocator_current() == a1);

    // Звільняємо пам'ять A1 всередині контексту A1
    fds_free(ptr_a1);
    assert(fds_allocator_live_blocks_count(a1) == 0);

    // 6. Повертаємо початковий стан
    fds_allocator_pop();

    // Очищення алокаторів
    fds_allocator_destroy(a1);
    fds_allocator_destroy(a2);
    fds_allocator_destroy(a3);

    printf("  [OK] Вкладеність працює коректно!\n\n");
}

// ============================================================================
// 2. Тест підміни алокаторів та перехресного звільнення/реалокації
// ============================================================================
void test_allocator_substitution(void) {
    printf("[TEST] Перевірка підміни алокаторів (Cross-allocator Free/Realloc)...\n");

    fds_allocator *a1 = fds_allocator_create();
    fds_allocator *a2 = fds_allocator_create();

    // Виділяємо пам'ять через A1
    fds_allocator_push(a1);
    char *ptr1 = (char*)fds_alloc(64);
    strcpy(ptr1, "Hello from Allocator 1");
    assert(fds_allocator_live_blocks_count(a1) == 1);
    assert(fds_allocator_live_blocks_count(a2) == 0);

    // ПІДМІНА КОНТЕКСТУ: Перемикаємось на A2
    fds_allocator_push(a2);
    assert(fds_allocator_current() == a2);

    // Спроба 1: Реалокація блоку з A1, коли активним є A2
    // Очікується: реалокація пройде без segfault і збереже власника A1
    ptr1 = (char*)fds_realloc(ptr1, 128);
    assert(ptr1 != NULL);
    assert(strcmp(ptr1, "Hello from Allocator 1") == 0);
    assert(fds_allocator_live_blocks_count(a1) == 1); // Блок все ще належить A1
    assert(fds_allocator_live_blocks_count(a2) == 0); // A2 чистий

    // Спроба 2: Явне виділення в A2 і спроба звільнити блок A1
    void *ptr2 = fds_alloc(32);
    assert(fds_allocator_live_blocks_count(a2) == 1);

    // Звільняємо ptr1 (виділений у A1), перебуваючи в контексті A2
    // Або через fds_free_a(a2, ptr1) — заголовок повинен захистити від segfault
    fds_free_a(a2, ptr1); 

    // Перевіряємо, що лічильник зменшився саме у A1, а не у A2
    assert(fds_allocator_live_blocks_count(a1) == 0);
    assert(fds_allocator_live_blocks_count(a2) == 1);

    // Очищаємо залишки
    fds_free(ptr2);
    assert(fds_allocator_live_blocks_count(a2) == 0);

    // Відновлюємо стек TLS
    fds_allocator_pop(); // pop A2
    fds_allocator_pop(); // pop A1

    fds_allocator_destroy(a1);
    fds_allocator_destroy(a2);

    printf("  [OK] Підміна алокаторів та захист від Segfault працює успішно!\n\n");
}

// ============================================================================
// Main
// ============================================================================
int main(void) {
    printf("===========================================\n");
    printf(" Запуск модульних тестів для fds_allocator \n");
    printf("===========================================\n\n");

    test_allocator_nesting();
    test_allocator_substitution();

    printf("Усі тести пройдено успішно!\n");
    return 0;
}