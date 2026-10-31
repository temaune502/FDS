#ifndef FDS_MAP_H
#define FDS_MAP_H

typedef enum
{
    FDS_MAP_READ = 1u << 0,
    FDS_MAP_WRITE = 1u << 1,
    FDS_MAP_EXECUTE = 1u << 2,

    FDS_MAP_READ_WRITE = FDS_MAP_READ | FDS_MAP_WRITE,
} FdsMapFlags;

typedef struct
{
    void *data;
    usize size;

    uptr file_handle;
    uptr mapping_handle;

    u32 flags;
    bool is_valid;
} FdsMappedFile;

/*
 * Map an existing file into memory.
 *
 * READ     - memory can be read.
 * WRITE    - memory can be modified.
 * EXECUTE  - memory can contain executable code.
 *
 * EXECUTE requires READ.
 * WRITE mappings use shared file-backed memory.
 *
 * Returns an invalid, zero-initialized structure on failure.
 */
FdsMappedFile fds_file_map(const char *path, u32 flags);

/* Flush modifications to the underlying file. */
bool fds_file_flush_mapped(FdsMappedFile *mapped);

/* Release the mapping and associated OS handles. */
void fds_file_unmap(FdsMappedFile *mapped);

/* Obtain a read-only string view of the mapped bytes. */
SV fds_file_mapped_as_sv(const FdsMappedFile *mapped);

#endif /* FDS_MAP_H */

/* ============================================================
 * Implementation
 *
 * Define FDS_MAP_IMPLEMENTATION in exactly one C translation
 * unit before including this header.
 * ============================================================ */

#ifdef FDS_MAP_IMPLEMENTATION

#ifndef FDS_MAP_IMPLEMENTATION_INCLUDED
#define FDS_MAP_IMPLEMENTATION_INCLUDED

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#endif

static bool fds_map_flags_valid(u32 flags)
{
    const u32 known_flags =
        FDS_MAP_READ |
        FDS_MAP_WRITE |
        FDS_MAP_EXECUTE;

    if ((flags & ~known_flags) != 0)
        return false;

    if ((flags & FDS_MAP_EXECUTE) &&
        !(flags & FDS_MAP_READ))
        return false;

    if (!(flags & (FDS_MAP_READ |
                   FDS_MAP_WRITE)))
        return false;

    return true;
}

FdsMappedFile fds_file_map(const char *path, u32 flags)
{
    FdsMappedFile mapped = {0};

    if (!path || !path[0])
        return mapped;

    if (!fds_map_flags_valid(flags))
        return mapped;

    const bool writable = (flags & FDS_MAP_WRITE) != 0;
    const bool executable = (flags & FDS_MAP_EXECUTE) != 0;

#if defined(_WIN32)

    DWORD file_access = GENERIC_READ;

    if (writable)
        file_access |= GENERIC_WRITE;

    /*
     * Allow other readers and writers to open the file.
     * Windows still enforces the access rights of the mapping.
     */
    HANDLE file = CreateFileA(
        path,
        file_access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (file == INVALID_HANDLE_VALUE)
        return mapped;

    LARGE_INTEGER file_size;

    if (!GetFileSizeEx(file, &file_size) ||
        file_size.QuadPart <= 0 ||
        (uint64_t)file_size.QuadPart > (uint64_t)SIZE_MAX)
    {
        CloseHandle(file);
        return mapped;
    }

    DWORD protection;

    if (executable)
    {
        protection = writable
                         ? PAGE_EXECUTE_READWRITE
                         : PAGE_EXECUTE_READ;
    }
    else
    {
        protection = writable
                         ? PAGE_READWRITE
                         : PAGE_READONLY;
    }

    HANDLE mapping = CreateFileMappingA(
        file,
        NULL,
        protection,
        0,
        0,
        NULL);

    if (!mapping)
    {
        CloseHandle(file);
        return mapped;
    }

    DWORD view_access = 0;

    if (flags & FDS_MAP_READ)
        view_access |= FILE_MAP_READ;

    if (writable)
        view_access |= FILE_MAP_WRITE;

    if (executable)
        view_access |= FILE_MAP_EXECUTE;

    void *data = MapViewOfFile(
        mapping,
        view_access,
        0,
        0,
        0);

    if (!data)
    {
        CloseHandle(mapping);
        CloseHandle(file);
        return mapped;
    }

    mapped.data = data;
    mapped.size = (usize)file_size.QuadPart;
    mapped.file_handle = (uptr)file;
    mapped.mapping_handle = (uptr)mapping;
    mapped.flags = flags;
    mapped.is_valid = true;

#else

    int open_flags = writable ? O_RDWR : O_RDONLY;

    int fd = open(path, open_flags);

    if (fd < 0)
        return mapped;

    struct stat st;

    if (fstat(fd, &st) != 0 ||
        !S_ISREG(st.st_mode) ||
        st.st_size <= 0 ||
        (uintmax_t)st.st_size > (uintmax_t)SIZE_MAX)
    {
        close(fd);
        return mapped;
    }

    int protection = 0;

    if (flags & FDS_MAP_READ)
        protection |= PROT_READ;

    if (writable)
        protection |= PROT_WRITE;

    if (executable)
        protection |= PROT_EXEC;

    void *data = mmap(
        NULL,
        (usize)st.st_size,
        protection,
        MAP_SHARED,
        fd,
        0);

    if (data == MAP_FAILED)
    {
        close(fd);
        return mapped;
    }

    mapped.data = data;
    mapped.size = (usize)st.st_size;
    mapped.file_handle = (uptr)fd;
    mapped.mapping_handle = 0;
    mapped.flags = flags;
    mapped.is_valid = true;

#endif

    return mapped;
}

bool fds_file_flush_mapped(FdsMappedFile *mapped)
{
    if (!mapped ||
        !mapped->is_valid ||
        !mapped->data ||
        mapped->size == 0 ||
        !(mapped->flags & FDS_MAP_WRITE))
    {
        return false;
    }

#if defined(_WIN32)

    if (!FlushViewOfFile(mapped->data, mapped->size))
        return false;

    /*
     * FlushViewOfFile writes dirty mapped pages.
     * FlushFileBuffers additionally requests a flush of the
     * underlying file's buffered data.
     */
    if (!FlushFileBuffers((HANDLE)mapped->file_handle))
        return false;

    return true;

#else

    if (msync(mapped->data, mapped->size, MS_SYNC) != 0)
        return false;

    return true;

#endif
}

void fds_file_unmap(FdsMappedFile *mapped)
{
    if (!mapped)
        return;

    if (mapped->is_valid)
    {
#if defined(_WIN32)

        UnmapViewOfFile(mapped->data);

        if (mapped->mapping_handle)
            CloseHandle((HANDLE)mapped->mapping_handle);

        if (mapped->file_handle)
            CloseHandle((HANDLE)mapped->file_handle);

#else

        munmap(mapped->data, mapped->size);
        close((int)mapped->file_handle);

#endif
    }

    /*
     * Reset everything, including handles and flags.
     * Calling unmap again is safe.
     */
    *mapped = (FdsMappedFile){0};
}

SV fds_file_mapped_as_sv(const FdsMappedFile *mapped)
{
    SV sv = {0};

    if (!mapped || !mapped->is_valid)
        return sv;

    sv.data = (const char *)mapped->data;
    sv.count = mapped->size;

    return sv;
}

#endif /* FDS_MAP_IMPLEMENTATION_INCLUDED */
#endif /* FDS_MAP_IMPLEMENTATION */