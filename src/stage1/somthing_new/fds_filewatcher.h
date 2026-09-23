#ifndef FDS_WATCHER_H
#define FDS_WATCHER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDS_WATCHER_MAX_EVENTS
#define FDS_WATCHER_MAX_EVENTS 64
#endif

typedef enum fds_watch_action {
    FDS_WATCH_ACTION_UNKNOWN = 0,
    FDS_WATCH_ACTION_MODIFIED,
    FDS_WATCH_ACTION_CREATED,
    FDS_WATCH_ACTION_DELETED,
    FDS_WATCH_ACTION_RENAMED
} fds_watch_action;

typedef struct fds_watch_event {
    char path[256];
    fds_watch_action action;
} fds_watch_event;

typedef struct fds_watcher {
    // Внутрішня кільцева черга подій
    fds_watch_event queue[FDS_WATCHER_MAX_EVENTS];
    size_t head;
    size_t tail;
    size_t count;

#if defined(_WIN32)
    void* dir_handle;       // HANDLE папки
    void* overlapped;       // OVERLAPPED структура для асинхронного I/O
    uint8_t buffer[4096];   // Буфер подій драйвера
    bool is_pending;        // Прапорець очікування I/O
#else
    int inotify_fd;         // File descriptor inotify
#endif
} fds_watcher;

// Ініціалізація та відкриття неблокуючого спостерігача
bool fds_watcher_init(fds_watcher* w, const char* dir_path);

// Неблокуюча перевірка сисколів ОС та наповнення черги (викликати щокадру)
void fds_watcher_poll(fds_watcher* w);

// Витягування події з черги (повертає false, якщо черга порожня)
bool fds_watcher_pop_event(fds_watcher* w, fds_watch_event* out_event);

// Закриття системних хендлів
void fds_watcher_destroy(fds_watcher* w);

#ifdef __cplusplus
}
#endif

#endif // FDS_WATCHER_H

// ============================================================================
// РЕАЛІЗАЦІЯ (FDS_WATCHER_IMPLEMENTATION)
// ============================================================================
#ifdef FDS_WATCHER_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/inotify.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

static void fds_watcher_push_event(fds_watcher* w, const char* path, fds_watch_action action) {
    if (w->count >= FDS_WATCHER_MAX_EVENTS) return; // Переповнення черги

    fds_watch_event* ev = &w->queue[w->tail];
    snprintf(ev->path, sizeof(ev->path), "%s", path);
    ev->action = action;

    w->tail = (w->tail + 1) % FDS_WATCHER_MAX_EVENTS;
    w->count++;
}

bool fds_watcher_init(fds_watcher* w, const char* dir_path) {
    memset(w, 0, sizeof(fds_watcher));

#if defined(_WIN32)
    HANDLE hDir = CreateFileA(
        dir_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) return false;

    OVERLAPPED* ov = (OVERLAPPED*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(OVERLAPPED));
    if (!ov) {
        CloseHandle(hDir);
        return false;
    }

    w->dir_handle = (void*)hDir;
    w->overlapped = (void*)ov;
    w->is_pending = false;
    return true;
#else
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) return false;

    int wd = inotify_add_watch(fd, dir_path, IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE);
    if (wd < 0) {
        close(fd);
        return false;
    }

    w->inotify_fd = fd;
    return true;
#endif
}

void fds_watcher_poll(fds_watcher* w) {
#if defined(_WIN32)
    HANDLE hDir = (HANDLE)w->dir_handle;
    OVERLAPPED* ov = (OVERLAPPED*)w->overlapped;

    // Якщо асинхронний запит ще не відправлено — відправляємо
    if (!w->is_pending) {
        DWORD bytes_returned = 0;
        BOOL ok = ReadDirectoryChangesW(
            hDir,
            w->buffer,
            sizeof(w->buffer),
            TRUE, // Підпапки
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            &bytes_returned,
            ov,
            NULL
        );

        if (ok || GetLastError() == ERROR_IO_PENDING) {
            w->is_pending = true;
        } else {
            return;
        }
    }

    // Перевіряємо статус без блокування потоку (bWait = FALSE)
    DWORD bytes_transferred = 0;
    if (GetOverlappedResult(hDir, ov, &bytes_transferred, FALSE)) {
        w->is_pending = false; // Запит виконано

        if (bytes_transferred > 0) {
            FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)w->buffer;
            while (info) {
                char filename[256] = {0};
                WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength / sizeof(WCHAR), filename, sizeof(filename) - 1, NULL, NULL);

                fds_watch_action action = FDS_WATCH_ACTION_UNKNOWN;
                switch (info->Action) {
                    case FILE_ACTION_MODIFIED:          action = FDS_WATCH_ACTION_MODIFIED; break;
                    case FILE_ACTION_ADDED:             action = FDS_WATCH_ACTION_CREATED; break;
                    case FILE_ACTION_REMOVED:           action = FDS_WATCH_ACTION_DELETED; break;
                    case FILE_ACTION_RENAMED_OLD_NAME: 
                    case FILE_ACTION_RENAMED_NEW_NAME:  action = FDS_WATCH_ACTION_RENAMED; break;
                }

                if (action != FDS_WATCH_ACTION_UNKNOWN) {
                    fds_watcher_push_event(w, filename, action);
                }

                if (info->NextEntryOffset == 0) break;
                info = (FILE_NOTIFY_INFORMATION*)((uint8_t*)info + info->NextEntryOffset);
            }
        }

        // Перезапускаємо Overlapped спостереження
        memset(ov, 0, sizeof(OVERLAPPED));
    }
#else
    uint8_t buffer[4096];
    ssize_t len = read(w->inotify_fd, buffer, sizeof(buffer));
    if (len <= 0) return; // Немає нових подій або EAGAIN

    const struct inotify_event* event;
    for (uint8_t* ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
        event = (const struct inotify_event*)ptr;

        if (event->len > 0) {
            fds_watch_action action = FDS_WATCH_ACTION_UNKNOWN;
            if (event->mask & IN_MODIFY)      action = FDS_WATCH_ACTION_MODIFIED;
            else if (event->mask & IN_CREATE) action = FDS_WATCH_ACTION_CREATED;
            else if (event->mask & IN_DELETE) action = FDS_WATCH_ACTION_DELETED;
            else if (event->mask & (IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO)) action = FDS_WATCH_ACTION_RENAMED;

            if (action != FDS_WATCH_ACTION_UNKNOWN) {
                fds_watcher_push_event(w, event->name, action);
            }
        }
    }
#endif
}

bool fds_watcher_pop_event(fds_watcher* w, fds_watch_event* out_event) {
    if (w->count == 0) return false;

    *out_event = w->queue[w->head];
    w->head = (w->head + 1) % FDS_WATCHER_MAX_EVENTS;
    w->count--;
    return true;
}

void fds_watcher_destroy(fds_watcher* w) {
#if defined(_WIN32)
    if (w->dir_handle) CloseHandle((HANDLE)w->dir_handle);
    if (w->overlapped) HeapFree(GetProcessHeap(), 0, w->overlapped);
#else
    if (w->inotify_fd >= 0) close(w->inotify_fd);
#endif
    memset(w, 0, sizeof(fds_watcher));
}

#endif // FDS_WATCHER_IMPLEMENTATION