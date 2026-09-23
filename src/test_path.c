
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define FDS_IMPL
#include "fds.h" 

#define FDS_PATH_IMPLEMENTATION
#include "fds_path.h"

// Допоміжний макрос для зручності перевірки fds_string_view
#define ASSERT_SV_EQ(sv, str) assert(fds_sv_eq((sv), fds_sv(str)))

int main(void) {
    printf("=== Running fds_path tests ===\n\n");

    // --- 1. Аналіз шляхів ---
    printf("[1/3] Testing Path Analysis...\n");
    
    fds_string_view path1 = fds_sv("src/modules/sys/main.c");
    ASSERT_SV_EQ(fds_path_basename(path1), "main.c");
    ASSERT_SV_EQ(fds_path_dirname(path1), "src/modules/sys");
    ASSERT_SV_EQ(fds_path_ext(path1), ".c");
    ASSERT_SV_EQ(fds_path_stem(path1), "main");

    // Тест без папок і розширень
    fds_string_view path2 = fds_sv("Makefile");
    ASSERT_SV_EQ(fds_path_basename(path2), "Makefile");
    ASSERT_SV_EQ(fds_path_dirname(path2), "");
    ASSERT_SV_EQ(fds_path_ext(path2), "");
    ASSERT_SV_EQ(fds_path_stem(path2), "Makefile");


    // --- 2. Модифікація та склеювання ---
    printf("[2/3] Testing Path Modification & Append...\n");
    
    FdsStringBuilder sb;
    fds_sb_init(&sb);

    // Append 
    fds_path_append(&sb, fds_sv("build"));
    fds_path_append(&sb, fds_sv("obj"));
    fds_path_append(&sb, fds_sv("main.o"));
    
    // Перевірка (з урахуванням нативного роздільника)
#ifdef _WIN32
    assert(strcmp(sb.data, "build\\obj\\main.o") == 0);
#else
    assert(strcmp(sb.data, "build/obj/main.o") == 0);
#endif
    
    fds_sb_reset(&sb);
    
    // Change Ext
    fds_path_change_ext(&sb, fds_sv("src/sys/mem.c"), fds_sv(".o"));
    assert(strcmp(sb.data, "src/sys/mem.o") == 0);
    
    fds_sb_reset(&sb);
    fds_path_change_ext(&sb, fds_sv("archive.tar.gz"), fds_sv(".zip"));
    assert(strcmp(sb.data, "archive.tar.zip") == 0); // міняє тільки останнє розширення


    // --- 3. Абсолютні та Відносні ---
    printf("[3/3] Testing Absolute & Relative Paths...\n");
    fds_sb_reset(&sb);

    // Is Absolute
#ifdef _WIN32
    assert(fds_path_is_absolute(fds_sv("C:\\Windows\\System32")) == true);
    assert(fds_path_is_absolute(fds_sv("src\\main.c")) == false);
#else
    assert(fds_path_is_absolute(fds_sv("/usr/bin/clang")) == true);
    assert(fds_path_is_absolute(fds_sv("src/main.c")) == false);
#endif

    // Absolute with Collapse 
    // Імітуємо колапс (на різних ОС абсолютний корінь різний)
#ifdef _WIN32
    fds_path_absolute(&sb, fds_sv("C:\\project\\build\\..\\src\\.\\main.c"));
    assert(strcmp(sb.data, "C:\\project\\src\\main.c") == 0);
#else
    fds_path_absolute(&sb, fds_sv("/usr/local/bin/../lib/./libfds.so"));
    assert(strcmp(sb.data, "/usr/local/lib/libfds.so") == 0);
#endif
    fds_sb_reset(&sb);

    // Relative
    fds_string_view target = fds_sv("project/src/main.c");
    fds_string_view base   = fds_sv("project/build/obj");
    
    fds_path_relative_to(&sb, target, base);
    
#ifdef _WIN32
    assert(strcmp(sb.data, "..\\..\\src\\main.c") == 0);
#else
    assert(strcmp(sb.data, "../../src/main.c") == 0);
#endif

    fds_sb_free(&sb);

    printf("\n[ SUCCESS ] All fds_path tests passed! \n");
    return 0;
}