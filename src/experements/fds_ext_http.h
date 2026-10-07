/*
    fds_ext_http.h — Кросплатформна HTTP/HTTPS обгортка для FDS.
    
    На Windows використовує нативний WinINet API.
    На Linux/macOS використовує libcurl.

    В ОДНОМУ C-файлі вашого проєкту:
        #define FDS_EXT_HTTP_IMPL
        #include "fds_ext_http.h"
*/

#ifndef FDS_EXT_HTTP_H
#define FDS_EXT_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef FDS_EXT_HTTP_DEF
    #define FDS_EXT_HTTP_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   Коди результатів
   ============================================================================ */
typedef enum {
    FDS_HTTP_OK = 0,
    FDS_HTTP_INVALID_ARGUMENT,
    FDS_HTTP_NOT_INITIALIZED,
    FDS_HTTP_INIT_FAILED,
    FDS_HTTP_NETWORK_ERROR,
    FDS_HTTP_FILE_ERROR,
    FDS_HTTP_HTTP_ERROR,
    FDS_HTTP_TOO_LARGE,
    FDS_HTTP_CANCELLED,
    FDS_HTTP_OUT_OF_MEMORY
} FdsHttpResult;

/* Поверніть false з callback-функції, щоб скасувати завантаження. */
typedef bool (*FdsHttpProgressFn)(
    void *user,
    uint64_t downloaded,
    uint64_t total);

typedef struct {
    const char *user_agent;          /* NULL = значення за замовчуванням */
    uint32_t connect_timeout_ms;     /* 0 = за замовчуванням */
    uint32_t timeout_ms;
    bool follow_redirects;           /* Рекомендовано true */
    bool verify_tls;                 /* Перевірка сертифікатів (true) */
    uint64_t max_download_size;      /* 0 = без обмежень */
    FdsHttpProgressFn progress;
    void *progress_user;
} FdsHttpOptions;

typedef struct {
    FdsHttpResult result;
    long http_status;
    uint64_t bytes_written;
    int native_error_code;           /* CURLcode на Linux, GetLastError() на Windows */
    char error[256];
} FdsHttpResponse;

/* Опаковий контекст для збереження стану між запитами */
typedef struct FdsHttpContext FdsHttpContext;

/* ============================================================================
   Глобальна ініціалізація та Контекст
   ============================================================================ */
FDS_EXT_HTTP_DEF bool fds_http_init(void);
FDS_EXT_HTTP_DEF void fds_http_cleanup(void);
FDS_EXT_HTTP_DEF bool fds_http_is_initialized(void);

FDS_EXT_HTTP_DEF FdsHttpOptions fds_http_options_default(void);
FDS_EXT_HTTP_DEF const char *fds_http_result_string(FdsHttpResult result);

/* Створення та знищення контексту завантаження */
FDS_EXT_HTTP_DEF FdsHttpContext* fds_http_context_create(void);
FDS_EXT_HTTP_DEF void fds_http_context_destroy(FdsHttpContext *ctx);

/* ============================================================================
   Завантаження
   ============================================================================ */
/* Завантаження у файл */
FDS_EXT_HTTP_DEF FdsHttpResponse fds_http_download_file(
    FdsHttpContext *ctx,
    const char *url,
    const char *output_path,
    const FdsHttpOptions *options);

/* Завантаження в пам'ять (out_data потрібно звільнити через free()) */
FDS_EXT_HTTP_DEF FdsHttpResponse fds_http_download_mem(
    FdsHttpContext *ctx,
    const char *url,
    void **out_data,
    size_t *out_size,
    const FdsHttpOptions *options);

#ifdef __cplusplus
}
#endif

#endif /* FDS_EXT_HTTP_H */

/* ============================================================================
   РЕАЛІЗАЦІЯ
   ============================================================================ */
#ifdef FDS_EXT_HTTP_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <wininet.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "wininet.lib")
    #endif
#else
    #include <curl/curl.h>
#endif

static bool fds_http_initialized = false;

struct FdsHttpContext {
#ifdef _WIN32
    HINTERNET hInternet;
#else
    CURL *curl;
#endif
};

typedef struct {
    bool is_mem;
    FILE *file;
    uint8_t *buffer;
    size_t capacity;
    uint64_t bytes_written;
    uint64_t max_download_size;
    bool too_large;
    bool write_error;
    bool cancelled;
    bool oom_error;
    FdsHttpProgressFn progress;
    void *progress_user;
} FdsHttpWriteContext;

static void fds_http_clear_response(FdsHttpResponse *response) {
    memset(response, 0, sizeof(*response));
    response->result = FDS_HTTP_NETWORK_ERROR;
}

static bool fds_http_process_chunk(FdsHttpWriteContext *ctx, const void *data, size_t size) {
    if (size == 0) return true;

    if (ctx->max_download_size > 0 && ctx->bytes_written + size > ctx->max_download_size) {
        ctx->too_large = true;
        return false;
    }

    if (ctx->is_mem) {
        if (ctx->bytes_written + size > ctx->capacity) {
            size_t new_cap = ctx->capacity == 0 ? 8192 : ctx->capacity * 2;
            while (new_cap < ctx->bytes_written + size) new_cap *= 2;
            
            /* Виділяємо +1 байт для безпечного null-термінатора в кінці */
            void *new_buf = realloc(ctx->buffer, new_cap + 1);
            if (!new_buf) { 
                ctx->oom_error = true; 
                return false; 
            }
            ctx->buffer = (uint8_t*)new_buf;
            ctx->capacity = new_cap;
        }
        memcpy(ctx->buffer + ctx->bytes_written, data, size);
        ctx->bytes_written += size;
    } else {
        if (fwrite(data, 1, size, ctx->file) != size) {
            ctx->write_error = true;
            return false;
        }
        ctx->bytes_written += size;
    }
    return true;
}

FDS_EXT_HTTP_DEF bool fds_http_init(void) {
    if (fds_http_initialized) return true;
#ifndef _WIN32
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return false;
#endif
    fds_http_initialized = true;
    return true;
}

FDS_EXT_HTTP_DEF void fds_http_cleanup(void) {
    if (!fds_http_initialized) return;
#ifndef _WIN32
    curl_global_cleanup();
#endif
    fds_http_initialized = false;
}

FDS_EXT_HTTP_DEF bool fds_http_is_initialized(void) {
    return fds_http_initialized;
}

FDS_EXT_HTTP_DEF FdsHttpOptions fds_http_options_default(void) {
    FdsHttpOptions options;
    memset(&options, 0, sizeof(options));
    options.user_agent = "FDS-HTTP/1.0";
    options.follow_redirects = true;
    options.verify_tls = true;
    return options;
}

FDS_EXT_HTTP_DEF const char *fds_http_result_string(FdsHttpResult result) {
    switch (result) {
        case FDS_HTTP_OK:                return "OK";
        case FDS_HTTP_INVALID_ARGUMENT:  return "invalid argument";
        case FDS_HTTP_NOT_INITIALIZED:   return "HTTP is not initialized";
        case FDS_HTTP_INIT_FAILED:       return "HTTP context init failed";
        case FDS_HTTP_NETWORK_ERROR:     return "network error";
        case FDS_HTTP_FILE_ERROR:        return "file error";
        case FDS_HTTP_HTTP_ERROR:        return "HTTP status error";
        case FDS_HTTP_TOO_LARGE:         return "download is too large";
        case FDS_HTTP_CANCELLED:         return "transfer cancelled";
        case FDS_HTTP_OUT_OF_MEMORY:     return "out of memory";
        default:                         return "unknown error";
    }
}

FDS_EXT_HTTP_DEF FdsHttpContext* fds_http_context_create(void) {
    FdsHttpContext *ctx = (FdsHttpContext*)calloc(1, sizeof(FdsHttpContext));
    if (!ctx) return NULL;

#ifdef _WIN32
    ctx->hInternet = InternetOpenA("FDS-HTTP/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!ctx->hInternet) {
        free(ctx);
        return NULL;
    }
#else
    ctx->curl = curl_easy_init();
    if (!ctx->curl) {
        free(ctx);
        return NULL;
    }
#endif
    return ctx;
}

FDS_EXT_HTTP_DEF void fds_http_context_destroy(FdsHttpContext *ctx) {
    if (!ctx) return;
#ifdef _WIN32
    if (ctx->hInternet) InternetCloseHandle(ctx->hInternet);
#else
    if (ctx->curl) curl_easy_cleanup(ctx->curl);
#endif
    free(ctx);
}

#ifndef _WIN32
static size_t fds_curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    FdsHttpWriteContext *ctx = (FdsHttpWriteContext *)userdata;
    size_t bytes = size * nmemb;
    if (bytes == 0) return 0;
    
    if (!fds_http_process_chunk(ctx, ptr, bytes)) {
        return 0; /* Перериває завантаження curl */
    }
    return bytes;
}

static int fds_curl_progress_cb(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal; (void)ulnow;
    FdsHttpWriteContext *ctx = (FdsHttpWriteContext *)userdata;
    if (!ctx || !ctx->progress) return 0;

    if (!ctx->progress(ctx->progress_user, dlnow > 0 ? (uint64_t)dlnow : 0, dltotal > 0 ? (uint64_t)dltotal : 0)) {
        ctx->cancelled = true;
        return 1;
    }
    return 0;
}
#endif

static FdsHttpResponse fds_http_download_internal(
    FdsHttpContext *ctx,
    const char *url,
    const char *output_path,
    void **out_data,
    size_t *out_size,
    const FdsHttpOptions *options) 
{
    FdsHttpResponse response;
    fds_http_clear_response(&response);

    if (!url || url[0] == '\0' || !ctx) {
        response.result = FDS_HTTP_INVALID_ARGUMENT;
        snprintf(response.error, sizeof(response.error), "Invalid argument or null context");
        return response;
    }

    if (!fds_http_initialized) {
        response.result = FDS_HTTP_NOT_INITIALIZED;
        snprintf(response.error, sizeof(response.error), "call fds_http_init() first");
        return response;
    }

    FdsHttpOptions opt = options ? *options : fds_http_options_default();
    FdsHttpWriteContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.max_download_size = opt.max_download_size;
    wctx.progress = opt.progress;
    wctx.progress_user = opt.progress_user;

    if (output_path) {
        wctx.is_mem = false;
        wctx.file = fopen(output_path, "wb");
        if (!wctx.file) {
            response.result = FDS_HTTP_FILE_ERROR;
            snprintf(response.error, sizeof(response.error), "cannot open output file");
            return response;
        }
    } else {
        wctx.is_mem = true;
        if (out_data) *out_data = NULL;
        if (out_size) *out_size = 0;
    }

#ifdef _WIN32
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (!opt.verify_tls) {
        flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    if (opt.user_agent) {
        InternetSetOptionA(ctx->hInternet, INTERNET_OPTION_USER_AGENT, (void*)opt.user_agent, (DWORD)strlen(opt.user_agent));
    }
    if (opt.connect_timeout_ms > 0) {
        DWORD ct = opt.connect_timeout_ms;
        InternetSetOptionA(ctx->hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &ct, sizeof(ct));
    }
    if (opt.timeout_ms > 0) {
        DWORD rt = opt.timeout_ms;
        InternetSetOptionA(ctx->hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &rt, sizeof(rt));
    }

    HINTERNET hUrl = InternetOpenUrlA(ctx->hInternet, url, NULL, 0, flags, 0);
    if (!hUrl) {
        response.native_error_code = GetLastError();
        snprintf(response.error, sizeof(response.error), "InternetOpenUrlA failed: %d", response.native_error_code);
        goto done;
    }

    DWORD status = 0;
    DWORD status_len = sizeof(status);
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &status_len, NULL)) {
        response.http_status = status;
    }

    uint64_t total_size = 0;
    DWORD content_len = 0;
    DWORD clen_len = sizeof(content_len);
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &content_len, &clen_len, NULL)) {
        total_size = content_len;
    }

    char chunk[8192];
    DWORD bytes_read = 0;
    
    while (InternetReadFile(hUrl, chunk, sizeof(chunk), &bytes_read) && bytes_read > 0) {
        if (!fds_http_process_chunk(&wctx, chunk, bytes_read)) break;
        
        if (wctx.progress) {
            if (!wctx.progress(wctx.progress_user, wctx.bytes_written, total_size)) {
                wctx.cancelled = true;
                break;
            }
        }
    }
    InternetCloseHandle(hUrl);
    response.native_error_code = 0;

#else
    char curl_err[CURL_ERROR_SIZE] = {0};
    curl_easy_reset(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, fds_curl_write_cb);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &wctx);
    curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, curl_err);
    curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION, opt.follow_redirects ? 1L : 0L);
    curl_easy_setopt(ctx->curl, CURLOPT_FAILONERROR, 0L);
    curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYPEER, opt.verify_tls ? 1L : 0L);
    curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYHOST, opt.verify_tls ? 2L : 0L);
    curl_easy_setopt(ctx->curl, CURLOPT_NOSIGNAL, 1L);

    if (opt.user_agent) curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT, opt.user_agent);
    if (opt.connect_timeout_ms > 0) curl_easy_setopt(ctx->curl, CURLOPT_CONNECTTIMEOUT_MS, (long)opt.connect_timeout_ms);
    if (opt.timeout_ms > 0) curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT_MS, (long)opt.timeout_ms);

    if (opt.progress) {
        curl_easy_setopt(ctx->curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(ctx->curl, CURLOPT_XFERINFOFUNCTION, fds_curl_progress_cb);
        curl_easy_setopt(ctx->curl, CURLOPT_XFERINFODATA, &wctx);
    }

    CURLcode code = curl_easy_perform(ctx->curl);
    response.native_error_code = (int)code;
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &response.http_status);
    
    if (code != CURLE_OK && !wctx.cancelled && !wctx.too_large && !wctx.write_error && !wctx.oom_error) {
        snprintf(response.error, sizeof(response.error), "%s", curl_err[0] ? curl_err : curl_easy_strerror(code));
    }
#endif

done:
    if (wctx.file) fclose(wctx.file);

    response.bytes_written = wctx.bytes_written;

    if (wctx.cancelled) {
        response.result = FDS_HTTP_CANCELLED;
        snprintf(response.error, sizeof(response.error), "transfer cancelled by callback");
    } else if (wctx.oom_error) {
        response.result = FDS_HTTP_OUT_OF_MEMORY;
        snprintf(response.error, sizeof(response.error), "failed to allocate memory for download");
    } else if (wctx.too_large) {
        response.result = FDS_HTTP_TOO_LARGE;
        snprintf(response.error, sizeof(response.error), "download exceeded max_download_size");
    } else if (wctx.write_error) {
        response.result = FDS_HTTP_FILE_ERROR;
        snprintf(response.error, sizeof(response.error), "failed writing data");
    } else if (response.native_error_code != 0) {
        response.result = FDS_HTTP_NETWORK_ERROR;
        /* Текст помилки вже встановлено у блоках вище */
    } else if (response.http_status >= 400) {
        response.result = FDS_HTTP_HTTP_ERROR;
        snprintf(response.error, sizeof(response.error), "HTTP status %ld", response.http_status);
    } else {
        response.result = FDS_HTTP_OK;
        response.error[0] = '\0';
    }

    if (response.result != FDS_HTTP_OK) {
        if (!wctx.is_mem && output_path) remove(output_path);
        if (wctx.is_mem && wctx.buffer) {
            free(wctx.buffer);
            wctx.buffer = NULL;
            wctx.bytes_written = 0;
        }
    }

    if (wctx.is_mem && response.result == FDS_HTTP_OK) {
        if (wctx.buffer) {
            wctx.buffer[wctx.bytes_written] = '\0'; /* Безпечне завершення нулем */
        }
        if (out_data) *out_data = wctx.buffer;
        if (out_size) *out_size = (size_t)wctx.bytes_written;
    }

    return response;
}

FDS_EXT_HTTP_DEF FdsHttpResponse fds_http_download_file(
    FdsHttpContext *ctx,
    const char *url,
    const char *output_path,
    const FdsHttpOptions *options) 
{
    return fds_http_download_internal(ctx, url, output_path, NULL, NULL, options);
}

FDS_EXT_HTTP_DEF FdsHttpResponse fds_http_download_mem(
    FdsHttpContext *ctx,
    const char *url,
    void **out_data,
    size_t *out_size,
    const FdsHttpOptions *options) 
{
    return fds_http_download_internal(ctx, url, NULL, out_data, out_size, options);
}

#endif /* FDS_EXT_HTTP_IMPL */