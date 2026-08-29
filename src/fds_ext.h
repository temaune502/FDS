#ifndef FDS_EXT_H
#define FDS_EXT_H

#include <stdlib.h>
#include <stdio.h>

#include <stddef.h>
#include <stdbool.h>

#include <ctype.h>


typedef enum {
    FDS_FILE_READ   = 1 << 0,
    FDS_FILE_WRITE  = 1 << 1,
    FDS_FILE_CREATE = 1 << 2,
    FDS_FILE_APPEND = 1 << 3,
} FdsFileFlags;

#pragma pack(push, 1)
typedef struct {
    char magic[4];    // 4-байтний ідентифікатор формату
    uint32_t version; // Версія структури даних
} FdsFileHeader;
#pragma pack(pop)


typedef enum {
    FDS_SEEK_SET = 0,
    FDS_SEEK_CUR = 1,
    FDS_SEEK_END = 2,
} FdsSeekOrigin;

typedef struct {
    uintptr_t handle;
    bool is_valid;
} FdsFile;

typedef enum {
    FDS_MAP_READ       = 1 << 0,
    FDS_MAP_READ_WRITE = 1 << 1,
} FdsMapFlags;

typedef struct {
    void     *data;           // Вказівник на початок проєкції в RAM
    size_t    size;           // Точний розмір файлу в байтах
    uintptr_t file_handle;    // OS file handle (int fd або HANDLE)
    uintptr_t mapping_handle; // Потрібен тільки для Windows (HANDLE), на POSIX = 0
    bool      is_valid;
} FdsMappedFile;

#define FDS_FILE_PTR(f) ((FILE*)(f).handle)

// Читає весь бінарний файл у виділений пам'яттю буфер.
// Пам'ять для *out_buf виділяється через malloc(), її потрібно звільнити (free).
bool fds_file_read_bytes(const char *filepath, void **out_buf, size_t *out_size);

// Створює новий файл (або перезаписує існуючий) і записує туди size байт з buf.
bool fds_file_write_bytes(const char *filepath, const void *buf, size_t size);

// Додає size байт з buf у кінець файлу.
bool fds_file_append_bytes(const char *filepath, const void *buf, size_t size);

bool fds_da_write_file(const char *filepath, const void *items, size_t count, size_t item_size);

bool fds_da_read_file(const char *filepath, void **out_items, size_t *out_count, size_t item_size);

#define fds_da_write(filepath, da) \
    fds_da_write_file((filepath), (da)->items, (da)->count, sizeof(*(da)->items))

#define fds_da_read(filepath, da) \
    fds_da_read_file((filepath), (void**)&((da)->items), &((da)->count), sizeof(*(da)->items))


// --- МАКРОСИ ДЛЯ ЗАПИСУ ---
// Записує значення змінної (передається сама змінна, макрос сам бере її адресу &)
#define fds_file_write_val(file, val) fds_file_write((file), &(val), sizeof(val))

// --- МАКРОСИ ДЛЯ ЧИТАННЯ ---
// Читає дані безпосередньо у змінну за її адресою
#define fds_file_read_val(file, val_ptr) fds_file_read((file), (val_ptr), sizeof(*(val_ptr)))

#define fds_file_skip_type(file, type) fds_file_skip((file),  (int64_t)sizeof((type)))

// Читає дані та повертає їх як результат (зручно для присвоєння: x = fds_file_get(f, int))
#define fds_file_get(file, type) \
    ({ type _tmp; fds_file_read((file), &_tmp, sizeof(type)) == sizeof(type) ? _tmp : (type){0}; })


#define DA_FIELDS size_t count; size_t capacity

SV sv_chop_by_delim(SV *sv, char delim);
SV sv_trim_ext(SV sv);
SV  sv_trim_right_ext(SV sv);
SV  sv_trim_left_ext(SV sv);

SV sv_chop_right(SV *sv, size_t n);
SV sv_chop_left(SV *sv, size_t n);




FdsFile fds_file_open(const char *path, uint32_t flags);
void    fds_file_close(FdsFile *file);

size_t  fds_file_read(FdsFile file, void *dst, size_t size);
size_t  fds_file_write(FdsFile file, const void *src, size_t size);

bool    fds_file_seek(FdsFile file, int64_t offset, FdsSeekOrigin origin);
int64_t fds_file_tell(FdsFile file);
int64_t fds_file_size(FdsFile file);
void    fds_file_flush(FdsFile file);

char* fds_file_read_str(FdsFile file, void* (*allocator)(size_t));
bool fds_file_write_str(FdsFile file, const char *str);

bool fds_file_skip(FdsFile file, int64_t bytes_to_skip);

bool fds_file_check_magic(FdsFile *file, const char expected_magic[4], uint32_t min_version);
bool fds_file_write_magic(FdsFile *file, const char magic[4], uint32_t version);

FdsMappedFile fds_file_map(const char *path, uint32_t flags);
void fds_file_unmap(FdsMappedFile *mapped);
void fds_file_flush_mapped(FdsMappedFile *mapped);
FILE* fds_fmemopen_win32(void *buf, size_t size, const char *mode);
FdsFile fds_file_from_mapped(FdsMappedFile *mapped);
SV fds_file_mapped_as_sv(FdsMappedFile *mapped);



#ifdef FDS_EXT_IMPL



SV sv_chop_by_delim(SV *sv, char delim)
{
    size_t i = 0;
    while (i < sv->count && sv->data[i] != delim) {
        i += 1;
    }

    SV result = sv_from_parts(sv->data, i);

    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->count -= i;
        sv->data  += i;
    }

    return result;
}



SV sv_chop_left(SV *sv, size_t n)
{
    if (n > sv->count) {
        n = sv->count;
    }

    SV result = sv_from_parts(sv->data, n);

    sv->data  += n;
    sv->count -= n;

    return result;
}

SV sv_chop_right(SV *sv, size_t n)
{
    if (n > sv->count) {
        n = sv->count;
    }

    SV result = sv_from_parts(sv->data + sv->count - n, n);

    sv->count -= n;

    return result;
}



SV  sv_trim_left_ext(SV sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i])) {
        i += 1;
    }

    return sv_from_parts(sv.data + i, sv.count - i);
}

SV  sv_trim_right_ext(SV sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
        i += 1;
    }

    return sv_from_parts(sv.data, sv.count - i);
}

SV sv_trim_ext(SV sv)
{
    return sv_trim_right_ext(sv_trim_left_ext(sv));
}


bool fds_da_write_file(const char *filepath, const void *items, size_t count, size_t item_size)
{
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    // 1. Записуємо кількість елементів (заголовок)
    if (fwrite(&count, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return false;
    }

    // 2. Записуємо самі елементи з купи
    if (count > 0 && items != NULL) {
        if (fwrite(items, item_size, count, f) != count) {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}




bool fds_da_read_file(const char *filepath, void **out_items, size_t *out_count, size_t item_size)
{
    if (!filepath || !out_items || !out_count) return false;

    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    // 1. Читаємо кількість елементів
    size_t count = 0;
    if (fread(&count, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return false;
    }

    if (count == 0) {
        *out_items = NULL;
        *out_count = 0;
        fclose(f);
        return true;
    }

    // 2. Виділяємо пам'ять під буфер
    void *items = malloc(count * item_size);
    if (!items) {
        fclose(f);
        return false;
    }

    // 3. Зчитуємо елементи
    if (fread(items, item_size, count, f) != count) {
        free(items);
        fclose(f);
        return false;
    }

    fclose(f);
    *out_items = items;
    *out_count = count;
    return true;
}







bool fds_file_read_bytes(const char *filepath, void **out_buf, size_t *out_size)
{
    if (!filepath || !out_buf || !out_size) return false;

    *out_buf = NULL;
    *out_size = 0;

    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    // Отримуємо розмір файлу
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return false;
    }

    fseek(f, 0, SEEK_SET);

    // Якщо файл порожній — повертаємо успіх із NULL буфером
    if (size == 0) {
        fclose(f);
        return true;
    }

    void *buffer = malloc((size_t)size);
    if (!buffer) {
        fclose(f);
        return false;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (bytes_read != (size_t)size) {
        free(buffer);
        return false;
    }

    *out_buf = buffer;
    *out_size = (size_t)size;
    return true;
}

bool fds_file_write_bytes(const char *filepath, const void *buf, size_t size)
{
    if (!filepath) return false;
    if (size > 0 && !buf) return false;

    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    if (size > 0) {
        size_t bytes_written = fwrite(buf, 1, size, f);
        if (bytes_written != size) {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

bool fds_file_append_bytes(const char *filepath, const void *buf, size_t size)
{
    if (!filepath) return false;
    if (size == 0) return true;
    if (!buf) return false;

    FILE *f = fopen(filepath, "ab");
    if (!f) return false;

    size_t bytes_written = fwrite(buf, 1, size, f);
    fclose(f);

    return bytes_written == size;
}


FdsFile fds_file_open(const char *path, uint32_t flags) {
    FdsFile file = { .handle = 0, .is_valid = false };
    if (!path) return file;

    const char *mode = "rb";

    if ((flags & FDS_FILE_READ) && (flags & FDS_FILE_WRITE)) {
        if (flags & FDS_FILE_CREATE)      mode = "w+b";
        else if (flags & FDS_FILE_APPEND) mode = "a+b";
        else                              mode = "r+b";
    } else if (flags & FDS_FILE_WRITE) {
        if (flags & FDS_FILE_APPEND)      mode = "ab";
        else                              mode = "wb";
    } else if (flags & FDS_FILE_APPEND) {
        mode = "ab";
    }

    FILE *f = fopen(path, mode);
    if (f) {
        file.handle = (uintptr_t)f;
        file.is_valid = true;
    }

    return file;
}

 void fds_file_close(FdsFile *file) {
    if (!file || !file->is_valid) return;

    FILE *f = FDS_FILE_PTR(*file);
    if (f) fclose(f);

    file->handle = 0;
    file->is_valid = false;
}

 size_t fds_file_read(FdsFile file, void *dst, size_t size) {
    if (!file.is_valid || !dst || size == 0) return 0;
    return fread(dst, 1, size, FDS_FILE_PTR(file));
}

 size_t fds_file_write(FdsFile file, const void *src, size_t size) {
    if (!file.is_valid || !src || size == 0) return 0;
    return fwrite(src, 1, size, FDS_FILE_PTR(file));
}

bool fds_file_seek(FdsFile file, int64_t offset, FdsSeekOrigin origin) {
    if (!file.is_valid) return false;

    int std_origin = SEEK_SET;
    if (origin == FDS_SEEK_CUR) std_origin = SEEK_CUR;
    if (origin == FDS_SEEK_END) std_origin = SEEK_END;

#if defined(_WIN32)
    return _fseeki64(FDS_FILE_PTR(file), offset, std_origin) == 0;
#else
    return fseeko(FDS_FILE_PTR(file), (off_t)offset, std_origin) == 0;
#endif
}

 int64_t fds_file_tell(FdsFile file) {
    if (!file.is_valid) return -1;

#if defined(_WIN32)
    return _ftelli64(FDS_FILE_PTR(file));
#else
    return (int64_t)ftello(FDS_FILE_PTR(file));
#endif
}

 int64_t fds_file_size(FdsFile file) {
    if (!file.is_valid) return -1;

    int64_t current = fds_file_tell(file);
    if (current < 0) return -1;

    if (!fds_file_seek(file, 0, FDS_SEEK_END)) return -1;

    int64_t size = fds_file_tell(file);
    fds_file_seek(file, current, FDS_SEEK_SET);

    return size;
}

 void fds_file_flush(FdsFile file) {
    if (!file.is_valid) return;
    fflush(FDS_FILE_PTR(file));
}

 bool fds_file_write_magic(FdsFile *file, const char magic[4], uint32_t version) {
    if (!file || !file->is_valid) return false;

    FdsFileHeader header;
    memcpy(header.magic, magic, 4);
    header.version = version;

    size_t written = fds_file_write(*file, &header, sizeof(FdsFileHeader));
    return written == sizeof(FdsFileHeader);
}

bool fds_file_skip(FdsFile file, int64_t bytes_to_skip) {
    // Якщо просити пропустити 0 байт — це успіх, нічого робити не треба
    if (bytes_to_skip == 0) return true;
    
    // Забороняємо відмотувати файл назад через skip (для цього є чіткий seek)
    if (bytes_to_skip < 0) return false; 

    return fds_file_seek(file, bytes_to_skip, FDS_SEEK_CUR);
}

bool fds_file_check_magic(FdsFile *file, const char expected_magic[4], uint32_t min_version) {
    if (!file || !file->is_valid) return false;

    FdsFileHeader header = {0};
    size_t read_bytes = fds_file_read(*file, &header, sizeof(FdsFileHeader));

    if (read_bytes != sizeof(FdsFileHeader)) {
        return false;
    }

    if (memcmp(header.magic, expected_magic, 4) != 0) {
        return false;
    }

    if (header.version < min_version) {
        return false;
    }

    return true;
}

bool fds_file_write_str(FdsFile file, const char *str) {
    if (!str) return false;
    uint32_t len = (uint32_t)strlen(str);
    if (fds_file_write(file, &len, sizeof(len)) != sizeof(len)) return false;
    return fds_file_write(file, str, len) == len;
}

// Повертає зліпок рядка 
char* fds_file_read_str(FdsFile file, void* (*allocator)(size_t)) {
    uint32_t len = 0;
    if (fds_file_read(file, &len, sizeof(len)) != sizeof(len)) return NULL;
    char *buf = allocator(len + 1);
    if (!buf) return NULL;
    if (fds_file_read(file, buf, len) != len) return NULL;
    buf[len] = '\0';
    return buf;
}

#if defined(_WIN32)
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

FdsMappedFile fds_file_map(const char *path, uint32_t flags) {
    FdsMappedFile mapped = {0};
    if (!path) return mapped;

    bool writable = (flags & FDS_MAP_READ_WRITE) != 0;

#if defined(_WIN32)
    DWORD access = GENERIC_READ | (writable ? GENERIC_WRITE : 0);
    DWORD share  = FILE_SHARE_READ;
    DWORD page_prot = writable ? PAGE_READWRITE : PAGE_READONLY;
    DWORD map_access = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

    HANDLE hFile = CreateFileA(path, access, share, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return mapped;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size) || file_size.QuadPart == 0) {
        CloseHandle(hFile);
        return mapped;
    }

    HANDLE hMapping = CreateFileMappingA(hFile, NULL, page_prot, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return mapped;
    }

    void *ptr = MapViewOfFile(hMapping, map_access, 0, 0, 0);
    if (!ptr) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return mapped;
    }

    mapped.data = ptr;
    mapped.size = (size_t)file_size.QuadPart;
    mapped.file_handle = (uintptr_t)hFile;
    mapped.mapping_handle = (uintptr_t)hMapping;
    mapped.is_valid = true;

#else // POSIX (Linux / macOS)
    int open_flags = writable ? O_RDWR : O_RDONLY;
    int prot = PROT_READ | (writable ? PROT_WRITE : 0);

    int fd = open(path, open_flags);
    if (fd < 0) return mapped;

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size == 0) {
        close(fd);
        return mapped;
    }

    void *ptr = mmap(NULL, st.st_size, prot, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        return mapped;
    }

    mapped.data = ptr;
    mapped.size = (size_t)st.st_size;
    mapped.file_handle = (uintptr_t)fd;
    mapped.mapping_handle = 0;
    mapped.is_valid = true;
#endif

    return mapped;
}

void fds_file_unmap(FdsMappedFile *mapped) {
    if (!mapped || !mapped->is_valid) return;

#if defined(_WIN32)
    UnmapViewOfFile(mapped->data);
    CloseHandle((HANDLE)mapped->mapping_handle);
    CloseHandle((HANDLE)mapped->file_handle);
#else
    munmap(mapped->data, mapped->size);
    close((int)mapped->file_handle);
#endif

    mapped->data = NULL;
    mapped->size = 0;
    mapped->is_valid = false;
}

// Примусове скидання модифікованих сторінок пам'яті на диск
void fds_file_flush_mapped(FdsMappedFile *mapped) {
    if (!mapped || !mapped->is_valid) return;

#if defined(_WIN32)
    FlushViewOfFile(mapped->data, mapped->size);
#else
    msync(mapped->data, mapped->size, MS_SYNC);
#endif
}



#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>

FILE* fds_fmemopen_win32(void *buf, size_t size, const char *mode) {
    // 1. Створюємо анонімний "пайп" (канал) у пам'яті, який ОС сприймає як файл
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, NULL, (DWORD)size)) return NULL;

    // 2. Якщо в нас є початкові дані, записуємо їх у канал
    if (buf && size > 0) {
        DWORD written;
        WriteFile(hWrite, buf, (DWORD)size, &written, NULL);
    }
    CloseHandle(hWrite); // Закриваємо сторону запису, щоб потік знав, де кінець

    // 3. Конвертуємо Windows HANDLE у стандартний файловий дескриптор C (fd)
    int fd = _open_osfhandle((intptr_t)hRead, _O_RDONLY | _O_BINARY);
    if (fd == -1) {
        CloseHandle(hRead);
        return NULL;
    }

    // 4. Перетворюємо дескриптор у звичайний FILE*
    FILE *f = _fdopen(fd, mode);
    if (!f) {
        _close(fd); 
        return NULL;
    }

    return f;
}
#define fmemopen fds_fmemopen_win32
#endif


// Конвертер: перетворює FdsMappedFile у звичайний FdsFile

FdsFile fds_file_from_mapped(FdsMappedFile *mapped) {
    FdsFile file = { .handle = 0, .is_valid = false };
    if (!mapped || !mapped->is_valid) return file;

    // Створюємо FILE* потік поверх пам'яті mmap
    FILE *f = fmemopen(mapped->data, mapped->size, "rwb");
    if (f) {
        file.handle = (uintptr_t)f;
        file.is_valid = true;
    }

    return file;
}


// Перетворення мапленого файлу в StringView за 1 виклик
SV fds_file_mapped_as_sv(FdsMappedFile *mapped) {
    if (!mapped || !mapped->is_valid) return (SV){0};
    return (SV){
        .data  = (const char*)mapped->data,
        .count = mapped->size
    };
}
#endif // FDS_EXT_IMPL
#endif // FDS_EXT_H