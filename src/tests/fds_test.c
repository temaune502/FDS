#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef FDS_MALLOC
#define FDS_MALLOC(size) malloc(size)
#endif

#ifndef FDS_FREE
#define FDS_FREE(ptr) free(ptr)
#endif

#ifndef FDS_ASSERT
#define FDS_ASSERT(condition, message) assert((condition) && (message))
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static FILE *fds_utf8_fopen_read(const char *filepath)
{
    int wide_length;
    wchar_t *wide_filepath;
    FILE *file;

    FDS_ASSERT(filepath != NULL, "filepath must not be NULL");

    wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                      filepath, -1, NULL, 0);
    if (wide_length <= 0)
        return NULL;

    wide_filepath = (wchar_t *)FDS_MALLOC((size_t)wide_length * sizeof(*wide_filepath));
    if (!wide_filepath)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            filepath, -1, wide_filepath, wide_length) <= 0)
    {
        FDS_FREE(wide_filepath);
        return NULL;
    }

    file = _wfopen(wide_filepath, L"rb");
    FDS_FREE(wide_filepath);
    return file;
}
#else
static FILE *fds_utf8_fopen_read(const char *filepath)
{
    FDS_ASSERT(filepath != NULL, "filepath must not be NULL");
    return fopen(filepath, "rb");
}
#endif

/* Reads UTF-8 text without changing its bytes and adds a trailing NUL. */
bool fds_file_read_utf8(const char *filepath, char **out_text, size_t *out_size)
{
    FILE *file;
    long file_size;
    char *text;
    size_t bytes_read;

    FDS_ASSERT(filepath != NULL, "filepath must not be NULL");
    FDS_ASSERT(out_text != NULL, "out_text must not be NULL");
    FDS_ASSERT(out_size != NULL, "out_size must not be NULL");

    *out_text = NULL;
    *out_size = 0;

    file = fds_utf8_fopen_read(filepath);
    if (!file)
        return false;

    if (fseek(file, 0, SEEK_END) != 0)
        goto fail;

    file_size = ftell(file);
    if (file_size < 0)
        goto fail;

    if (fseek(file, 0, SEEK_SET) != 0)
        goto fail;

    text = (char *)FDS_MALLOC((size_t)file_size + 1);
    if (!text)
        goto fail;

    bytes_read = fread(text, 1, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size)
    {
        FDS_FREE(text);
        goto fail;
    }

    text[bytes_read] = '\0';
    fclose(file);
    *out_text = text;
    *out_size = bytes_read;
    return true;

fail:
    fclose(file);
    return false;
}

void fds_file_read_utf8_free(char *text)
{
    FDS_FREE(text);
}
