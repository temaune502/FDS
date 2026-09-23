#define FDS_WATCHER_IMPLEMENTATION
#include "fds_filewatcher.h"
#include <stdio.h>

#if defined(_WIN32)
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

int main(void) {
    fds_watcher watcher;
    const char* watch_dir = "./assets";

    if (!fds_watcher_init(&watcher, watch_dir)) {
        printf("Не вдалося відкрити watcher для директорії %s\n", watch_dir);
        return 1;
    }

    printf("Стежимо за папкою '%s' в одному потоці...\n", watch_dir);

    while(1){
        // 1. Неблокуюче опитування ОС
        fds_watcher_poll(&watcher);

        // 2. Обробка всіх зібраних подій з черги
        fds_watch_event ev;
        while (fds_watcher_pop_event(&watcher, &ev)) {
            const char* action_str = "ЗМІНЕНО";
            if (ev.action == FDS_WATCH_ACTION_CREATED) action_str = "СТВОРЕНО";
            if (ev.action == FDS_WATCH_ACTION_DELETED) action_str = "ВИДАЛЕНО";
            if (ev.action == FDS_WATCH_ACTION_RENAMED) action_str = "ПЕРЕЙМЕНОВАНО";

            printf("[EVENT] Файл: %s | Дія: %s\n", ev.path, action_str);
        }

        // Імітація роботи кадру (наприклад, 60 FPS = ~16ms)
        sleep_ms(16);
    }

    fds_watcher_destroy(&watcher);
    return 0;
}