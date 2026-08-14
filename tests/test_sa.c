#define FDS_IMPLEMENTATION
#include "fds.h"




int main() {
    StringArray cmd = {0};
    SA_INIT(&cmd, "ffmpeg", "-i", "input.mp4", "-vf", "scale=1280:720",
            "output.mp4");
    
    char* project_dir = "urmom";
    sa_pushf(&cmd, "-I%s/include", project_dir);
    printf("Команда:\n");
    SA_PRINT_LINES(&cmd);

    // Перевірити, чи є прапор -vf
    if (sa_contains(&cmd, "-vf"))
        printf("\nЗнайдено відеофільтр!\n");
    
    // Вставити додаткові опції після "-i"
    size_t pos = sa_find(&cmd, "-i");
    if (pos != (size_t)-1) {
        sa_insert(&cmd, pos + 1, "-r");
        sa_insert(&cmd, pos + 2, "30");
    }

    // Об'єднати в один рядок для журналу
    char *full_command = sa_join(&cmd, " ");
    printf("Повна команда: %s\n", full_command);
    free(full_command);

    // Створити копію, відсортувати і вивести
    StringArray sorted;
    sa_copy(&sorted, &cmd);
    sa_sort(&sorted, NULL);
    printf("\nВідсортовані аргументи:\n");
    SA_PRINT(&sorted);

    // Розібрати рядок з PATH
    StringArray paths;
    sa_init(&paths, 4);
    sa_split(&paths, "/usr/bin:/bin:/usr/local/bin", ":", true);
    printf("\nКаталоги PATH:\n");
    SA_PRINT_LINES(&paths);

    sa_free(&cmd);
    sa_free(&sorted);
    sa_free(&paths);
    return 0;
}