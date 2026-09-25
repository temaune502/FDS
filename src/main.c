#define FDS_IMPL
#include "fds.h"

#define Fds_Cmd StringArray

void fds_go_rebuild_urselfeeee(int argc, char **argv, const char *source_path) {
    const char *binary_path = argv[0];

    // якщо у argv[0] немає .exe, а на диск воно збереглося з .exe
#ifdef _WIN32
    // Якщо у вас в FDS є перевірка на закінчення на .exe, додайте її тут,
    // оскільки для rename розширення вкрай бажане.
#endif

    // 1. Перевіряємо, чи потрібен ребілд (порівняння часу mtime вихідного коду та бінарника)
    // Замініть fds_needs_rebuild на вашу існуючу функцію
    int rebuild_is_needed = fds_needs_rebuild(binary_path, source_path);
    if (rebuild_is_needed < 0) {
        exit(1); // Помилка при перевірці файлів
    }
    if (!rebuild_is_needed) {
        return; // Ребілд не потрібен, продовжуємо виконання поточного скрипта
    }

    // Формуємо шляхи для тимчасового та резервного файлів
    char temp_binary_path[1024];
    char old_binary_path[1024];
    snprintf(temp_binary_path, sizeof(temp_binary_path), "%s.tmp", binary_path);
    snprintf(old_binary_path, sizeof(old_binary_path), "%s.old", binary_path);

    // 2. Компілюємо новий бінарник у ТИМЧАСОВИЙ файл (.tmp)
    // Якщо тут станеться синтаксична помилка, оригінальний binary_path лишиться недоторканим!
    Fds_Cmd cmd = {0};
    fds_cmd_append(&cmd, "gcc"); // Або ваш шлях до компілятора (cl.exe / gcc / clang)
    fds_cmd_append(&cmd, "-ggdb"); // Або ваш шлях до компілятора (cl.exe / gcc / clang)
    fds_cmd_append(&cmd, source_path);
    fds_cmd_append(&cmd, "-o");
    fds_cmd_append(&cmd, temp_binary_path);
    // Додайте інші необхідні флаги чи додаткові файли через ваші буфери/масиви
    fds_cmd_result resu = fds_cmd_run_ext(sa_join(&cmd, " "));
    if (resu.exit_code != 0) {
        // Компіляція провалилася — просто чистимо тимчасовий файл і падаємо
        remove(temp_binary_path);
        fds_cmd_free(&cmd);
        exit(1);
    }
    fds_cmd_result_free(&resu);
    fds_cmd_free(&cmd);

    // 3. Безпечна ротація файлів для Windows та Linux
    // На Windows не можна перезаписати файл, який зараз виконується,
    // але його можна перейменувати!
    
    remove(old_binary_path); // Видаляємо старий .old хлам, якщо залишився з минулого разу

    // Перейменовуємо поточний запущений файл у .old (звільняємо оригінальне ім'я)
    if (rename(binary_path, old_binary_path) != 0) {
        fprintf(stderr, "ERROR: Failed to rename %s to %s\n", binary_path, old_binary_path);
        remove(temp_binary_path);
        exit(1);
    }

    // Перейменовуємо свіжоскомпільований .tmp у справжній робочий бінарник
    if (rename(temp_binary_path, binary_path) != 0) {
        fprintf(stderr, "ERROR: Failed to rename temp binary to %s\n", binary_path);
        // Пробуємо відкотити назад старий файл
        rename(old_binary_path, binary_path);
        remove(temp_binary_path);
        exit(1);
    }

    // 4. Перезапускаємо новий бінарник із тими ж аргументами, що передали нам
    Fds_Cmd restart_cmd = {0};
    fds_cmd_append(&restart_cmd, binary_path);
    for (int i = 1; i < argc; ++i) {
        fds_cmd_append(&restart_cmd, argv[i]);
    }
    fds_cmd_run_detached(fds_cmd_render(&restart_cmd));
    // if (resu.exit_code != 0) {
    //     fds_cmd_free(&restart_cmd);
    //     exit(1);
    // }
    // fds_cmd_result_free(&resu);
    fds_cmd_free(&restart_cmd);

    // Успішно запустили нову версію — завершуємо старий процес
    exit(0);
}


int main(int argc, char **argv)
{
    // fds_go_rebuild_urself(argc, argv, "src/main.c");
    FDS_REBUILD_YOURSELF(argc, argv);
    fds_log(FINFO, "Hello temaune!! hahaha\n");
    
    return 0;
}