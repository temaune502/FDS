typedef enum
{
    FDS_MAP_READ = 1 << 0,
    FDS_MAP_READ_WRITE = 1 << 1,
} FdsMapFlags;

typedef struct
{
    void *data;               // Вказівник на початок проєкції в RAM
    usize size;              // Точний розмір файлу в байтах
    uptr file_handle;    // OS file handle (s32 fd або HANDLE)
    uptr mapping_handle; // Потрібен тільки для Windows (HANDLE), на POSIX = 0
    bool is_valid;
} FdsMappedFile;

FdsMappedFile fds_file_map(const char *path, u32 flags);
void fds_file_unmap(FdsMappedFile *mapped);
void fds_file_flush_mapped(FdsMappedFile *mapped);
FILE *fds_fmemopen_win32(void *buf, usize size, const char *mode);
FdsFile fds_file_from_mapped(FdsMappedFile *mapped);
SV fds_file_mapped_as_sv(FdsMappedFile *mapped);





#if defined(_WIN32)
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

    FdsMappedFile fds_file_map(const char *path, u32 flags)
    {
        FdsMappedFile mapped zeroe;
        if (!path)
            return mapped;

        bool writable = (flags & FDS_MAP_READ_WRITE) != 0;

#if defined(_WIN32)
        DWORD access = GENERIC_READ | (writable ? GENERIC_WRITE : 0);
        DWORD share = FILE_SHARE_READ;
        DWORD page_prot = writable ? PAGE_READWRITE : PAGE_READONLY;
        DWORD map_access = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

        HANDLE hFile = CreateFileA(path, access, share, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return mapped;

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size) || file_size.QuadPart == 0)
        {
            CloseHandle(hFile);
            return mapped;
        }

        HANDLE hMapping = CreateFileMappingA(hFile, NULL, page_prot, 0, 0, NULL);
        if (!hMapping)
        {
            CloseHandle(hFile);
            return mapped;
        }

        void *ptr = MapViewOfFile(hMapping, map_access, 0, 0, 0);
        if (!ptr)
        {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return mapped;
        }

        mapped.data = ptr;
        mapped.size = (usize)file_size.QuadPart;
        mapped.file_handle = (uptr)hFile;
        mapped.mapping_handle = (uptr)hMapping;
        mapped.is_valid = true;

#else // POSIX (Linux / macOS)
        s32 open_flags = writable ? O_RDWR : O_RDONLY;
        s32 prot = PROT_READ | (writable ? PROT_WRITE : 0);

        s32 fd = open(path, open_flags);
        if (fd < 0)
            return mapped;

        struct stat st;
        if (fstat(fd, &st) < 0 || st.st_size == 0)
        {
            close(fd);
            return mapped;
        }

        void *ptr = mmap(NULL, st.st_size, prot, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED)
        {
            close(fd);
            return mapped;
        }

        mapped.data = ptr;
        mapped.size = (usize)st.st_size;
        mapped.file_handle = (uptr)fd;
        mapped.mapping_handle = 0;
        mapped.is_valid = true;
#endif

        return mapped;
    }

    void fds_file_unmap(FdsMappedFile *mapped)
    {
        if (!mapped || !mapped->is_valid)
            return;

#if defined(_WIN32)
        UnmapViewOfFile(mapped->data);
        CloseHandle((HANDLE)mapped->mapping_handle);
        CloseHandle((HANDLE)mapped->file_handle);
#else
        munmap(mapped->data, mapped->size);
        close((s32)mapped->file_handle);
#endif

        mapped->data = NULL;
        mapped->size = 0;
        mapped->is_valid = false;
    }

    // Примусове скидання модифікованих сторінок пам'яті на диск
    void fds_file_flush_mapped(FdsMappedFile *mapped)
    {
        if (!mapped || !mapped->is_valid)
            return;

#if defined(_WIN32)
        FlushViewOfFile(mapped->data, mapped->size);
#else
        msync(mapped->data, mapped->size, MS_SYNC);
#endif
    }

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>

    FILE *fds_fmemopen_win32(void *buf, usize size, const char *mode)
    {
        // 1. Створюємо анонімний "пайп" (канал) у пам'яті, який ОС сприймає як файл
        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, NULL, (DWORD)size))
            return NULL;

        // 2. Якщо в нас є початкові дані, записуємо їх у канал
        if (buf && size > 0)
        {
            DWORD written;
            WriteFile(hWrite, buf, (DWORD)size, &written, NULL);
        }
        CloseHandle(hWrite); // Закриваємо сторону запису, щоб потік знав, де кінець

        // 3. Конвертуємо Windows HANDLE у стандартний файловий дескриптор C (fd)
        s32 fd = _open_osfhandle((iptr)hRead, _O_RDONLY | _O_BINARY);
        if (fd == -1)
        {
            CloseHandle(hRead);
            return NULL;
        }

        // 4. Перетворюємо дескриптор у звичайний FILE*
        FILE *f = _fdopen(fd, mode);
        if (!f)
        {
            _close(fd);
            return NULL;
        }

        return f;
    }
#define fmemopen fds_fmemopen_win32
#endif

    // Конвертер: перетворює FdsMappedFile у звичайний FdsFile

    FdsFile fds_file_from_mapped(FdsMappedFile *mapped)
    {
        FdsFile file zeroe;
        file.handle = 0;
        file.is_valid = false;
        if (!mapped || !mapped->is_valid)
            return file;

        // Створюємо FILE* потік поверх пам'яті mmap
        FILE *f = fmemopen(mapped->data, mapped->size, "r+b");
        if (f)
        {
            file.handle = (uptr)f;
            file.is_valid = true;
        }

        return file;
    }

    // Перетворення мапленого файлу в StringView за 1 виклик
    SV fds_file_mapped_as_sv(FdsMappedFile *mapped)
    {
        if (!mapped || !mapped->is_valid)
        {
            SV sv zeroe;
            return sv;
        }
        SV sv zeroe;
        sv.count = mapped->size,
        sv.data = (const char *)mapped->data;
        return sv;
    }