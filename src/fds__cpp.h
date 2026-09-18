#ifndef FDS_H
#define FDS_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <stdalign.h>
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define KB ((size_t)1024)
#define MB (KB * 1024)
#define GB (MB * 1024)
#define TB (GB * 1024)

#ifndef TEMP_ARENA_SIZE
#define TEMP_ARENA_SIZE (8 * MB)
#endif

#ifndef SV_NPOS
#define SV_NPOS SIZE_MAX
#endif

#ifndef SB_INITIAL_CAPACITY
#define SB_INITIAL_CAPACITY 64
#endif


#ifndef ALIGNMENT
#define ALIGNMENT 16 /* must be a power of two */
#endif
#define ALIGN(s) (((size_t)(s) + (ALIGNMENT - 1)) & ~((size_t)(ALIGNMENT - 1)))

#define FOOTER_SIZE sizeof(size_t)
#define MIN_BLOCK_SIZE ALIGN(HEADER_SIZE + FOOTER_SIZE)

#define array_len(arr) (sizeof(arr) / sizeof((arr)[0]))

#define FDS_PANIC(...) fds_log(FFATAL, __VA_ARGS__);

#define FDS_LZ_MAGIC_0 0x46 // 'F'
#define FDS_LZ_MAGIC_1 0x43 // 'C'

#define FDS_LZ_MODE_RAW 0x00
#define FDS_LZ_MODE_LZSS 0x01

#define FDS_LZ_WINDOW_SIZE 4096
#define FDS_LZ_MIN_MATCH 3
#define FDS_LZ_MAX_MATCH 18
#define FDS_LZ_HASH_SIZE 4096

#define TODO(...)                                                             \
    do                                                                        \
    {                                                                         \
        fprintf(stderr, "[TODO] %s:%d (%s): ", __FILE__, __LINE__, __func__); \
        fprintf(stderr, __VA_ARGS__);                                         \
        fprintf(stderr, "\n");                                                \
    } while (0)

#define fds_unused (void)

#define fds_da_write(filepath, da) \
    fds_da_write_file((filepath), (da)->items, (da)->count, sizeof(*(da)->items))

#define fds_da_read(filepath, da) \
    fds_da_read_file((filepath), (void **)&((da)->items), &((da)->count), sizeof(*(da)->items))

// --- МАКРОСИ ДЛЯ ЗАПИСУ ---
// Записує значення змінної (передається сама змінна, макрос сам бере її адресу &)
#define fds_file_write_val(file, val) fds_file_write((file), &(val), sizeof(val))

// --- МАКРОСИ ДЛЯ ЧИТАННЯ ---
// Читає дані безпосередньо у змінну за її адресою
#define fds_file_read_val(file, val_ptr) fds_file_read((file), (val_ptr), sizeof(*(val_ptr)))

#define fds_file_skip_type(file, type) fds_file_skip((file), (int64_t)sizeof((type)))

// Читає дані та повертає їх як результат (зручно для присвоєння: x = fds_file_get(f, int))
#define fds_file_get(file, type) \
    ({ type _tmp; fds_file_read((file), &_tmp, sizeof(type)) == sizeof(type) ? _tmp : (type){0}; })

#define DA_FIELDS \
    size_t count; \
    size_t capacity

// #ifndef FDS_MALLOC
// #define FDS_CALLOC(a,p) calloc((a), (p));
// #define FDS_MALLOC(sz) malloc(sz)
// #define FDS_REALLOC(ptr, sz) realloc(ptr, sz)
// #define FDS_FREE(ptr) free((ptr))
// #endif




#ifdef __cplusplus
#define FDS_THREAD_LOCAL thread_local
#else
#define FDS_THREAD_LOCAL _Thread_local
#endif

#ifndef FDS_ASSERT
#define FDS_ASSERT(cond, msg) assert((cond) && (msg))
#endif



#define TEMP_THREAD_SCOPE()                                                                \
    __attribute__((cleanup(_temp_arena_thread_cleanup))) int _temp_arena_thread_dummy = 0; \
    /* Immediately initialize the arena (optional, but useful) */                          \
    (void)temp_arena_get()

#define TEMP_BUF(size) fixed_arena_alloc(temp_arena_get(), (size))

// Allocate an array of the given type
#define TEMP_ARRAY(Type, count) \
    ((Type *)fixed_arena_alloc_array(temp_arena_get(), (count), sizeof(Type)))

// Copy the line
#define TEMP_STRDUP(str) \
    fixed_arena_strdup(temp_arena_get(), (str))

// Generate a string via sprintf in a temporary buffer
// Returns a char* null-terminated string.
// Format: TEMP_SPRINTF("Hello %s", name)
#define TEMP_SPRINTF(fmt, ...) \
    temp_arena_sprintf(temp_arena_get(), (fmt), __VA_ARGS__)

#define TEMP_RESTORE(mark) fixed_arena_restore(temp_arena_get(), (mark))
#define TEMP_MARK() fixed_arena_mark(temp_arena_get())

#define TEMP_SCOPE() \
    __attribute__((cleanup(temp_arena_restore_mark))) FixedArenaMark temp_mark = fixed_arena_mark(temp_arena_get())



// Macros work with StringArray
// A macro for quick initialization + adding a few lines.
// Example: SA_INIT(&arr, "gcc", "-Wall", "-O2");
// The macro will insert a NULL sentinel as the last argument.
#define sa_init(sa, ...) sa_init_from_strings((sa), __VA_ARGS__, NULL)
#define sa_pushm(sa, ...) sa_push_many_impl((sa), __VA_ARGS__, NULL)

#define SA_FOREACH(sa, it) \
    for (char **it = (sa)->data; it != (sa)->data + (sa)->size; ++it)


//   Macro for loging
#define fds_log(level, ...) fds_log_impl(level, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef _WIN32
#define FDS_ISATTY _isatty
#define FDS_FILENO _fileno
#else
#include <unistd.h>
#define FDS_ISATTY isatty
#define FDS_FILENO fileno
#endif

//Allocator functions from reading file
typedef void *(*allocator)(size_t);


//=======================================================
//                    Allocator
//=======================================================
typedef struct fds_allocator fds_allocator;


typedef struct fds_allocator_stats {
    size_t alloc_count;
    size_t realloc_count;
    size_t free_count;
    size_t tmp_alloc_calls;
    size_t tmp_new_blocks;
    size_t permanent_count;
    size_t current_allocated;
    size_t peak_allocated;
    size_t total_allocated;
    size_t total_freed;
} fds_allocator_stats;

struct fds_allocator {
    void* (*alloc_fn)(fds_allocator *a, size_t size);
    void* (*calloc_fn)(fds_allocator *a, size_t num, size_t size);
    void* (*realloc_fn)(fds_allocator *a, void *ptr, size_t size);
    void  (*free_fn)(fds_allocator *a, void *ptr);
    void* (*alloc_tmp_fn)(fds_allocator *a, size_t size);
    void* (*alloc_permanent_fn)(fds_allocator *a, size_t size);

    void *all_blocks;
    void *tmp_active;
    void *tmp_free;

    size_t live_blocks_count;

#ifdef DEBUG_MEM
    size_t stats_alloc_count;
    size_t stats_realloc_count;
    size_t stats_free_count;
    size_t stats_tmp_alloc_calls;
    size_t stats_tmp_new_blocks;
    size_t stats_permanent_count;
    size_t stats_current_allocated;
    size_t stats_peak_allocated;
    size_t stats_total_allocated;
    size_t stats_total_freed;
#endif
};

typedef struct block_header {
    size_t size;
    struct block_header *next;
    struct block_header *prev;
    int is_tmp;
#ifdef DEBUG_MEM
    const char *file;
    int line;
    int is_permanent;
#endif
} block_header;


//=======================================================
//                    Log levels
//=======================================================

typedef enum
{
    FINFO = 0,
    FWARN,
    FERROR,
    FFATAL
} fds_log_level;

//=======================================================
//                    String Array
//=======================================================

typedef struct
{
    char **data;     // array of owned strings
    size_t size;     // number of strings currently stored
    size_t capacity; // allocated slots
} StringArray;

//=======================================================
//                    String builder
//=======================================================
typedef struct
{
    size_t count;
    size_t capacity;
    char *items;
} SB;

//=======================================================
//                     String view
//=======================================================
typedef struct
{
    size_t count;
    const char *data;
} SV;

#define SV_FMT "%.*s"
#define SV_ARGS(sv) (int)(sv).count, (sv).data /* int cast is required by printf's "%.*s" */

//=======================================================
//                        Fixed Arena
//=======================================================
typedef struct
{
    unsigned char *data;
    size_t offset;
    size_t capacity;
} FixedArena;

typedef size_t FixedArenaMark;

//=======================================================
//          INI parser structs
//=======================================================
// A structure for storing an INI key-value pair
typedef struct
{
    SV key;
    SV value;
} IniKV;

// A structure for a section (eg [Window]) of the INI
typedef struct
{
    SV name;
    IniKV *items; // Dynamic array (controlled by macros da_*)
    size_t count;
    size_t capacity;
} IniSection;

// The main structure of the INI config
typedef struct
{
    IniSection *items; // Dynamic array of sections
    size_t count;
    size_t capacity;
    FixedArena arena; // An arena for storing the entire contents of an INI file
} IniConfig;

//=======================================================
//              FDS folders manipulation struct
//=======================================================
typedef struct
{
    void *internal_handle;
    void *internal_find_data;
    int is_first;
} FdsDirIter;

//=======================================================
//                  Flag parser
//=======================================================
typedef enum
{
    FLAG_BOOL,
    FLAG_STRING,
    FLAG_INT,
    FLAG_FLOAT,
    FLAG_STRING_LIST,
    FLAG_INT_LIST,
    FLAG_FLOAT_LIST
} FlagType;

typedef struct
{
    char *name;
    FlagType type;
    void *ptr;
    char *defval;
    char *usage;
    bool set;
    bool required;
} Flag;

typedef struct
{
    Flag *items;
    size_t count;
    size_t capacity;

    SV *args;
    size_t args_count;
    size_t args_cap;

    char *name;
    void (*usage_func)(void);
} FlagSet;

typedef struct
{
    SV *items;
    size_t count;
    size_t capacity;
} da_SV;
typedef struct
{
    int *items;
    size_t count;
    size_t capacity;
} da_int;
typedef struct
{
    float *items;
    size_t count;
    size_t capacity;
} da_float;


// Procs struct
typedef struct
{
    bool success;      // Чи вдалося взагалі запустити процес (false, якщо файла не існує)
    int exit_code;     // Код завершення програми (0 = успіх)
    char *stdout_data; // Буфер стандартного виводу (завжди нуль-термінований)
    char *stderr_data; // Буфер виводу помилок (завжди нуль-термінований)
    size_t stdout_len; // Довжина виводу
    size_t stderr_len; // Довжина помилок
} fds_cmd_result;

/////////////////////////////////////////////
////               Event              ///////
/////////////////////////////////////////////
typedef uint32_t FdsEventType;

typedef struct
{
    FdsEventType type;
    uint64_t timestamp;
    union
    {
        int32_t i32[4];
        uint32_t u32[4];
        float f32[4];
        uint64_t u64[2];
        void *ptr;
    } as;
} FdsEvent;

typedef struct
{
    FdsEvent *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} FdsEventQueue;

/////////////////////////////////////////////
////               Compression        ///////
/////////////////////////////////////////////
typedef enum
{
    FDS_CMP_ERROR = -1,
    FDS_CMP_STORED = 0,
    FDS_CMP_COMPRESSED = 1
} FdsCompressStatus;

/////////////////////////////////////////////
////               Bytes              ///////
/////////////////////////////////////////////
// A dynamically growing buffer for writing binary data.
typedef struct
{
    uint8_t *data;   // Pointer to allocated memory
    size_t size;     // Current number of bytes written
    size_t capacity; // Total allocated capacity
} FdsBytesBuilder;

typedef struct
{
    const uint8_t *data;
    size_t size;
} FdsBytesView;
/////////////////////////////////////////////
////               File               ///////
/////////////////////////////////////////////

#define FDS_FILE_PTR(f) ((FILE *)(f).handle)

typedef enum
{
    FDS_FILE_READ = 1 << 0,
    FDS_FILE_WRITE = 1 << 1,
    FDS_FILE_CREATE = 1 << 2,
    FDS_FILE_APPEND = 1 << 3,
} FdsFileFlags;

#pragma pack(push, 1)
typedef struct
{
    char magic[4];    // 4-байтний ідентифікатор формату
    uint32_t version; // Версія структури даних
} FdsFileHeader;
#pragma pack(pop)

typedef enum
{
    FDS_SEEK_SET = 0,
    FDS_SEEK_CUR = 1,
    FDS_SEEK_END = 2,
} FdsSeekOrigin;

typedef struct
{
    uintptr_t handle;
    bool is_valid;
} FdsFile;

typedef enum
{
    FDS_MAP_READ = 1 << 0,
    FDS_MAP_READ_WRITE = 1 << 1,
} FdsMapFlags;

typedef struct
{
    void *data;               // Вказівник на початок проєкції в RAM
    size_t size;              // Точний розмір файлу в байтах
    uintptr_t file_handle;    // OS file handle (int fd або HANDLE)
    uintptr_t mapping_handle; // Потрібен тільки для Windows (HANDLE), на POSIX = 0
    bool is_valid;
} FdsMappedFile;
//=================================================================================================================================
//                                          Functions declaration
//=================================================================================================================================

// Bytes fuctions
//  Creates a new builder with an optional initial capacity (0 is fine).
//  e.g., FdsBytesBuilder bb = fds_bb_create(1024);
FdsBytesBuilder fds_bb_create(size_t initial_capacity);

// Frees the underlying memory of the builder.
// e.g., fds_bb_destroy(&bb);
void fds_bb_destroy(FdsBytesBuilder *bb);

// Ensures the builder has enough capacity to add 'additional_size' bytes.
// Automatically called by append functions, but useful for pre-allocating.
void fds_bb_reserve(FdsBytesBuilder *bb, size_t additional_size);

// Resets the size to 0 but keeps the allocated memory (capacity remains).
void fds_bb_clear(FdsBytesBuilder *bb);

// ============================================================================
// BYTESVIEW INTEROP (Conversion)
// ============================================================================

// Converts a Builder into a non-owning View.
// Note: The returned View becomes invalid if the Builder is destroyed or grows.
// e.g., FdsBytesView final_data = fds_bb_to_view(&bb);
FdsBytesView fds_bb_to_view(const FdsBytesBuilder *bb);

// Creates a new Builder by copying all data from an existing View.
// e.g., FdsBytesBuilder bb = fds_bb_from_view(my_view);
FdsBytesBuilder fds_bb_from_view(FdsBytesView view);

// Appends all data from a View into the Builder.
// e.g., fds_bb_append_view(&bb, file_header_view);
void fds_bb_append_view(FdsBytesBuilder *bb, FdsBytesView view);

// ============================================================================
// WRITING / APPENDING
// ============================================================================

// Appends a raw memory block to the builder.
// e.g., fds_bb_append(&bb, &my_struct, sizeof(my_struct));
void fds_bb_append(FdsBytesBuilder *bb, const void *data, size_t size);

// Appends a single byte.
// e.g., fds_bb_append_byte(&bb, 0xFF);
void fds_bb_append_byte(FdsBytesBuilder *bb, uint8_t byte);

// Appends a 16-bit unsigned integer (Little-Endian).
void fds_bb_append_u16_le(FdsBytesBuilder *bb, uint16_t val);

// Appends a 16-bit unsigned integer (Big-Endian / Network Order).
void fds_bb_append_u16_be(FdsBytesBuilder *bb, uint16_t val);

// Appends a 32-bit unsigned integer (Little-Endian).
void fds_bb_append_u32_le(FdsBytesBuilder *bb, uint32_t val);

// Appends a 32-bit unsigned integer (Big-Endian / Network Order).
void fds_bb_append_u32_be(FdsBytesBuilder *bb, uint32_t val);

// Appends a 64-bit unsigned integer
void fds_bb_append_u64_le(FdsBytesBuilder *bb, uint64_t val);
void fds_bb_append_u64_be(FdsBytesBuilder *bb, uint64_t val);

// Appends a 32-bit float (IEEE 754) safely
void fds_bb_append_f32_le(FdsBytesBuilder *bb, float val);
void fds_bb_append_f32_be(FdsBytesBuilder *bb, float val);

// Appends a 64-bit double (IEEE 754) safely
void fds_bb_append_f64_le(FdsBytesBuilder *bb, double val);
void fds_bb_append_f64_be(FdsBytesBuilder *bb, double val);

// Повертає поточну позицію (offset) для майбутнього патчінгу
size_t fds_bb_get_pos(const FdsBytesBuilder *bb);

// Перезаписує 32-бітне число за вказаним зміщенням (без зміни розміру буфера)
void fds_bb_patch_u32_le(FdsBytesBuilder *bb, size_t offset, uint32_t val);

// Додає класичний C-рядок (з нуль-термінатором на кінці)
void fds_bb_append_cstr(FdsBytesBuilder *bb, const char *str);

// Додає рядок з префіксом довжини (u16)
void fds_bb_append_string_u16(FdsBytesBuilder *bb, const char *str);

bool fds_bb_save_to_file(const FdsBytesBuilder *bb, const char *filepath);

// ============================================================================
// CONSTRUCTORS
// ============================================================================

// Creates a BytesView from a raw data pointer and size.
// e.g., fds_bv(buffer, 1024);
FdsBytesView fds_bv(const void *data, size_t size);

// Returns an empty BytesView (data = NULL, size = 0).
FdsBytesView fds_bv_empty(void);

// Creates a BytesView from a null-terminated C string.
// Note: It calculates length up to, but not including, the '\0'.
// e.g., fds_bv_from_cstr("PNG_MAGIC");
FdsBytesView fds_bv_from_cstr(const char *str);

// ============================================================================
// SLICING & REGIONS (Returns a new view, does not mutate the original)
// ============================================================================

// Safely extracts a sub-region from the view. Clamps length to available size.
// e.g., FdsBytesView payload = fds_bv_subview(packet, 12, 100); // skip 12 byte header
FdsBytesView fds_bv_subview(FdsBytesView view, size_t offset, size_t length);

// Returns a view containing the first 'count' bytes. Clamps to view.size.
FdsBytesView fds_bv_take(FdsBytesView view, size_t count);

// Returns a view skipping the first 'count' bytes. Clamps to view.size.
// e.g., view = fds_bv_skip(view, 4); // skip 4 bytes of magic number
FdsBytesView fds_bv_skip(FdsBytesView view, size_t count);

// ============================================================================
// Allocator functions
// ============================================================================

// Створення та знищення алокатора
fds_allocator* fds_allocator_create(void);
void fds_allocator_destroy(fds_allocator *a);

// Керування контекстом (TLS)
void fds_allocator_push(fds_allocator *a);
void fds_allocator_pop(void);
fds_allocator* fds_allocator_current(void);

// Явне очищення тимчасової пам'яті
void fds_allocator_clear_tmp(fds_allocator *a);

// Статистика та логування
size_t fds_allocator_live_blocks_count(fds_allocator *a);
#ifdef DEBUG_MEM
void fds_allocator_get_stats(fds_allocator *a, fds_allocator_stats *out_stats);
void fds_allocator_print_stats(fds_allocator *a);
#endif

// Внутрішні реалізації алокації
void* fds_alloc_impl(fds_allocator *a, size_t size);
void* fds_calloc_impl(fds_allocator *a, size_t num, size_t size);
void* fds_realloc_impl(fds_allocator *a, void *ptr, size_t size);
void  fds_free_impl(fds_allocator *a, void *ptr);
void* fds_alloc_tmp_impl(fds_allocator *a, size_t size);
void* fds_alloc_permanent_impl(fds_allocator *a, size_t size);

#ifdef DEBUG_MEM
void* fds_alloc_impl_tracked(fds_allocator *a, size_t size, const char *file, int line);
void* fds_calloc_impl_tracked(fds_allocator *a, size_t num, size_t size, const char *file, int line);
void* fds_realloc_impl_tracked(fds_allocator *a, void *ptr, size_t size, const char *file, int line);
void* fds_alloc_tmp_impl_tracked(fds_allocator *a, size_t size, const char *file, int line);
void* fds_alloc_permanent_impl_tracked(fds_allocator *a, size_t size, const char *file, int line);
#endif





// Макроси-обгортки
#ifdef DEBUG_MEM
    #define fds_alloc(size)                fds_alloc_impl_tracked(NULL, size, __FILE__, __LINE__)
    #define fds_calloc(num, size)          fds_calloc_impl_tracked(NULL, num, size, __FILE__, __LINE__)
    #define fds_free(ptr)                  fds_free_impl(NULL, ptr)
    #define fds_realloc(ptr, size)         fds_realloc_impl_tracked(NULL, ptr, size, __FILE__, __LINE__)
    #define fds_alloc_tmp(size)            fds_alloc_tmp_impl_tracked(NULL, size, __FILE__, __LINE__)
    #define fds_alloc_permanent(size)      fds_alloc_permanent_impl_tracked(NULL, size, __FILE__, __LINE__)
    
    #define fds_alloc_a(a, size)           fds_alloc_impl_tracked(a, size, __FILE__, __LINE__)
    #define fds_calloc_a(a, num, size)     fds_calloc_impl_tracked(a, num, size, __FILE__, __LINE__)
    #define fds_free_a(a, ptr)             fds_free_impl(a, ptr)
    #define fds_realloc_a(a, ptr, size)    fds_realloc_impl_tracked(a, ptr, size, __FILE__, __LINE__)
    #define fds_alloc_tmp_a(a, size)       fds_alloc_tmp_impl_tracked(a, size, __FILE__, __LINE__)
    #define fds_alloc_permanent_a(a, size) fds_alloc_permanent_impl_tracked(a, size, __FILE__, __LINE__)
#else
    #define fds_alloc(size)                fds_alloc_impl(NULL, size)
    #define fds_calloc(num, size)          fds_calloc_impl(NULL, num, size)
    #define fds_free(ptr)                  fds_free_impl(NULL, ptr)
    #define fds_realloc(ptr, size)         fds_realloc_impl(NULL, ptr, size)
    #define fds_alloc_tmp(size)            fds_alloc_tmp_impl(NULL, size)
    #define fds_alloc_permanent(size)      fds_alloc_permanent_impl(NULL, size)
    
    #define fds_alloc_a(a, size)           fds_alloc_impl(a, size)
    #define fds_calloc_a(a, num, size)     fds_calloc_impl(a, num, size)
    #define fds_free_a(a, ptr)             fds_free_impl(a, ptr)
    #define fds_realloc_a(a, ptr, size)    fds_realloc_impl(a, ptr, size)
    #define fds_alloc_tmp_a(a, size)       fds_alloc_tmp_impl(a, size)
    #define fds_alloc_permanent_a(a, size) fds_alloc_permanent_impl(a, size)
#endif


#define FDS_ALIGNMENT 16
#define FDS_ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define HEADER_SIZE FDS_ALIGN_UP(sizeof(block_header), FDS_ALIGNMENT)


// ============================================================================
// COMPARISON & SEARCH
// ============================================================================

// Deep comparison of two byte views. Returns true if sizes and contents match exactly.
bool fds_bv_equals(FdsBytesView a, FdsBytesView b);

// Checks if the view starts with the given prefix.
// e.g., fds_bv_has_prefix(file_data, fds_bv_from_cstr("PK\x03\x04")); // check ZIP magic
bool fds_bv_has_prefix(FdsBytesView view, FdsBytesView prefix);

// Checks if the view ends with the given suffix.
bool fds_bv_has_suffix(FdsBytesView view, FdsBytesView suffix);

// Finds the first occurrence of a specific byte. Returns index, or -1 if not found.
// e.g., intptr_t null_pos = fds_bv_find_byte(view, 0x00);
intptr_t fds_bv_find_byte(FdsBytesView view, uint8_t byte);

// Finds the first occurrence of a byte pattern. Returns index, or -1 if not found.
// e.g., intptr_t sig_pos = fds_bv_find_subview(view, fds_bv(signature, 4));
intptr_t fds_bv_find_subview(FdsBytesView view, FdsBytesView pattern);

// ============================================================================
// STREAM PARSING (Mutates the view pointer/size directly to advance through data)
// ============================================================================

// Pops 1 byte from the front of the view and advances it. Returns false if empty.
// e.g., uint8_t type; if (fds_bv_pop_byte(&stream, &type)) { ... }
bool fds_bv_pop_byte(FdsBytesView *view, uint8_t *out_byte);

// Pops 'count' bytes from the front, returning them as a new view, and advances the original view.
// e.g., FdsBytesView header = fds_bv_pop_bytes(&stream, 16);
FdsBytesView fds_bv_pop_bytes(FdsBytesView *view, size_t count);

// Reads a 16-bit unsigned integer (Little-Endian) and advances the view by 2 bytes.
bool fds_bv_read_u16_le(FdsBytesView *view, uint16_t *out_val);

// Reads a 16-bit unsigned integer (Big-Endian / Network Order) and advances the view by 2 bytes.
// e.g., uint16_t port; fds_bv_read_u16_be(&tcp_packet, &port);
bool fds_bv_read_u16_be(FdsBytesView *view, uint16_t *out_val);

// Reads a 32-bit unsigned integer (Little-Endian) and advances the view by 4 bytes.
// e.g., uint32_t chunk_size; fds_bv_read_u32_le(&png_stream, &chunk_size);
bool fds_bv_read_u32_le(FdsBytesView *view, uint32_t *out_val);

// Reads a 32-bit unsigned integer (Big-Endian / Network Order) and advances the view by 4 bytes.
bool fds_bv_read_u32_be(FdsBytesView *view, uint32_t *out_val);

// Reads a 64-bit unsigned integer (Little-Endian / Big-Endian)
bool fds_bv_read_u64_le(FdsBytesView *view, uint64_t *out_val);
bool fds_bv_read_u64_be(FdsBytesView *view, uint64_t *out_val);

// Reads a 32-bit float (IEEE 754) safely avoiding strict-aliasing UB
bool fds_bv_read_f32_le(FdsBytesView *view, float *out_val);
bool fds_bv_read_f32_be(FdsBytesView *view, float *out_val);

// Reads a 64-bit double (IEEE 754)
bool fds_bv_read_f64_le(FdsBytesView *view, double *out_val);
bool fds_bv_read_f64_be(FdsBytesView *view, double *out_val);

// Читає рядок, очікуючи u16 префікс довжини.
// Повертає View, що вказує лише на текст (без копіювання!).
bool fds_bv_read_string_u16(FdsBytesView *view, FdsBytesView *out_str_view);

// Compression
FdsCompressStatus fds_ext_compress_lz(FdsBytesView input, FdsBytesBuilder *out_builder);
bool fds_ext_decompress_lz(FdsBytesView input, FdsBytesBuilder *out_builder);

// honestly, I don't remember why I wrote it, but for something important, so it should be left
static inline int safe_add(size_t a, size_t b, size_t *res);

// Logging fuctions Start ================================================================================================================
static inline bool fds_should_use_color(FILE *stream);
void fds_log_impl(fds_log_level level, const char *file, int line, const char *func, const char *fmt, ...);
// Logging fuctions End ================================================================================================================

// Console utils fuctions Start ================================================================================================================

void fds_cli_init(int *argc, char ***argv);
char *fds_internal_utf16_to_utf8(const wchar_t *utf16_str);
static wchar_t *fds_internal_utf8_to_utf16(const char *utf8_str);
size_t utf8_strlen(const char *s);

// Console utils fuctions End ================================================================================================================

// Flag parser fuctions Start ================================================================================================================

FlagSet *flagset_new(void);
void flagset_free(FlagSet *fs);
void flagset_var(FlagSet *fs, FlagType type, void *ptr, const char *name, const char *defval, const char *usage);
void flagset_required(FlagSet *fs); // marks the last added checkbox as required

void flagset_parse(FlagSet *fs, int argc, char **argv);
size_t flagset_narg(FlagSet *fs);
SV flagset_arg(FlagSet *fs, size_t i);
void flagset_usage(FlagSet *fs);

static inline void flagset_bool(FlagSet *fs, bool *ptr, const char *name, bool defval, const char *usage);
static inline void flagset_string(FlagSet *fs, char **ptr, const char *name, const char *defval, const char *usage);
static inline void flagset_int(FlagSet *fs, int *ptr, const char *name, int defval, const char *usage);
static inline void flagset_float(FlagSet *fs, float *ptr, const char *name, float defval, const char *usage);
static inline void flagset_string_list(FlagSet *fs, void *list, const char *name, const char *usage);
static inline void flagset_int_list(FlagSet *fs, void *list, const char *name, const char *usage);
static inline void flagset_float_list(FlagSet *fs, void *list, const char *name, const char *usage);

static Flag *find_flag(FlagSet *fs, SV name);
static void set_flag_value(Flag *f, SV val);

// Flag parser fuctions End ================================================================================================================


// String Array fuctions Start================================================================================================================

// A function that accepts strings via a variable list,
// NULL terminated.
bool sa_init_from_strings(StringArray *sa, const char *first, ...);

static char *str_dup(const char *s);

// Work with many arrays
bool sa_copy(StringArray *dst, const StringArray *src);         // deep copy
bool sa_append_array(StringArray *dst, const StringArray *src); // add all rows from another array

void sa_trim(StringArray *sa);                // trim spaces on both sides of each line
bool sa_trim_at(StringArray *sa, size_t idx); // trim a specific line

// Find the index of the first row that exactly matches (case sensitive).
// Returns -1 (or SIZE_MAX) if not found.
size_t sa_find(const StringArray *sa, const char *str);
bool sa_contains(const StringArray *sa, const char *str); // is there such a line?

// Combine all lines into one, inserting a delimiter between them.
// Returns a new string (release via free()).
char *sa_join(const StringArray *sa, const char *delimiter);

// Split the str string by the delimiter separator and add all parts to the array.
// If skip_empty == true, empty fragments are not added.
bool sa_split(StringArray *sa, const char *str, const char *delimiter, bool skip_empty);

size_t sa_find_custom(const StringArray *sa, const char *needle,
                      int (*cmp)(const char *, const char *));
void sa_to_lower(StringArray *sa);
void sa_to_upper(StringArray *sa);
void sa_reverse(StringArray *sa);
// Sort the array. If cmp == NULL, strcmp() is used.
void sa_sort(StringArray *sa, int (*cmp)(const void *, const void *));
//============================================================
// Conclusion
// ============================================================
bool sa_pushf(StringArray *sa, const char *fmt, ...);
// Just print the array, each line on a new line (or with a prefix).

void sa_print(const StringArray *sa); // uses printf for each line
void sa_fprint(FILE *stream, const StringArray *sa);

// --- Lifecycle ---
bool sa_new(StringArray *sa, size_t initial_cap);
void sa_free(StringArray *sa);

// --- Adding / removing ---
bool sa_push(StringArray *sa, const char *str); // copies the string
bool sa_push_many_impl(StringArray *sa, const char *first, ...);
char *sa_pop(StringArray *sa); // caller must free() the returned string
bool sa_insert(StringArray *sa, size_t idx, const char *str);
bool sa_remove(StringArray *sa, size_t idx);

// --- Access ---
char *sa_get(StringArray *sa, size_t idx);                 // NULL if out of bounds
bool sa_set(StringArray *sa, size_t idx, const char *str); // replaces, frees old

// --- Utility ---
size_t sa_len(StringArray *sa);
void sa_clear(StringArray *sa); // empties but keeps capacity

static bool sa_grow(StringArray *sa);

// String Array fuctions End================================================================================================================

// Event fuctions Start================================================================================================================
// Ініціалізація та очищення
FdsEventQueue fds_event_queue_init(FdsEvent *buffer, size_t capacity);
void fds_event_clear(FdsEventQueue *q);

// Операції запису та читання
bool fds_event_push(FdsEventQueue *q, FdsEvent event);
bool fds_event_poll(FdsEventQueue *q, FdsEvent *out_event);
size_t fds_event_poll_many(FdsEventQueue *q, FdsEvent *events, size_t capacity);
bool fds_event_peek(const FdsEventQueue *q, FdsEvent *out_event);

// Пропуск / видалення подій
bool fds_event_discard(FdsEventQueue *q);
size_t fds_event_discard_many(FdsEventQueue *q, size_t count);

// Інспекція стану
bool fds_event_is_empty(const FdsEventQueue *q);
bool fds_event_is_full(const FdsEventQueue *q);
size_t fds_event_count(const FdsEventQueue *q);
size_t fds_event_capacity(const FdsEventQueue *q);
size_t fds_event_remaining(const FdsEventQueue *q);

// Фабричні функції (створення з обнуленням union)
FdsEvent fds_event_make(FdsEventType type, uint64_t timestamp);
FdsEvent fds_event_make_i32(FdsEventType type, uint64_t timestamp, int32_t value);
FdsEvent fds_event_make_u32(FdsEventType type, uint64_t timestamp, uint32_t value);
FdsEvent fds_event_make_f32(FdsEventType type, uint64_t timestamp, float value);
FdsEvent fds_event_make_ptr(FdsEventType type, uint64_t timestamp, void *ptr);
// Event fuctions End================================================================================================================

// File fuctions Start================================================================================================================
bool fds_file_read_bytes(const char *filepath, void **out_buf, size_t *out_size);

// Створює новий файл (або перезаписує існуючий) і записує туди size байт з buf.
bool fds_file_write_bytes(const char *filepath, const void *buf, size_t size);

// Додає size байт з buf у кінець файлу.
bool fds_file_append_bytes(const char *filepath, const void *buf, size_t size);

bool fds_da_write_file(const char *filepath, const void *items, size_t count, size_t item_size);

bool fds_da_read_file(const char *filepath, void **out_items, size_t *out_count, size_t item_size);

SV sv_chop_by_delim(SV *sv, char delim);
SV sv_trim_ext(SV sv);
SV sv_trim_right_ext(SV sv);
SV sv_trim_left_ext(SV sv);

SV sv_chop_right(SV *sv, size_t n);
SV sv_chop_left(SV *sv, size_t n);




bool fds_read_entire_file(const char *filepath, char **out_data, size_t *out_size);


FdsFile fds_file_open(const char *path, uint32_t flags);
void fds_file_close(FdsFile *file);

size_t fds_file_read(FdsFile file, void *dst, size_t size);
size_t fds_file_write(FdsFile file, const void *src, size_t size);

bool fds_file_seek(FdsFile file, int64_t offset, FdsSeekOrigin origin);
int64_t fds_file_tell(FdsFile file);
int64_t fds_file_size(FdsFile file);
void fds_file_flush(FdsFile file);

char *fds_file_read_str(FdsFile file, void *(*allocator)(size_t));
bool fds_file_write_str(FdsFile file, const char *str);

bool fds_file_skip(FdsFile file, int64_t bytes_to_skip);

bool fds_file_check_magic(FdsFile *file, const char expected_magic[4], uint32_t min_version);
bool fds_file_write_magic(FdsFile *file, const char magic[4], uint32_t version);

FdsMappedFile fds_file_map(const char *path, uint32_t flags);
void fds_file_unmap(FdsMappedFile *mapped);
void fds_file_flush_mapped(FdsMappedFile *mapped);
FILE *fds_fmemopen_win32(void *buf, size_t size, const char *mode);
FdsFile fds_file_from_mapped(FdsMappedFile *mapped);
SV fds_file_mapped_as_sv(FdsMappedFile *mapped);
// File fuctions End================================================================================================================

// String builder fuctions Start================================================================================================================

// Create SB from a regular string
SB sb_from_cstr(const char *str);

// Create an empty SB
SB sb_new(void);

// Attach sv to sb
void sb_append_sv(SB *sb, SV sv);

// Release SB
void sb_free(SB *sb);

// Append a normal line
void sb_append(SB *sb, const char *str);

// Append a string of a specified length
void sb_append_n(SB *sb, const char *str, size_t len);

// Allocate the specified amount of memory for sb if less than already allocated will cause an error
void sb_reserve(SB *sb, size_t capacity);

// Allocate additional memory
void sb_reserve_extra(SB *sb, size_t extra);

// Returns a pointer to an ordinary string with SB
char *sb_to_cstr(SB *sb);

// Add '\0' at the end
void sb_append_null(SB *sb);

// Appends one character
void sb_append_char(SB *sb, char c);
// Creates a full copy of SB
SB sb_clone(const SB *sb);

// Appends a string with formatting
void sb_appendf(SB *sb, const char *fmt, ...);
// String builder fuctions End ================================================================================================================

// Time utils fuctions Start ================================================================================================================

double fds_time_now(void);
void fds_sleep_ms(int milliseconds);

// Time utils fuctions End ================================================================================================================

// String view fuctions Start ================================================================================================================

void sv_remove_prefix(SV *sv, size_t count);
char *sv_to_cstr(SV sv);
char sv_at(SV sv, size_t index);
SV sv_new(void);
SV sv_from_cstr(const char *str);
SV sv_from_sb(const SB *sb);
SV sv_from_parts(const char *str, size_t len);
int sv_eq(SV sv1, SV sv2);
int sv_eq_cstr(SV sv1, const char *str);
void sv_trim_left(SV *sv);
void sv_trim_right(SV *sv);
void sv_trim(SV *sv);
void sv_slice(SV *sv, size_t begin, size_t end);
void sv_remove_suffix(SV *sv, size_t count);
int sv_ends_with(SV sv, SV suffix);
int sv_starts_with(SV sv, SV prefix);
int sv_starts_with_char(SV sv, char c);
int sv_ends_with_char(SV sv, char c);
SV sv_split_left(SV *sv, char c);
SV sv_split_right(SV *sv, char c);
size_t sv_find_char(SV sv, char c);
size_t sv_rfind_char(SV sv, char c);
int sv_consume_char(SV *sv, char c);
int sv_consume(SV *sv, SV prefix);
int sv_next_line(SV *text, SV *out_line);

static bool is_integer(SV sv);
static int sv_to_int(SV sv);
static float sv_to_float(SV sv);

// String view fuctions End ================================================================================================================

// String conversation fuctions Start ================================================================================================================

SV sb_to_sv(const SB *sb);

SB sv_to_sb(SV sv);

// String conversation fuctions End ================================================================================================================

// Fixed arena fuctions Start ================================================================================================================

char *fixed_arena_strdup(FixedArena *arena, const char *str);
char *fixed_arena_strndup(FixedArena *arena, const char *str, size_t len);
void *fixed_arena_memdup(FixedArena *arena, const void *src, size_t size);
void *fixed_arena_alloc_array(FixedArena *arena, size_t count, size_t element_size);
void *fixed_arena_alloc_zero(FixedArena *arena, size_t size);
void *fixed_arena_alloc(FixedArena *arena, size_t size);
void *fixed_arena_alloc_align(FixedArena *arena, size_t size, size_t alignment);
void fixed_arena_restore(FixedArena *arena, FixedArenaMark mark);
int fixed_arena_contains(const FixedArena *arena, const void *ptr);
int fixed_arena_is_empty(const FixedArena *arena);
FixedArenaMark fixed_arena_mark(const FixedArena *arena);
size_t fixed_arena_available(const FixedArena *arena);
size_t fixed_arena_used(const FixedArena *arena);
void fixed_arena_reset(FixedArena *arena);
FixedArena fixed_arena_create(size_t capacity);
void fixed_arena_free(FixedArena *arena);

// Fixed arena fuctions End ================================================================================================================

// Files and folders fuctions Start ================================================================================================================
size_t fds_get_file_size(const char *filepath);

int fds_file_read_to_arena(const char *filepath, FixedArena *arena, SV *out_sv);
int fds_file_read_to_sb(const char *filepath, SB *out_sb);
int fds_file_write_sv(const char *filepath, SV content);
int fds_file_append_sv(const char *filepath, SV content);
int fds_file_append_sb(const char *filepath, const SB *sb);
int fds_file_write_sb(const char *filepath, const SB *sb);
int fds_path_extension(SV filepath, SV *out_ext);
int fds_rename(const char *oldpath, const char *newpath);

time_t get_file_mtime(const char *path);

int fds_dir_create(const char *dirpath);

int fds_file_exists(const char *filepath);
int fds_dir_exists(const char *dirpath);

int fds_dir_iter_open(const char *dirpath, FdsDirIter *iter);

int fds_dir_iter_next(FdsDirIter *iter, SV *out_name, int *out_is_dir);

void fds_dir_iter_close(FdsDirIter *iter);

// Files and folders fuctions End ================================================================================================================

// INI fuctions Start ================================================================================================================

char *ini_get_temp_cstr(const IniConfig *config, const char *section, const char *key, const char *default_val);
int ini_get_bool(const IniConfig *config, const char *section, const char *key, int default_val);
float ini_get_float(const IniConfig *config, const char *section, const char *key, float default_val);
int ini_get_int(const IniConfig *config, const char *section, const char *key, int default_val);
SV ini_get_sv(const IniConfig *config, const char *section, const char *key, const char *default_val);
SV ini_get(const IniConfig *config, const char *section, const char *key);

void ini_free(IniConfig *config);

void ini_print(const IniConfig *config);

IniConfig ini_parse(const char *filepath);
IniConfig ini_parse_sv(SV content);
IniConfig ini_parse_sb(SB *content);

// INI fuctions End ================================================================================================================

// Procs fuctions Start ================================================================================================================

int fds_cmd_run_Simp(const char *cmd_utf8, char **out_output);
fds_cmd_result fds_cmd_run_ext(const char *cmd_utf8);
void fds_cmd_result_free(fds_cmd_result *res);
static void fds_append_pipe_data(char **buffer, size_t *len, size_t *cap, const char *chunk, size_t chunk_size);

// Procs fuctions End ================================================================================================================



#ifndef FDS_MALLOC
#define FDS_CALLOC(a,p) fds_calloc((a), (p));
#define FDS_MALLOC(sz) fds_alloc(sz)
#define FDS_REALLOC(ptr, sz) fds_realloc(ptr, sz)
#define FDS_FREE(ptr) fds_free((ptr))
#endif





void temp_arena_destroy(void);
static inline void temp_arena_restore_mark(FixedArenaMark *mark);
char *temp_arena_sprintf(FixedArena *arena, const char *fmt, ...);
void temp_arena_reset(void);
FixedArena *temp_arena_get(void);
static inline void _temp_arena_thread_cleanup(int *dummy);



// Start of implementation!!!
#ifdef FDS_IMPL



#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>

#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <time.h>
#endif

#ifdef _WIN32
static double fds_g_timer_frequency = 0.0;
#endif


static const char EMPTY_STR[] = "";

FDS_THREAD_LOCAL FixedArena temp_arena_instance = {0};
FDS_THREAD_LOCAL int temp_arena_initialized = 0;

static inline int safe_add(size_t a, size_t b, size_t *res)
{
    if (a > SIZE_MAX - b)
        return 0;
    *res = a + b;
    return 1;
}


// Loging functions Start ================================================================================================================

static inline bool fds_should_use_color(FILE *stream)
{
    return FDS_ISATTY(FDS_FILENO(stream)) != 0;
}

void fds_log_impl(fds_log_level level, const char *file, int line, const char *func, const char *fmt, ...)
{
    const char *prefix = "";
    const char *color = "";
    const char *reset = "";
    const char *meta_color = "";
    FILE *stream = stdout;

    // Встановлюємо рівні та потоки
    switch (level)
    {
    case FINFO:
        prefix = "[INFO] ";
        color = "\x1b[32m";
        break;
    case FWARN:
        prefix = "[WARN] ";
        color = "\x1b[33m";
        break;
    case FERROR:
        prefix = "[ERROR]";
        color = "\x1b[31m";
        stream = stderr;
        break;
    case FFATAL:
        prefix = "[FATAL]";
        color = "\x1b[35m";
        stream = stderr;
        break;
    }

    // Якщо вивід йде у файл, зануляємо всі кольори
    if (fds_should_use_color(stream))
    {
        reset = "\x1b[0m";
        meta_color = "\x1b[90m"; // Сірий для метаданих
    }
    else
    {
        color = "";
        reset = "";
        meta_color = "";
    }

    // Виводимо префікс і метадані (з кольором або без)
    fprintf(stream, "%s%s%s %s%s:%d:%s:%s ", color, prefix, reset, meta_color, file, line, func, reset);

    // Виводимо повідомлення
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    fprintf(stream, "\n");
    fflush(stream);

    if (level == FFATAL)
    {
        exit(69);
    }
}

// Loging functions End ================================================================================================================

// Console utils functions Start ================================================================================================================

static wchar_t *fds_internal_utf8_to_utf16(const char *utf8_str)
{
    if (!utf8_str)
        return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (len <= 0)
        return NULL;

    // Використовуємо TempArena, щоб не очищати пам'ять руками
    wchar_t *wstr = (wchar_t *)FDS_MALLOC(len * sizeof(wchar_t));
    if (!wstr)
        return NULL;

    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, len);
    return wstr;
}

// Конвертація UTF-16 -> UTF-8 (потрібна при читанні назв файлів із папки)
char *fds_internal_utf16_to_utf8(const wchar_t *utf16_str)
{
    if (!utf16_str)
        return NULL;

    int len = WideCharToMultiByte(CP_UTF8, 0, utf16_str, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
        return NULL;

    char *str = (char *)FDS_MALLOC(len);
    if (!str)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, utf16_str, -1, str, len, NULL, NULL);
    return str;
}

#pragma GCC diagnostic pop
// Ініціалізація консолі та нормалізація argv до UTF-8
void fds_cli_init(int *argc, char ***argv)
{
#ifdef _WIN32
    // 1. Примусово перемикаємо консоль Windows на UTF-8 (Code Page 65001)
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    // 2. Отримуємо точний юнікодний командний рядок Windows (UTF-16)
    int wargc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv)
        return;

    // 3. Перетворюємо wchar_t** (UTF-16) у char** (UTF-8)
    // Використовуємо кастомну арену або malloc для виділення масиву
    char **utf8_argv = (char **)FDS_MALLOC(sizeof(char *) * (wargc + 1));

    for (int i = 0; i < wargc; i++)
    {
        // Розраховуємо необхідний розмір буфера UTF-8
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);

        char *utf8_str = (char *)FDS_MALLOC(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, utf8_str, size_needed, NULL, NULL);

        utf8_argv[i] = utf8_str;
    }
    utf8_argv[wargc] = NULL;

    LocalFree(wargv);

    // Перезаписуємо вказівники на UTF-8 версію
    *argc = wargc;
    *argv = utf8_argv;
#else
    // На Linux / macOS argv та термінал вже за замовчуванням у UTF-8
    (void)argc;
    (void)argv;
#endif
}
size_t utf8_strlen(const char *s)
{
    size_t count = 0;
    while (*s)
    {
        // Skip continuation bytes (0x80 to 0xBF)
        if ((*s & 0xC0) != 0x80)
        {
            count++;
        }
        s++;
    }
    return count;
}
// Console utils functions End ================================================================================================================

// Procs functions Start ================================================================================================================

static void fds_append_pipe_data(char **buffer, size_t *len, size_t *cap, const char *chunk, size_t chunk_size)
{
    if (*len + chunk_size + 1 > *cap)
    {
        *cap = (*cap == 0) ? 1024 : (*cap * 2) + chunk_size;
        char *new_buf = (char *)FDS_REALLOC(*buffer, *cap);
        if (new_buf)
        {
            *buffer = new_buf;
        }
        else
        {
            fds_log(FFATAL, "Out of memory! By more memory!!!");
        }
    }
    memcpy(*buffer + *len, chunk, chunk_size);
    *len += chunk_size;
    (*buffer)[*len] = '\0'; // Завжди тримаємо нуль-термінатор для printf
}

// Звільняє пам'ять, виділену під результат команди
void fds_cmd_result_free(fds_cmd_result *res)
{
    if (res->stdout_data)
        FDS_FREE(res->stdout_data);
    if (res->stderr_data)
        FDS_FREE(res->stderr_data);
    res->stdout_data = NULL;
    res->stderr_data = NULL;
    res->stderr_len = 0;
    res->stdout_len = 0;
    res->exit_code = -1;
}

fds_cmd_result fds_cmd_run_ext(const char *cmd_utf8)
{
    fds_cmd_result res = {0};
    // Ініціалізуємо буфери порожніми рядками, щоб завжди можна було безпечно робити printf
    res.stdout_data = (char *)FDS_CALLOC(1, 1);
    res.stderr_data = (char *)FDS_CALLOC(1, 1);

    size_t out_cap = 1, err_cap = 1;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE out_rd, out_wr, err_rd, err_wr;
    // Створюємо пайпи
    if (!CreatePipe(&out_rd, &out_wr, &sa, 0))
        return res;
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0); // Читаючі кінці не успадковуються

    if (!CreatePipe(&err_rd, &err_wr, &sa, 0))
    {
        CloseHandle(out_rd);
        CloseHandle(out_wr);
        return res;
    }
    SetHandleInformation(err_rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = out_wr;
    si.hStdError = err_wr;
    si.wShowWindow = SW_HIDE; // Не блимаємо чорним вікном консолі

    PROCESS_INFORMATION pi = {0};

    // Перетворюємо команду в UTF-16. (Буфер має бути змінним, CreateProcessW може його модифікувати)
    wchar_t *wcmd = fds_internal_utf8_to_utf16(cmd_utf8);

    if (!CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(out_rd);
        CloseHandle(out_wr);
        CloseHandle(err_rd);
        CloseHandle(err_wr);
        return res;
    }

    // Батьківський процес повинен закрити свої копії записуючих кінців
    CloseHandle(out_wr);
    CloseHandle(err_wr);

    res.success = true;
    bool out_open = true, err_open = true;

    // Читаємо пайпи одночасно (Deadlock Prevention)
    while (out_open || err_open)
    {
        bool data_read = false;
        DWORD avail = 0, bytes_read = 0;
        char chunk[4096];

        if (out_open && PeekNamedPipe(out_rd, NULL, 0, NULL, &avail, NULL) && avail > 0)
        {
            if (ReadFile(out_rd, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0)
            {
                fds_append_pipe_data(&res.stdout_data, &res.stdout_len, &out_cap, chunk, bytes_read);
                data_read = true;
            }
            else
            {
                out_open = false;
            }
        }
        else if (out_open && !avail)
        {
            // Перевіряємо, чи процес вже закрив пайп
            if (!PeekNamedPipe(out_rd, NULL, 0, NULL, &avail, NULL))
                out_open = false;
        }

        if (err_open && PeekNamedPipe(err_rd, NULL, 0, NULL, &avail, NULL) && avail > 0)
        {
            if (ReadFile(err_rd, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0)
            {
                fds_append_pipe_data(&res.stderr_data, &res.stderr_len, &err_cap, chunk, bytes_read);
                data_read = true;
            }
            else
            {
                err_open = false;
            }
        }
        else if (err_open && !avail)
        {
            if (!PeekNamedPipe(err_rd, NULL, 0, NULL, &avail, NULL))
                err_open = false;
        }

        // Щоб не палити 100% CPU у циклі, якщо процес нічого не пише
        if (!data_read && (out_open || err_open))
            Sleep(1);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    res.exit_code = exit_code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(out_rd);
    CloseHandle(err_rd);

#else
    // POSIX реалізація
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) == -1 || pipe(err_pipe) == -1)
        return res;

    pid_t pid = fork();
    if (pid < 0)
        return res;

    if (pid == 0)
    { // Child
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);

        // Використовуємо sh -c для того, щоб команда парсилася так само, як у Windows
        execl("/bin/sh", "sh", "-c", cmd_utf8, (char *)NULL);
        exit(127); // Якщо execl провалився
    }

    // Parent
    close(out_pipe[1]);
    close(err_pipe[1]);
    res.success = true;

    // Використовуємо poll() для одночасного читання
    struct pollfd pfd[2];
    pfd[0].fd = out_pipe[0];
    pfd[0].events = POLLIN;
    pfd[1].fd = err_pipe[0];
    pfd[1].events = POLLIN;

    while (pfd[0].fd != -1 || pfd[1].fd != -1)
    {
        if (poll(pfd, 2, -1) < 0)
            break;

        char chunk[4096];

        for (int i = 0; i < 2; i++)
        {
            if (pfd[i].fd != -1 && (pfd[i].revents & POLLIN))
            {
                ssize_t bytes = read(pfd[i].fd, chunk, sizeof(chunk));
                if (bytes > 0)
                {
                    if (i == 0)
                        fds_append_pipe_data(&res.stdout_data, &res.stdout_len, &out_cap, chunk, bytes);
                    else
                        fds_append_pipe_data(&res.stderr_data, &res.stderr_len, &err_cap, chunk, bytes);
                }
                else
                {
                    close(pfd[i].fd);
                    pfd[i].fd = -1; // EOF
                }
            }
            else if (pfd[i].fd != -1 && (pfd[i].revents & (POLLHUP | POLLERR)))
            {
                close(pfd[i].fd);
                pfd[i].fd = -1;
            }
        }
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
    {
        res.exit_code = WEXITSTATUS(status);
    }
    else
    {
        res.exit_code = -1;
    }
#endif

    return res;
}

int fds_cmd_run_Simp(const char *cmd_utf8, char **out_output)
{
    FILE *pipe = NULL;

#ifdef _WIN32
    // На Windows переводимо команду в UTF-16, щоб підтримувати кирилицю в шляхах
    wchar_t *wcmd = fds_internal_utf8_to_utf16(cmd_utf8);
    if (!wcmd)
        return -1;

    // "rt" - read text mode (автоматично конвертує \r\n у \n)
    pipe = _wpopen(wcmd, L"rt");
#else
    // На POSIX системах UTF-8 працює нативно
    pipe = popen(cmd_utf8, "r");
#endif

    if (!pipe)
    {
        if (out_output)
            *out_output = NULL;
        return -1;
    }

    // Якщо користувач хоче отримати вивід
    if (out_output)
    {
        size_t capacity = 1024;
        size_t size = 0;
        char *buffer = (char *)FDS_MALLOC(capacity);

        if (buffer)
        {
            buffer[0] = '\0';
            char chunk[256];

            // Зчитуємо потік шматками
            while (fgets(chunk, sizeof(chunk), pipe) != NULL)
            {
                size_t chunk_len = strlen(chunk);

                // Розширюємо буфер, якщо не вистачає місця
                if (size + chunk_len + 1 > capacity)
                {
                    capacity = capacity * 2 + chunk_len;
                    char *new_buf = (char *)FDS_REALLOC(buffer, capacity);
                    if (!new_buf)
                        break; // Обробка нестачі пам'яті
                    buffer = new_buf;
                }

                strcpy(buffer + size, chunk);
                size += chunk_len;
            }
        }
        *out_output = buffer;
    }
    else
    {
        // Якщо вивід не потрібен, просто чекаємо завершення команди,
        // але треба вичитати буфер, щоб процес не завис, якщо виводу багато
        char dump[256];
        while (fgets(dump, sizeof(dump), pipe) != NULL)
        {
        }
    }
    FDS_FREE(wcmd);
    // Закриваємо pipe і повертаємо код завершення команди
#ifdef _WIN32
    return _pclose(pipe);
#else
    // pclose на POSIX повертає статус, який треба розпакувати через WEXITSTATUS
    int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

// Procs functions End ================================================================================================================

// Event functions Start ================================================================================================================
FdsEventQueue fds_event_queue_init(FdsEvent *buffer, size_t capacity)
{
    FDS_ASSERT(buffer != NULL, "Event buffer pointer cannot be NULL");
    FDS_ASSERT(capacity > 0, "Queue capacity must be greater than 0");

    FdsEventQueue q = {
        .buffer = buffer,
        .capacity = capacity,
        .head = 0,
        .tail = 0,
        .count = 0};
    return q;
}

void fds_event_clear(FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool fds_event_push(FdsEventQueue *q, FdsEvent event)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(q->capacity > 0, "Queue capacity is 0");

    if (q->count >= q->capacity)
    {
        return false;
    }

    q->buffer[q->head] = event;
    q->head = (q->head + 1) % q->capacity;
    q->count++;

    return true;
}

bool fds_event_poll(FdsEventQueue *q, FdsEvent *out_event)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(out_event != NULL, "Output event pointer is NULL");

    if (q->count == 0)
    {
        return false;
    }

    *out_event = q->buffer[q->tail];
    q->tail = (q->tail + 1) % q->capacity;
    q->count--;

    return true;
}

size_t fds_event_poll_many(FdsEventQueue *q, FdsEvent *events, size_t capacity)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    if (capacity == 0)
        return 0;
    FDS_ASSERT(events != NULL, "Output events buffer is NULL");

    size_t to_read = (capacity < q->count) ? capacity : q->count;
    for (size_t i = 0; i < to_read; ++i)
    {
        events[i] = q->buffer[q->tail];
        q->tail = (q->tail + 1) % q->capacity;
    }
    q->count -= to_read;

    return to_read;
}

bool fds_event_peek(const FdsEventQueue *q, FdsEvent *out_event)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");
    FDS_ASSERT(out_event != NULL, "Output event pointer is NULL");

    if (q->count == 0)
    {
        return false;
    }

    *out_event = q->buffer[q->tail];
    return true;
}

bool fds_event_discard(FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");

    if (q->count == 0)
    {
        return false;
    }

    q->tail = (q->tail + 1) % q->capacity;
    q->count--;
    return true;
}

size_t fds_event_discard_many(FdsEventQueue *q, size_t count)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    FDS_ASSERT(q->buffer != NULL, "Queue buffer is NULL");

    size_t to_discard = (count < q->count) ? count : q->count;
    if (to_discard > 0)
    {
        q->tail = (q->tail + to_discard) % q->capacity;
        q->count -= to_discard;
    }

    return to_discard;
}

bool fds_event_is_empty(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->count == 0;
}

bool fds_event_is_full(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->count >= q->capacity;
}

size_t fds_event_count(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->count;
}

size_t fds_event_capacity(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->capacity;
}

size_t fds_event_remaining(const FdsEventQueue *q)
{
    FDS_ASSERT(q != NULL, "Queue pointer is NULL");
    return q->capacity - q->count;
}

// --- Factory Functions ---

FdsEvent fds_event_make(FdsEventType type, uint64_t timestamp)
{
    FdsEvent e = {.type = type, .timestamp = timestamp, .as = {}};
    return e;
}

FdsEvent fds_event_make_i32(FdsEventType type, uint64_t timestamp, int32_t value)
{
    FdsEvent e = {.type = type, .timestamp = timestamp, .as = {}};
    e.as.i32[0] = value;
    return e;
}

FdsEvent fds_event_make_u32(FdsEventType type, uint64_t timestamp, uint32_t value)
{
    FdsEvent e = {.type = type, .timestamp = timestamp, .as = {}};
    e.as.u32[0] = value;
    return e;
}

FdsEvent fds_event_make_f32(FdsEventType type, uint64_t timestamp, float value)
{
    FdsEvent e = {.type = type, .timestamp = timestamp, .as = {}};
    e.as.f32[0] = value;
    return e;
}

FdsEvent fds_event_make_ptr(FdsEventType type, uint64_t timestamp, void *ptr)
{
    FdsEvent e = {.type = type, .timestamp = timestamp, .as = {}};
    e.as.ptr = ptr;
    return e;
}
// Event functions End ================================================================================================================
// Bytes builder adn view functions Start ================================================================================================================

FdsBytesBuilder fds_bb_create(size_t initial_capacity)
{
    FdsBytesBuilder bb = {0};
    if (initial_capacity > 0)
    {
        bb.data = (uint8_t *)FDS_MALLOC(initial_capacity);
        FDS_ASSERT(bb.data != NULL, "BytesBuilder: memory allocation failed");
        bb.capacity = initial_capacity;
    }
    return bb;
}

void fds_bb_destroy(FdsBytesBuilder *bb)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    if (bb->data)
    {
        FDS_FREE(bb->data);
        bb->data = NULL;
    }
    bb->size = 0;
    bb->capacity = 0;
}

void fds_bb_reserve(FdsBytesBuilder *bb, size_t additional_size)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");

    if (bb->size + additional_size <= bb->capacity)
    {
        return; // Already have enough space
    }

    // Standard growth strategy: double the capacity or match exact needs
    size_t new_capacity = bb->capacity == 0 ? 16 : bb->capacity;
    while (new_capacity < bb->size + additional_size)
    {
        new_capacity *= 2;
    }

    uint8_t *new_data = (uint8_t *)FDS_REALLOC(bb->data, new_capacity);
    FDS_ASSERT(new_data != NULL, "BytesBuilder: memory reallocation failed");

    bb->data = new_data;
    bb->capacity = new_capacity;
}

void fds_bb_clear(FdsBytesBuilder *bb)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    bb->size = 0;
}

// --- BytesView Interop ---

FdsBytesView fds_bb_to_view(const FdsBytesBuilder *bb)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    return fds_bv(bb->data, bb->size);
}

FdsBytesBuilder fds_bb_from_view(FdsBytesView view)
{
    FdsBytesBuilder bb = fds_bb_create(view.size);
    if (view.size > 0 && view.data != NULL)
    {
        fds_bb_append(&bb, view.data, view.size);
    }
    return bb;
}

void fds_bb_append_view(FdsBytesBuilder *bb, FdsBytesView view)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    if (view.size > 0 && view.data != NULL)
    {
        fds_bb_append(bb, view.data, view.size);
    }
}

// --- Appending ---

void fds_bb_append(FdsBytesBuilder *bb, const void *data, size_t size)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    if (size == 0 || data == NULL)
        return;

    fds_bb_reserve(bb, size);
    memcpy(bb->data + bb->size, data, size);
    bb->size += size;
}

void fds_bb_append_byte(FdsBytesBuilder *bb, uint8_t byte)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    fds_bb_reserve(bb, 1);
    bb->data[bb->size++] = byte;
}

void fds_bb_append_u16_le(FdsBytesBuilder *bb, uint16_t val)
{
    fds_bb_reserve(bb, 2);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
}

void fds_bb_append_u16_be(FdsBytesBuilder *bb, uint16_t val)
{
    fds_bb_reserve(bb, 2);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
}

void fds_bb_append_u32_le(FdsBytesBuilder *bb, uint32_t val)
{
    fds_bb_reserve(bb, 4);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 24) & 0xFF);
}

void fds_bb_append_u32_be(FdsBytesBuilder *bb, uint32_t val)
{
    fds_bb_reserve(bb, 4);
    bb->data[bb->size++] = (uint8_t)((val >> 24) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
}
void fds_bb_append_u64_le(FdsBytesBuilder *bb, uint64_t val)
{
    fds_bb_reserve(bb, 8);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 24) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 32) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 40) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 48) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 56) & 0xFF);
}

void fds_bb_append_u64_be(FdsBytesBuilder *bb, uint64_t val)
{
    fds_bb_reserve(bb, 8);
    bb->data[bb->size++] = (uint8_t)((val >> 56) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 48) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 40) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 32) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 24) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
}

void fds_bb_append_f32_le(FdsBytesBuilder *bb, float val)
{
    uint32_t temp;
    memcpy(&temp, &val, sizeof(float));
    fds_bb_append_u32_le(bb, temp);
}

void fds_bb_append_f32_be(FdsBytesBuilder *bb, float val)
{
    uint32_t temp;
    memcpy(&temp, &val, sizeof(float));
    fds_bb_append_u32_be(bb, temp);
}

void fds_bb_append_f64_le(FdsBytesBuilder *bb, double val)
{
    uint64_t temp;
    memcpy(&temp, &val, sizeof(double));
    fds_bb_append_u64_le(bb, temp);
}

void fds_bb_append_f64_be(FdsBytesBuilder *bb, double val)
{
    uint64_t temp;
    memcpy(&temp, &val, sizeof(double));
    fds_bb_append_u64_be(bb, temp);
}

size_t fds_bb_get_pos(const FdsBytesBuilder *bb)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder is NULL");
    return bb->size;
}

void fds_bb_patch_u32_le(FdsBytesBuilder *bb, size_t offset, uint32_t val)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder is NULL");
    FDS_ASSERT(offset + 4 <= bb->size, "Patch offset out of bounds");

    bb->data[offset] = (uint8_t)(val & 0xFF);
    bb->data[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

// --- У fds_bytes_builder.h ---
void fds_bb_append_cstr(FdsBytesBuilder *bb, const char *str)
{
    if (!str)
        return;
    size_t len = strlen(str);
    fds_bb_append(bb, str, len + 1); // +1 для '\0'
}

void fds_bb_append_string_u16(FdsBytesBuilder *bb, const char *str)
{
    if (!str)
    {
        fds_bb_append_u16_le(bb, 0);
        return;
    }
    size_t len = strlen(str);
    FDS_ASSERT(len <= 0xFFFF, "String too long for u16 prefix");
    fds_bb_append_u16_le(bb, (uint16_t)len);
    fds_bb_append(bb, str, len);
}

bool fds_bb_save_to_file(const FdsBytesBuilder *bb, const char *filepath)
{
    FDS_ASSERT(bb != NULL, "BytesBuilder is NULL");
    FDS_ASSERT(filepath != NULL, "Filepath is NULL");

    if (bb->size == 0)
        return true; // Нічого зберігати

    FILE *file = fopen(filepath, "wb");
    if (!file)
        return false;

    size_t written = fwrite(bb->data, 1, bb->size, file);
    fclose(file);

    return written == bb->size;
}
FdsBytesView fds_bv(const void *data, size_t size)
{
    return (FdsBytesView){.data = (const uint8_t *)data, .size = size};
}

FdsBytesView fds_bv_empty(void)
{
    return (FdsBytesView){.data = NULL, .size = 0};
}

FdsBytesView fds_bv_from_cstr(const char *str)
{
    if (!str)
        return fds_bv_empty();
    return fds_bv(str, strlen(str));
}

FdsBytesView fds_bv_subview(FdsBytesView view, size_t offset, size_t length)
{
    if (offset >= view.size)
        return fds_bv_empty();
    size_t available = view.size - offset;
    size_t actual_len = length < available ? length : available;
    return fds_bv(view.data + offset, actual_len);
}

FdsBytesView fds_bv_take(FdsBytesView view, size_t count)
{
    return fds_bv_subview(view, 0, count);
}

FdsBytesView fds_bv_skip(FdsBytesView view, size_t count)
{
    if (count >= view.size)
        return fds_bv_empty();
    return fds_bv(view.data + count, view.size - count);
}

bool fds_bv_equals(FdsBytesView a, FdsBytesView b)
{
    if (a.size != b.size)
        return false;
    if (a.data == b.data)
        return true;
    if (!a.data || !b.data)
        return false;
    return memcmp(a.data, b.data, a.size) == 0;
}

bool fds_bv_has_prefix(FdsBytesView view, FdsBytesView prefix)
{
    if (prefix.size > view.size)
        return false;
    return fds_bv_equals(fds_bv_take(view, prefix.size), prefix);
}

bool fds_bv_has_suffix(FdsBytesView view, FdsBytesView suffix)
{
    if (suffix.size > view.size)
        return false;
    return fds_bv_equals(fds_bv_subview(view, view.size - suffix.size, suffix.size), suffix);
}

intptr_t fds_bv_find_byte(FdsBytesView view, uint8_t byte)
{
    if (!view.data || view.size == 0)
        return -1;
    const void *ptr = memchr(view.data, byte, view.size);
    if (!ptr)
        return -1;
    return (intptr_t)((const uint8_t *)ptr - view.data);
}

intptr_t fds_bv_find_subview(FdsBytesView view, FdsBytesView pattern)
{
    if (pattern.size == 0 || pattern.size > view.size)
        return -1;
    if (!view.data || !pattern.data)
        return -1;

    size_t max_idx = view.size - pattern.size;
    for (size_t i = 0; i <= max_idx; ++i)
    {
        if (memcmp(view.data + i, pattern.data, pattern.size) == 0)
        {
            return (intptr_t)i;
        }
    }
    return -1;
}

bool fds_bv_pop_byte(FdsBytesView *view, uint8_t *out_byte)
{
    FDS_ASSERT(view != NULL, "BytesView pointer is NULL");
    if (view->size == 0 || !view->data)
        return false;
    if (out_byte)
        *out_byte = view->data[0];
    view->data++;
    view->size--;
    return true;
}

FdsBytesView fds_bv_pop_bytes(FdsBytesView *view, size_t count)
{
    FDS_ASSERT(view != NULL, "BytesView pointer is NULL");
    size_t actual_count = count < view->size ? count : view->size;
    FdsBytesView result = fds_bv(view->data, actual_count);
    view->data += actual_count;
    view->size -= actual_count;
    return result;
}

bool fds_bv_read_u16_le(FdsBytesView *view, uint16_t *out_val)
{
    if (view->size < 2)
        return false;
    if (out_val)
    {
        *out_val = (uint16_t)view->data[0] | ((uint16_t)view->data[1] << 8);
    }
    view->data += 2;
    view->size -= 2;
    return true;
}

bool fds_bv_read_u16_be(FdsBytesView *view, uint16_t *out_val)
{
    if (view->size < 2)
        return false;
    if (out_val)
    {
        *out_val = ((uint16_t)view->data[0] << 8) | (uint16_t)view->data[1];
    }
    view->data += 2;
    view->size -= 2;
    return true;
}

bool fds_bv_read_u32_le(FdsBytesView *view, uint32_t *out_val)
{
    if (view->size < 4)
        return false;
    if (out_val)
    {
        *out_val = (uint32_t)view->data[0] | ((uint32_t)view->data[1] << 8) | ((uint32_t)view->data[2] << 16) | ((uint32_t)view->data[3] << 24);
    }
    view->data += 4;
    view->size -= 4;
    return true;
}

bool fds_bv_read_u32_be(FdsBytesView *view, uint32_t *out_val)
{
    if (view->size < 4)
        return false;
    if (out_val)
    {
        *out_val = ((uint32_t)view->data[0] << 24) | ((uint32_t)view->data[1] << 16) | ((uint32_t)view->data[2] << 8) | (uint32_t)view->data[3];
    }
    view->data += 4;
    view->size -= 4;
    return true;
}

bool fds_bv_read_u64_le(FdsBytesView *view, uint64_t *out_val)
{
    if (view->size < 8)
        return false;
    if (out_val)
    {
        *out_val = (uint64_t)view->data[0] | ((uint64_t)view->data[1] << 8) | ((uint64_t)view->data[2] << 16) | ((uint64_t)view->data[3] << 24) | ((uint64_t)view->data[4] << 32) | ((uint64_t)view->data[5] << 40) | ((uint64_t)view->data[6] << 48) | ((uint64_t)view->data[7] << 56);
    }
    view->data += 8;
    view->size -= 8;
    return true;
}

bool fds_bv_read_u64_be(FdsBytesView *view, uint64_t *out_val)
{
    if (view->size < 8)
        return false;
    if (out_val)
    {
        *out_val = ((uint64_t)view->data[0] << 56) | ((uint64_t)view->data[1] << 48) | ((uint64_t)view->data[2] << 40) | ((uint64_t)view->data[3] << 32) | ((uint64_t)view->data[4] << 24) | ((uint64_t)view->data[5] << 16) | ((uint64_t)view->data[6] << 8) | (uint64_t)view->data[7];
    }
    view->data += 8;
    view->size -= 8;
    return true;
}

bool fds_bv_read_f32_le(FdsBytesView *view, float *out_val)
{
    uint32_t temp;
    if (!fds_bv_read_u32_le(view, &temp))
        return false;
    if (out_val)
        memcpy(out_val, &temp, sizeof(float));
    return true;
}

bool fds_bv_read_f32_be(FdsBytesView *view, float *out_val)
{
    uint32_t temp;
    if (!fds_bv_read_u32_be(view, &temp))
        return false;
    if (out_val)
        memcpy(out_val, &temp, sizeof(float));
    return true;
}

bool fds_bv_read_f64_le(FdsBytesView *view, double *out_val)
{
    uint64_t temp;
    if (!fds_bv_read_u64_le(view, &temp))
        return false;
    if (out_val)
        memcpy(out_val, &temp, sizeof(double));
    return true;
}

bool fds_bv_read_f64_be(FdsBytesView *view, double *out_val)
{
    uint64_t temp;
    if (!fds_bv_read_u64_be(view, &temp))
        return false;
    if (out_val)
        memcpy(out_val, &temp, sizeof(double));
    return true;
}
bool fds_bv_read_string_u16(FdsBytesView *view, FdsBytesView *out_str_view)
{
    uint16_t len;
    if (!fds_bv_read_u16_le(view, &len))
        return false;

    if (view->size < len)
        return false;

    if (out_str_view)
    {
        *out_str_view = fds_bv(view->data, len);
    }

    view->data += len;
    view->size -= len;
    return true;
}
// Bytes builder adn view functions End ================================================================================================================

// FIleIO functions Start ================================================================================================================
SV sv_chop_by_delim(SV *sv, char delim)
{
    size_t i = 0;
    while (i < sv->count && sv->data[i] != delim)
    {
        i += 1;
    }

    SV result = sv_from_parts(sv->data, i);

    if (i < sv->count)
    {
        sv->count -= i + 1;
        sv->data += i + 1;
    }
    else
    {
        sv->count -= i;
        sv->data += i;
    }

    return result;
}

SV sv_chop_left(SV *sv, size_t n)
{
    if (n > sv->count)
    {
        n = sv->count;
    }

    SV result = sv_from_parts(sv->data, n);

    sv->data += n;
    sv->count -= n;

    return result;
}

SV sv_chop_right(SV *sv, size_t n)
{
    if (n > sv->count)
    {
        n = sv->count;
    }

    SV result = sv_from_parts(sv->data + sv->count - n, n);

    sv->count -= n;

    return result;
}

SV sv_trim_left_ext(SV sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i]))
    {
        i += 1;
    }

    return sv_from_parts(sv.data + i, sv.count - i);
}

SV sv_trim_right_ext(SV sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i]))
    {
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
    if (!f)
        return false;

    // 1. Записуємо кількість елементів (заголовок)
    if (fwrite(&count, sizeof(size_t), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    // 2. Записуємо самі елементи з купи
    if (count > 0 && items != NULL)
    {
        if (fwrite(items, item_size, count, f) != count)
        {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

bool fds_da_read_file(const char *filepath, void **out_items, size_t *out_count, size_t item_size)
{
    if (!filepath || !out_items || !out_count)
        return false;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return false;

    // 1. Читаємо кількість елементів
    size_t count = 0;
    if (fread(&count, sizeof(size_t), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    if (count == 0)
    {
        *out_items = NULL;
        *out_count = 0;
        fclose(f);
        return true;
    }

    // 2. Виділяємо пам'ять під буфер
    void *items = FDS_MALLOC(count * item_size);
    if (!items)
    {
        fclose(f);
        return false;
    }

    // 3. Зчитуємо елементи
    if (fread(items, item_size, count, f) != count)
    {
        FDS_FREE(items);
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
    if (!filepath || !out_buf || !out_size)
        return false;

    *out_buf = NULL;
    *out_size = 0;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return false;

    // Отримуємо розмір файлу
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        return false;
    }

    fseek(f, 0, SEEK_SET);

    // Якщо файл порожній — повертаємо успіх із NULL буфером
    if (size == 0)
    {
        fclose(f);
        return true;
    }

    void *buffer = FDS_MALLOC((size_t)size);
    if (!buffer)
    {
        fclose(f);
        return false;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (bytes_read != (size_t)size)
    {
        FDS_FREE(buffer);
        return false;
    }

    *out_buf = buffer;
    *out_size = (size_t)size;
    return true;
}

bool fds_file_write_bytes(const char *filepath, const void *buf, size_t size)
{
    if (!filepath)
        return false;
    if (size > 0 && !buf)
        return false;

    FILE *f = fopen(filepath, "wb");
    if (!f)
        return false;

    if (size > 0)
    {
        size_t bytes_written = fwrite(buf, 1, size, f);
        if (bytes_written != size)
        {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

bool fds_file_append_bytes(const char *filepath, const void *buf, size_t size)
{
    if (!filepath)
        return false;
    if (size == 0)
        return true;
    if (!buf)
        return false;

    FILE *f = fopen(filepath, "ab");
    if (!f)
        return false;

    size_t bytes_written = fwrite(buf, 1, size, f);
    fclose(f);

    return bytes_written == size;
}






bool fds_read_entire_file(const char *filepath, char **out_data, size_t *out_size)
{
    FILE *file = NULL;
    char *buffer = NULL;
    long size = 0;

    FDS_ASSERT(filepath != NULL, "filepath must not be NULL");
    FDS_ASSERT(out_data != NULL, "out_data must not be NULL");

    if (out_size) *out_size = 0;
    *out_data = NULL;

#ifdef _WIN32
    wchar_t wpath[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wpath, 1024) > 0) {
        file = _wfopen(wpath, L"rb");
    } else {
        file = fopen(filepath, "rb");
    }
#else
    file = fopen(filepath, "rb");
#endif

    if (!file) return false;

    if (fseek(file, 0, SEEK_END) != 0) goto fail;
    size = ftell(file);
    if (size < 0) goto fail;
    if (fseek(file, 0, SEEK_SET) != 0) goto fail;

    buffer = (char *)FDS_MALLOC((size_t)size + 1);
    if (!buffer) goto fail;

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        FDS_FREE(buffer);
        goto fail;
    }

    buffer[size] = '\0';
    fclose(file);

    *out_data = buffer;
    if (out_size) *out_size = (size_t)size;
    return true;

fail:
    fclose(file);
    return false;
}








FdsFile fds_file_open(const char *path, uint32_t flags)
{
    FdsFile file = {.handle = 0, .is_valid = false};
    if (!path)
        return file;

    const char *mode = "rb";

    if ((flags & FDS_FILE_READ) && (flags & FDS_FILE_WRITE))
    {
        if (flags & FDS_FILE_CREATE)
            mode = "w+b";
        else if (flags & FDS_FILE_APPEND)
            mode = "a+b";
        else
            mode = "r+b";
    }
    else if (flags & FDS_FILE_WRITE)
    {
        if (flags & FDS_FILE_APPEND)
            mode = "ab";
        else
            mode = "wb";
    }
    else if (flags & FDS_FILE_APPEND)
    {
        mode = "ab";
    }

    FILE *f = fopen(path, mode);
    if (f)
    {
        file.handle = (uintptr_t)f;
        file.is_valid = true;
    }

    return file;
}

void fds_file_close(FdsFile *file)
{
    if (!file || !file->is_valid)
        return;

    FILE *f = FDS_FILE_PTR(*file);
    if (f)
        fclose(f);

    file->handle = 0;
    file->is_valid = false;
}

size_t fds_file_read(FdsFile file, void *dst, size_t size)
{
    if (!file.is_valid || !dst || size == 0)
        return 0;
    return fread(dst, 1, size, FDS_FILE_PTR(file));
}

size_t fds_file_write(FdsFile file, const void *src, size_t size)
{
    if (!file.is_valid || !src || size == 0)
        return 0;
    return fwrite(src, 1, size, FDS_FILE_PTR(file));
}

bool fds_file_seek(FdsFile file, int64_t offset, FdsSeekOrigin origin)
{
    if (!file.is_valid)
        return false;

    int std_origin = SEEK_SET;
    if (origin == FDS_SEEK_CUR)
        std_origin = SEEK_CUR;
    if (origin == FDS_SEEK_END)
        std_origin = SEEK_END;

#if defined(_WIN32)
    return _fseeki64(FDS_FILE_PTR(file), offset, std_origin) == 0;
#else
    return fseeko(FDS_FILE_PTR(file), (off_t)offset, std_origin) == 0;
#endif
}

int64_t fds_file_tell(FdsFile file)
{
    if (!file.is_valid)
        return -1;

#if defined(_WIN32)
    return _ftelli64(FDS_FILE_PTR(file));
#else
    return (int64_t)ftello(FDS_FILE_PTR(file));
#endif
}

int64_t fds_file_size(FdsFile file)
{
    if (!file.is_valid)
        return -1;

    int64_t current = fds_file_tell(file);
    if (current < 0)
        return -1;

    if (!fds_file_seek(file, 0, FDS_SEEK_END))
        return -1;

    int64_t size = fds_file_tell(file);
    fds_file_seek(file, current, FDS_SEEK_SET);

    return size;
}

void fds_file_flush(FdsFile file)
{
    if (!file.is_valid)
        return;
    fflush(FDS_FILE_PTR(file));
}

bool fds_file_write_magic(FdsFile *file, const char magic[4], uint32_t version)
{
    if (!file || !file->is_valid)
        return false;

    FdsFileHeader header;
    memcpy(header.magic, magic, 4);
    header.version = version;

    size_t written = fds_file_write(*file, &header, sizeof(FdsFileHeader));
    return written == sizeof(FdsFileHeader);
}

bool fds_file_skip(FdsFile file, int64_t bytes_to_skip)
{
    // Якщо просити пропустити 0 байт — це успіх, нічого робити не треба
    if (bytes_to_skip == 0)
        return true;

    // Забороняємо відмотувати файл назад через skip (для цього є чіткий seek)
    if (bytes_to_skip < 0)
        return false;

    return fds_file_seek(file, bytes_to_skip, FDS_SEEK_CUR);
}

bool fds_file_check_magic(FdsFile *file, const char expected_magic[4], uint32_t min_version)
{
    if (!file || !file->is_valid)
        return false;

    FdsFileHeader header = {0};
    size_t read_bytes = fds_file_read(*file, &header, sizeof(FdsFileHeader));

    if (read_bytes != sizeof(FdsFileHeader))
    {
        return false;
    }

    if (memcmp(header.magic, expected_magic, 4) != 0)
    {
        return false;
    }

    if (header.version < min_version)
    {
        return false;
    }

    return true;
}

bool fds_file_write_str(FdsFile file, const char *str)
{
    if (!str)
        return false;
    uint32_t len = (uint32_t)strlen(str);
    if (fds_file_write(file, &len, sizeof(len)) != sizeof(len))
        return false;
    return fds_file_write(file, str, len) == len;
}

// Повертає зліпок рядка
char *fds_file_read_str(FdsFile file, void *(*allocator)(size_t))
{
    uint32_t len = 0;
    if (fds_file_read(file, &len, sizeof(len)) != sizeof(len))
        return NULL;
    char *buf = (char*)allocator(len + 1);
    if (!buf)
        return NULL;
    if (fds_file_read(file, buf, len) != len)
        return NULL;
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

FdsMappedFile fds_file_map(const char *path, uint32_t flags)
{
    FdsMappedFile mapped = {0};
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
    mapped.size = (size_t)file_size.QuadPart;
    mapped.file_handle = (uintptr_t)hFile;
    mapped.mapping_handle = (uintptr_t)hMapping;
    mapped.is_valid = true;

#else // POSIX (Linux / macOS)
    int open_flags = writable ? O_RDWR : O_RDONLY;
    int prot = PROT_READ | (writable ? PROT_WRITE : 0);

    int fd = open(path, open_flags);
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
    mapped.size = (size_t)st.st_size;
    mapped.file_handle = (uintptr_t)fd;
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
    close((int)mapped->file_handle);
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

FILE *fds_fmemopen_win32(void *buf, size_t size, const char *mode)
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
    int fd = _open_osfhandle((intptr_t)hRead, _O_RDONLY | _O_BINARY);
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
    FdsFile file = {.handle = 0, .is_valid = false};
    if (!mapped || !mapped->is_valid)
        return file;

    // Створюємо FILE* потік поверх пам'яті mmap
    FILE *f = fmemopen(mapped->data, mapped->size, "rwb");
    if (f)
    {
        file.handle = (uintptr_t)f;
        file.is_valid = true;
    }

    return file;
}

// Перетворення мапленого файлу в StringView за 1 виклик
SV fds_file_mapped_as_sv(FdsMappedFile *mapped)
{
    if (!mapped || !mapped->is_valid)
        return (SV){0};
    return (SV){
        .count = mapped->size,
        .data = (const char *)mapped->data};
}
// FIleIO functions End ================================================================================================================

// String Array functions Start ================================================================================================================

bool sa_pushf(StringArray *sa, const char *fmt, ...)
{
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len < 0)
    {
        va_end(args_copy);
        return false;
    }

    char *buf = (char *)FDS_MALLOC(len + 1);
    if (!buf)
    {
        va_end(args_copy);
        return false;
    }

    vsnprintf(buf, len + 1, fmt, args_copy);
    va_end(args_copy);

    // We add buf directly (without repeated strdup)
    if (sa->size == sa->capacity && !sa_grow(sa))
    {
        FDS_FREE(buf);
        return false;
    }
    sa->data[sa->size++] = buf;
    if (sa->data)
        sa->data[sa->size] = NULL; // NULL-sentinel
    return true;
}
// Reverse array of pointers in place
void sa_reverse(StringArray *sa)
{
    if (!sa || sa->size < 2)
        return;
    for (size_t i = 0; i < sa->size / 2; i++)
    {
        char *tmp = sa->data[i];
        sa->data[i] = sa->data[sa->size - 1 - i];
        sa->data[sa->size - 1 - i] = tmp;
    }
}

// Transfer to registers
void sa_to_upper(StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        for (char *p = sa->data[i]; *p; ++p)
            *p = (char)toupper((unsigned char)*p);
    }
}

void sa_to_lower(StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        for (char *p = sa->data[i]; *p; ++p)
            *p = (char)tolower((unsigned char)*p);
    }
}

// Search with a custom comparator
size_t sa_find_custom(const StringArray *sa, const char *needle,
                      int (*cmp)(const char *, const char *))
{
    if (!cmp)
        cmp = strcmp;
    for (size_t i = 0; i < sa->size; i++)
    {
        if (cmp(sa->data[i], needle) == 0)
            return i;
    }
    return (size_t)-1; // or SIZE_MAX
}

bool sa_copy(StringArray *dst, const StringArray *src)
{
    // The initial capacity is the same as that of the source, but at least 1
    size_t cap = src->size > 0 ? src->size : 1;
    if (!sa_new(dst, cap))
        return false;

    for (size_t i = 0; i < src->size; i++)
    {
        if (!sa_push(dst, src->data[i]))
        {
            // If it was not possible to add, we release everything and return an error
            sa_free(dst);
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------
// Add all lines from src to dst (dst is already initialized)
// ------------------------------------------------------------
bool sa_append_array(StringArray *dst, const StringArray *src)
{
    for (size_t i = 0; i < src->size; i++)
    {
        if (!sa_push(dst, src->data[i]))
            return false;
    }
    return true;
}

// ------------------------------------------------------------
// Internal function: Creates a new string without trailing spaces
// ------------------------------------------------------------
static char *trim_whitespace(const char *s)
{
    if (!s)
        return NULL;

    // We skip spaces at the beginning
    const char *start = s;
    while (*start && isspace((unsigned char)*start))
        start++;

    // We find the end without spaces
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;

    size_t len = (size_t)(end - start);
    char *trimmed = (char *)FDS_MALLOC(len + 1);
    if (!trimmed)
        return NULL;

    memcpy(trimmed, start, len);
    trimmed[len] = '\0';
    return trimmed;
}

// ------------------------------------------------------------
// Trim spaces on a specific line (replaces the line in place)
// ------------------------------------------------------------
bool sa_trim_at(StringArray *sa, size_t idx)
{
    if (idx >= sa->size)
        return false;

    char *trimmed = trim_whitespace(sa->data[idx]);
    if (!trimmed)
        return false; // it was not possible to allocate memory, we leave the old line

    FDS_FREE(sa->data[idx]);
    sa->data[idx] = trimmed;
    return true;
}

// ------------------------------------------------------------
// Trim spaces in all rows of the array
// ------------------------------------------------------------
void sa_trim(StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        // Ignore possible errors - it is better to leave the original
        sa_trim_at(sa, i);
    }
}

// ------------------------------------------------------------
// Checking for the presence of a line
// ------------------------------------------------------------
bool sa_contains(const StringArray *sa, const char *str)
{
    return sa_find(sa, str) != (size_t)-1;
}
bool sa_init_from_strings(StringArray *sa, const char *first, ...)
{
    sa_new(sa, 4); // the initial capacity is common sense

    if (!first)
        return true; // empty list

    va_list args;
    va_start(args, first);

    const char *s = first;
    while (s != NULL)
    {
        if (!sa_push(sa, s))
        {
            va_end(args);
            sa_free(sa);
            return false;
        }
        // FIXED: Read pointer to char, not char itself
        s = va_arg(args, const char *);
    }

    va_end(args);
    return true;
}
bool sa_push_many_impl(StringArray *sa, const char *first, ...)
{
    if (!first)
        return true; // empty list

    va_list args;
    va_start(args, first);

    const char *s = first;
    while (s != NULL)
    {
        if (!sa_push(sa, s))
        {
            va_end(args);
            sa_free(sa);
            return false;
        }
        // FIXED: Read pointer to char, not char itself
        s = va_arg(args, const char *);
    }

    va_end(args);
    return true;
}
char *sa_join(const StringArray *sa, const char *delim)
{
    if (sa->size == 0)
        return str_dup("");
    size_t delim_len = delim ? strlen(delim) : 0;
    size_t total = 1; // for '\0'
    for (size_t i = 0; i < sa->size; i++)
    {
        total += strlen(sa->data[i]);
        if (i < sa->size - 1)
            total += delim_len;
    }
    char *result = (char *)FDS_MALLOC(total);
    if (!result)
        return NULL;
    char *ptr = result;
    for (size_t i = 0; i < sa->size; i++)
    {
        size_t len = strlen(sa->data[i]);
        memcpy(ptr, sa->data[i], len);
        ptr += len;
        if (delim_len && i < sa->size - 1)
        {
            memcpy(ptr, delim, delim_len);
            ptr += delim_len;
        }
    }
    *ptr = '\0';
    return result;
}

bool sa_split(StringArray *sa, const char *str, const char *delim, bool skip_empty)
{
    // strategy: we look for occurrences of delim and select substrings
    char *copy = str_dup(str);
    if (!copy)
        return false;
    char *token = strtok(copy, delim);
    while (token)
    {
        if (!skip_empty || *token != '\0')
        {
            if (!sa_push(sa, token))
            {
                FDS_FREE(copy);
                return false;
            }
        }
        token = strtok(NULL, delim);
    }
    FDS_FREE(copy);
    return true;
}

// comparator by default
static inline int default_cmp(const void *a, const void *b)
{
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    return strcmp(str_a, str_b);
}

void sa_sort(StringArray *sa, int (*cmp)(const void *, const void *))
{
    if (!cmp)
    {
        // We use a safe comparator
        qsort(sa->data, sa->size, sizeof(char *), default_cmp);
    }
    else
    {
        // For a custom comparator, we still wrap
        // (can be done similarly if needed)
        qsort(sa->data, sa->size, sizeof(char *), cmp);
    }
}

size_t sa_find(const StringArray *sa, const char *str)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        if (strcmp(sa->data[i], str) == 0)
            return i;
    }
    return (size_t)-1;
}
// Just print each line on a new line
void sa_print(const StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        printf("%s\n", sa->data[i]);
    }
}

// Print each line with the index in square brackets
void sa_print_lines(const StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        printf("  [%zu] %s\n", i, sa->data[i]);
    }
}

// If output to a file is required
void sa_fprint(FILE *stream, const StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
    {
        fprintf(stream, "%s\n", sa->data[i]);
    }
}
// --- Internal: duplicate a string safely ---
static char *str_dup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *copy = (char *)FDS_MALLOC(len + 1);
    if (copy)
        memcpy(copy, s, len + 1);
    return copy;
}

// --- Internal: grow the data array ---
static bool sa_grow(StringArray *sa)
{
    size_t new_cap = sa->capacity == 0 ? 4 : sa->capacity * 2;
    char **tmp = (char **)FDS_REALLOC(sa->data, new_cap * sizeof(char *));
    if (!tmp)
        return false;
    sa->data = tmp;
    sa->capacity = new_cap;
    return true;
}

bool sa_new(StringArray *sa, size_t initial_cap)
{
    sa->data = NULL;
    sa->size = 0;
    sa->capacity = 0;
    if (initial_cap > 0)
    {
        sa->data = (char **)FDS_MALLOC(initial_cap * sizeof(char *));
        if (!sa->data)
            return false;
        sa->capacity = initial_cap;
    }
    return true;
}

void sa_free(StringArray *sa)
{
    sa_clear(sa);
    FDS_FREE(sa->data);
    sa->data = NULL;
    sa->capacity = 0;
}

bool sa_push(StringArray *sa, const char *str)
{
    char *copy = str_dup(str);
    if (!copy)
        return false;
    if (sa->size == sa->capacity && !sa_grow(sa))
    {
        FDS_FREE(copy);
        return false;
    }
    sa->data[sa->size++] = copy;
    return true;
}

char *sa_pop(StringArray *sa)
{
    if (sa->size == 0)
        return NULL;
    return sa->data[--sa->size];
}

bool sa_insert(StringArray *sa, size_t idx, const char *str)
{
    if (idx > sa->size)
        return false;
    if (sa->size == sa->capacity && !sa_grow(sa))
        return false;
    char *copy = str_dup(str);
    if (!copy)
        return false;
    // shift elements to the right
    memmove(&sa->data[idx + 1], &sa->data[idx],
            (sa->size - idx) * sizeof(char *));
    sa->data[idx] = copy;
    sa->size++;
    return true;
}

bool sa_remove(StringArray *sa, size_t idx)
{
    if (idx >= sa->size)
        return false;
    FDS_FREE(sa->data[idx]);
    // shift remaining elements left
    memmove(&sa->data[idx], &sa->data[idx + 1],
            (sa->size - idx - 1) * sizeof(char *));
    sa->size--;
    return true;
}

char *sa_get(StringArray *sa, size_t idx)
{
    return (idx < sa->size) ? sa->data[idx] : NULL;
}

bool sa_set(StringArray *sa, size_t idx, const char *str)
{
    if (idx >= sa->size)
        return false;
    char *copy = str_dup(str);
    if (!copy)
        return false;
    FDS_FREE(sa->data[idx]);
    sa->data[idx] = copy;
    return true;
}

size_t sa_len(StringArray *sa)
{
    return sa->size;
}

void sa_clear(StringArray *sa)
{
    for (size_t i = 0; i < sa->size; i++)
        FDS_FREE(sa->data[i]);
    sa->size = 0;
}

// String Array functions End ================================================================================================================

// String view functions Start ================================================================================================================

int sv_next_line(SV *text, SV *out_line)
{
    if (!text || !out_line || text->count == 0)
    {
        return 1; /* No more strings or false arguments */
    }

    size_t i = 0;
    while (i < text->count && text->data[i] != '\n')
    {
        i++;
    }

    /* We form SV for the current line */
    *out_line = sv_from_parts(text->data, i);

    /* Windows format processing: if the line ends with \r, we discard it */
    if (out_line->count > 0 && out_line->data[out_line->count - 1] == '\r')
    {
        out_line->count--;
    }

    /* We move the original text forward, skipping the \n itself */
    size_t advance = (i < text->count) ? i + 1 : i;
    sv_remove_prefix(text, advance);

    return 0;
}
void sv_remove_prefix(SV *sv, size_t count)
{
    if (count > sv->count)
        count = sv->count;
    sv->data += count;
    sv->count -= count;
}

char *sv_to_cstr(SV sv)
{
    char *cstr = (char *)FDS_MALLOC(sv.count + 1);
    if (cstr == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    if (sv.count > 0)
    {
        memcpy(cstr, sv.data, sv.count);
    }
    cstr[sv.count] = '\0';
    return cstr;
}

char sv_at(SV sv, size_t index)
{
    FDS_ASSERT(index < sv.count, "index < sv.count");
    return sv.data[index];
}

SV sv_new(void)
{
    SV sv;
    sv.count = 0;
    sv.data = EMPTY_STR; /* safe empty string */
    return sv;
}

SV sv_from_cstr(const char *str)
{
    FDS_ASSERT(str != NULL, "str != NULL");
    SV sv;
    sv.count = strlen(str);
    sv.data = sv.count ? str : EMPTY_STR;
    return sv;
}

SV sv_from_sb(const SB *sb)
{
    SV sv;
    sv.count = sb->count;
    sv.data = sb->count ? sb->items : EMPTY_STR;
    return sv;
}

SV sv_from_parts(const char *str, size_t len)
{
    FDS_ASSERT(str != NULL || len == 0, "str != NULL || len == 0");
    SV sv;
    sv.count = len;
    sv.data = (len > 0) ? str : EMPTY_STR;
    return sv;
}

int sv_eq(SV sv1, SV sv2)
{
    if (sv1.count != sv2.count)
        return 0;
    if (sv1.count == 0)
        return 1; /* both are empty - equal */
    return memcmp(sv1.data, sv2.data, sv1.count) == 0;
}

int sv_eq_cstr(SV sv1, const char *str)
{
    FDS_ASSERT(str != NULL, "str != NULL");
    size_t len = strlen(str);
    if (sv1.count != len)
        return 0;
    if (sv1.count == 0)
        return 1;
    return memcmp(sv1.data, str, sv1.count) == 0;
}

void sv_trim_left(SV *sv)
{
    while (sv->count && isspace((unsigned char)*sv->data))
    {
        sv->data++;
        sv->count--;
    }
}

void sv_trim_right(SV *sv)
{
    while (sv->count && isspace((unsigned char)sv->data[sv->count - 1]))
    {
        sv->count--;
    }
}

void sv_trim(SV *sv)
{
    sv_trim_left(sv);
    sv_trim_right(sv);
}

void sv_slice(SV *sv, size_t begin, size_t end)
{
    if (begin > end)
        begin = end;
    if (end > sv->count)
        end = sv->count;
    sv->data += begin;
    sv->count = end - begin;
    if (sv->count == 0)
        sv->data = EMPTY_STR;
}

void sv_remove_suffix(SV *sv, size_t count)
{
    if (count > sv->count)
        count = sv->count;
    sv->count -= count;
    if (sv->count == 0)
        sv->data = EMPTY_STR;
}

int sv_ends_with(SV sv, SV suffix)
{
    if (suffix.count == 0)
        return 1;
    if (suffix.count > sv.count)
        return 0;
    return memcmp(sv.data + sv.count - suffix.count, suffix.data, suffix.count) == 0;
}

int sv_starts_with(SV sv, SV prefix)
{
    if (prefix.count == 0)
        return 1;
    if (prefix.count > sv.count)
        return 0;
    return memcmp(sv.data, prefix.data, prefix.count) == 0;
}

int sv_starts_with_char(SV sv, char c)
{
    return sv.count > 0 && sv.data[0] == c;
}

int sv_ends_with_char(SV sv, char c)
{
    return sv.count > 0 && sv.data[sv.count - 1] == c;
}

SV sv_split_left(SV *sv, char c)
{
    size_t pos = sv_find_char(*sv, c);
    if (pos == SIZE_MAX)
    {
        SV out = *sv;
        sv->count = 0;
        sv->data = EMPTY_STR; /* the right remainder is empty */
        return out;
    }
    SV out = sv_from_parts(sv->data, pos);
    sv_remove_prefix(sv, pos + 1);
    return out;
}

SV sv_split_right(SV *sv, char c)
{
    size_t pos = sv_rfind_char(*sv, c);
    if (pos == SIZE_MAX)
    {
        /* no separator: left part = entire *sv, right = empty */
        SV out = sv_new();
        return out;
    }
    SV out = sv_from_parts(sv->data + pos + 1, sv->count - pos - 1);
    sv->count = pos;
    if (sv->count == 0)
        sv->data = EMPTY_STR;
    return out;
}

size_t sv_find_char(SV sv, char c)
{
    for (size_t i = 0; i < sv.count; i++)
        if (sv.data[i] == c)
            return i;
    return SIZE_MAX;
}

size_t sv_rfind_char(SV sv, char c)
{
    for (size_t i = sv.count; i > 0; i--)
        if (sv.data[i - 1] == c)
            return i - 1;
    return SIZE_MAX;
}

int sv_consume_char(SV *sv, char c)
{
    if (sv->count == 0 || sv->data[0] != c)
        return 0;
    sv_remove_prefix(sv, 1);
    return 1;
}

int sv_consume(SV *sv, SV prefix)
{
    if (!sv_starts_with(*sv, prefix))
        return 0;
    sv_remove_prefix(sv, prefix.count);
    return 1;
}

// String view functions End ================================================================================================================

// String builder functions Start ================================================================================================================

SV sb_to_sv(const SB *sb)
{
    if (sb == NULL)
        return sv_new();
    SV sv;
    sv.count = sb->count;
    sv.data = (sb->count && sb->items) ? sb->items : EMPTY_STR;
    return sv;
}

/* SV -> SB deep copy (memory allocation) */
SB sv_to_sb(SV sv)
{
    SB sb = {0};
    /* We choose the capacity: either by default SB_INITIAL_CAPACITY, or under the size of SV + NUL */
    size_t needed = sv.count + 1;
    sb.capacity = (needed > SB_INITIAL_CAPACITY) ? needed : SB_INITIAL_CAPACITY;

    sb.items = (char *)FDS_MALLOC(sb.capacity);
    if (sb.items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }

    sb.count = sv.count;
    if (sv.count > 0 && sv.data != NULL)
    {
        memcpy(sb.items, sv.data, sv.count);
    }
    sb.items[sb.count] = '\0';
    return sb;
}
static void sb_grow(SB *sb, size_t size)
{
    /* Checking for overflow when calculating the required volume */
    size_t needed;
    if (!safe_add(sb->count, size, &needed) || !safe_add(needed, 1, &needed))
    {
        fds_log(FERROR, "Requested size too large");
    }

    /* If the current capacity is sufficient, we do nothing */
    if (needed <= sb->capacity)
        return;

    /* We calculate the new capacity */
    size_t new_cap = sb->capacity;
    if (new_cap == 0)
    {
        new_cap = SB_INITIAL_CAPACITY;
    }
    while (new_cap < needed)
    {
        /* Multiplication overflow check */
        if (new_cap > SIZE_MAX / 2)
        {
            new_cap = needed; /* have reached the maximum, we just take the right one */
            break;
        }
        new_cap *= 2;
    }

    /* Allocation of memory */
    char *new_items = (char *)FDS_REALLOC(sb->items, new_cap);
    if (new_items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    sb->items = new_items;
    sb->capacity = new_cap;
}

void sb_append_sv(SB *sb, SV sv)
{
    sb_grow(sb, sv.count);
    memcpy(sb->items + sb->count, sv.data, sv.count);
    sb->count += sv.count;
    sb->items[sb->count] = '\0';
}

SB sb_from_cstr(const char *str)
{
    FDS_ASSERT(str != NULL,"std != NULL");
    SB sb = {0};
    size_t len = strlen(str);
    sb.count = len;
    sb.capacity = len + 1;
    sb.items = (char *)FDS_MALLOC(sb.capacity);
    if (sb.items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    memcpy(sb.items, str, len + 1);
    return sb;
}

SB sb_new(void)
{
    SB sb = {0};
    sb.capacity = SB_INITIAL_CAPACITY;
    sb.items = (char *)FDS_MALLOC(sb.capacity);
    if (sb.items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    sb.items[0] = '\0';
    return sb;
}

void sb_free(SB *sb)
{
    FDS_FREE(sb->items);
    sb->items = NULL;
    sb->count = 0;
    sb->capacity = 0;
}

void sb_append(SB *sb, const char *str)
{
    FDS_ASSERT(str != NULL, "str != NULL");
    size_t len = strlen(str);
    sb_grow(sb, len);
    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_append_n(SB *sb, const char *str, size_t len)
{
    FDS_ASSERT(str != NULL || len == 0, "str != NULL || len == 0");
    if (len == 0)
        return;
    sb_grow(sb, len);
    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_reserve(SB *sb, size_t capacity)
{
    if (capacity <= sb->capacity)
        return;
    char *new_items = (char *)FDS_REALLOC(sb->items, capacity);
    if (new_items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    sb->items = new_items;
    sb->capacity = capacity;
}

void sb_reserve_extra(SB *sb, size_t extra)
{
    if (extra == 0)
        return;
    size_t new_cap;
    if (!safe_add(sb->capacity, extra, &new_cap))
    {
        fds_log(FFATAL, "Capacity overflow");
    }
    char *new_items = (char *)FDS_REALLOC(sb->items, new_cap);
    if (new_items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    sb->items = new_items;
    sb->capacity = new_cap;
}

char *sb_to_cstr(SB *sb)
{
    sb->items[sb->count] = '\0';
    return sb->items;
}

void sb_append_null(SB *sb)
{
    sb_grow(sb, 0);
    sb->items[sb->count] = '\0';
}

void sb_append_char(SB *sb, char c)
{
    sb_grow(sb, 1);
    sb->items[sb->count++] = c;
    sb->items[sb->count] = '\0';
}

SB sb_clone(const SB *sb)
{
    SB copy = {0};
    copy.count = sb->count;
    copy.capacity = sb->count + 1;
    copy.items = (char *)FDS_MALLOC(copy.capacity);
    if (copy.items == NULL)
    {
        fds_log(FFATAL, "Out of memory");
    }
    memcpy(copy.items, sb->items, sb->count + 1);
    return copy;
}

void sb_appendf(SB *sb, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (len <= 0)
    {
        va_end(args);
        return;
    }
    sb_grow(sb, (size_t)len);
    vsnprintf(sb->items + sb->count, (size_t)len + 1, fmt, args);
    sb->count += (size_t)len;
    va_end(args);
}

// String builder functions End ================================================================================================================

// Dynamic arrays macros Start ================================================================================================================

#define da_realloc(da, new_cap)                                            \
    do                                                                     \
    {                                                                      \
        void *_p = FDS_REALLOC((da)->items, (new_cap) * sizeof(*(da)->items)); \
        if (!_p)                                                           \
        {                                                                  \
            fds_log(FFATAL, "Out of memory");                              \
        }                                                                  \
        (da)->items = (typeof((da)->items))_p;                              \
        (da)->capacity = (new_cap);                                        \
    } while (0)

// ============================================================
// Iterative macros
// ============================================================

// Direct pass: safe even for an empty array (the loop will not execute)
#define da_foreach(Type, it, da) \
    for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

// Reverse pass: to avoid an invalid pointer when count == 0,
// wrapped in if ((da)->count > 0). The initial iterator is then computed
// only if there are elements.
#define da_foreach_reverse(Type, it, da) \
    if ((da)->count > 0)                 \
        for (Type *it = (da)->items + (da)->count - 1; it >= (da)->items; --it)

// ============================================================
// Basic array operations
// ============================================================

// Free all memory and reset fields
#define da_free(da)         \
    do                      \
    {                       \
        FDS_FREE((da)->items);  \
        (da)->items = NULL; \
        (da)->count = 0;    \
        (da)->capacity = 0; \
    } while (0)

// Clear the array without reducing capacity
#define da_clear(da)     \
    do                   \
    {                    \
        (da)->count = 0; \
    } while (0)

#define da_empty(da) ((da)->count == 0)

// Get the last item; there must be at least one element
#define da_back(da) \
    (assert((da)->count > 0), (da)->items[(da)->count - 1])

// Get the first element; there must be at least one element
#define da_front(da) \
    (assert((da)->count > 0), (da)->items[0])

// Reserve a capacity of at least cap; does not reduce capacity if cap < capacity
#define da_reserve(da, cap)          \
    do                               \
    {                                \
        if ((cap) > (da)->capacity)  \
        {                            \
            da_realloc((da), (cap)); \
        }                            \
    } while (0)

// Delete the last element and return it; there must be at least one element
#define da_pop(da) \
    (assert((da)->count > 0), (da)->items[--(da)->count])

// Insert value at index position (0 <= index <= count)
#define da_insert(da, index, value)                              \
    do                                                           \
    {                                                            \
        assert((index) <= (da)->count);                          \
        da_push((da), (value));                                  \
        memmove(                                                 \
            &(da)->items[(index) + 1],                           \
            &(da)->items[(index)],                               \
            ((da)->count - (index) - 1) * sizeof(*(da)->items)); \
        (da)->items[(index)] = (value);                          \
    } while (0)

// Delete element at position index (0 <= index < count)
#define da_remove(da, index)                                     \
    do                                                           \
    {                                                            \
        assert((index) < (da)->count);                           \
        memmove(                                                 \
            &(da)->items[(index)],                               \
            &(da)->items[(index) + 1],                           \
            ((da)->count - (index) - 1) * sizeof(*(da)->items)); \
        --(da)->count;                                           \
    } while (0)

// Quick delete: replace the index element with the last one and decrement count
#define da_swap_remove(da, index)                          \
    do                                                     \
    {                                                      \
        assert((index) < (da)->count);                     \
        (da)->items[index] = (da)->items[(da)->count - 1]; \
        --(da)->count;                                     \
    } while (0)

// Deep copy: dst gets a copy of src's data
#define da_clone(dst, src)                            \
    do                                                \
    {                                                 \
        da_reserve((dst), (src)->count);              \
        memcpy((dst)->items,                          \
               (src)->items,                          \
               (src)->count * sizeof(*(src)->items)); \
        (dst)->count = (src)->count;                  \
    } while (0)

// Add cnt elements from the ptr array
#define da_append(da, ptr, cnt)                \
    do                                         \
    {                                          \
        da_reserve((da), (da)->count + (cnt)); \
        memcpy((da)->items + (da)->count,      \
               (ptr),                          \
               (cnt) * sizeof(*(da)->items));  \
        (da)->count += (cnt);                  \
    } while (0)

// Add one element to the end (with auto-expansion)
#define da_push(da, value)                                            \
    do                                                                \
    {                                                                 \
        if ((da)->count >= (da)->capacity)                            \
        {                                                             \
            size_t new_cap = (da)->capacity ? (da)->capacity * 2 : 4; \
            da_realloc((da), new_cap);                                \
        }                                                             \
        (da)->items[(da)->count++] = (value);                         \
    } while (0)

// Guarantee that you can add extra elements without redistribution
#define da_grow(da, extra)                                        \
    do                                                            \
    {                                                             \
        size_t need = (da)->count + (extra);                      \
        if (need > (da)->capacity)                                \
        {                                                         \
            size_t cap = (da)->capacity ? (da)->capacity * 2 : 4; \
            while (cap < need)                                    \
                cap *= 2;                                         \
            da_realloc((da), cap);                                \
        }                                                         \
    } while (0)

// Change the size of the array. New elements (if the size is increased) are filled with zeros.
// This fixes a previous bug with uninitialized data.
#define da_resize(da, size)                                                                \
    do                                                                                     \
    {                                                                                      \
        size_t _old_cnt = (da)->count;                                                     \
        da_reserve((da), (size));                                                          \
        (da)->count = (size);                                                              \
        if ((size) > _old_cnt)                                                             \
        {                                                                                  \
            memset((da)->items + _old_cnt, 0, ((size) - _old_cnt) * sizeof(*(da)->items)); \
        }                                                                                  \
    } while (0)

// Dynamic arrays macros End ================================================================================================================

// Flag parser fuctions Start ================================================================================================================

static inline void flagset_bool(FlagSet *fs, bool *ptr, const char *name, bool defval, const char *usage)
{
    flagset_var(fs, FLAG_BOOL, ptr, name, defval ? "true" : "false", usage);
}
static inline void flagset_string(FlagSet *fs, char **ptr, const char *name, const char *defval, const char *usage)
{
    flagset_var(fs, FLAG_STRING, ptr, name, defval, usage);
}
static inline void flagset_int(FlagSet *fs, int *ptr, const char *name, int defval, const char *usage)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", defval);
    flagset_var(fs, FLAG_INT, ptr, name, buf, usage);
}
static inline void flagset_float(FlagSet *fs, float *ptr, const char *name, float defval, const char *usage)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", (double)defval);
    flagset_var(fs, FLAG_FLOAT, ptr, name, buf, usage);
}
static inline void flagset_string_list(FlagSet *fs, void *list, const char *name, const char *usage)
{
    flagset_var(fs, FLAG_STRING_LIST, list, name, "", usage);
}
static inline void flagset_int_list(FlagSet *fs, void *list, const char *name, const char *usage)
{
    flagset_var(fs, FLAG_INT_LIST, list, name, "", usage);
}

static inline void flagset_float_list(FlagSet *fs, void *list, const char *name, const char *usage)
{
    flagset_var(fs, FLAG_FLOAT_LIST, list, name, "", usage);
}

static bool is_integer(SV sv)
{
    if (sv.count == 0)
        return false;
    size_t i = 0;
    if (sv.data[0] == '-' || sv.data[0] == '+')
        i++;
    if (i == sv.count)
        return false;
    for (; i < sv.count; i++)
        if (!isdigit((unsigned char)sv.data[i]))
            return false;
    return true;
}

static int sv_to_int(SV sv)
{
    char *cstr = sv_to_cstr(sv);
    long val = strtol(cstr, NULL, 10);
    FDS_FREE(cstr);
    return (int)val;
}

static float sv_to_float(SV sv)
{
    char *cstr = sv_to_cstr(sv);
    char *end;
    float val = strtof(cstr, &end);

    bool is_invalid = (end == cstr || *end != '\0');
    FDS_FREE(cstr);

    if (is_invalid)
    {
        fds_log(FERROR, "invalid float value: " SV_FMT, SV_ARGS(sv));
    }
    return val;
}

static Flag *find_flag(FlagSet *fs, SV name)
{
    for (size_t i = 0; i < fs->count; i++)
        if (sv_eq_cstr(name, fs->items[i].name))
            return &fs->items[i];
    return NULL;
}

/* -------------------------------------------------------------------
 *  Set a single flag value
 * ------------------------------------------------------------------- */

static void set_flag_value(Flag *f, SV val)
{
    switch (f->type)
    {
    case FLAG_BOOL:
        if (val.count == 0 || sv_eq_cstr(val, "true"))
            *(bool *)f->ptr = true;
        else if (sv_eq_cstr(val, "false"))
            *(bool *)f->ptr = false;
        else
        {
            fds_log(FERROR, "invalid boolean value for -%s: " SV_FMT, f->name, SV_ARGS(val));
        }
        break;
    case FLAG_STRING:
    {
        char **str = (char **)f->ptr;
        // We always release the old value, because it is either selected as the default (strdup),
        // or as a previous argument (sv_to_cstr). Both live in a heap.
        FDS_FREE(*str);
        *str = sv_to_cstr(val);
        break;
    }
    case FLAG_INT:
        if (!is_integer(val))
        {
            fds_log(FERROR, "invalid integer value for -%s: " SV_FMT, f->name, SV_ARGS(val));
        }
        *(int *)f->ptr = sv_to_int(val);
        break;
    case FLAG_FLOAT:
        *(float *)f->ptr = sv_to_float(val);
        break;
    case FLAG_STRING_LIST:
        da_push((da_SV *)f->ptr, val);
        break;
    case FLAG_INT_LIST:
        if (!is_integer(val))
        {
            fds_log(FERROR, "invalid integer value for -%s: " SV_FMT, f->name, SV_ARGS(val));
        }
        da_push((da_int *)f->ptr, sv_to_int(val));
        break;
    case FLAG_FLOAT_LIST:
        da_push((da_float *)f->ptr, sv_to_float(val));
        break;
    }
    f->set = true;
}

/* -------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------- */
FlagSet *flagset_new(void)
{
    FlagSet *fs = (FlagSet *)FDS_CALLOC(1, sizeof(*fs));
    if (!fs)
    {
        fds_log(FFATAL, "Out of memory");
    }
    return fs;
}

void flagset_free(FlagSet *fs)
{
    if (!fs)
        return;

    for (size_t i = 0; i < fs->count; i++)
    {
        // We free memory for C-strings, because they are guaranteed to lie in the heap
        if (fs->items[i].type == FLAG_STRING)
        {
            FDS_FREE(*(char **)fs->items[i].ptr);
        }

        // We release list buffers (dynamic arrays)
        switch (fs->items[i].type)
        {
        case FLAG_STRING_LIST:
        {
            da_SV *list = (da_SV *)fs->items[i].ptr;
            FDS_FREE(list->items);
            list->items = NULL;
            list->count = list->capacity = 0;
            break;
        }
        case FLAG_INT_LIST:
        {
            da_int *list = (da_int *)fs->items[i].ptr;
            FDS_FREE(list->items);
            list->items = NULL;
            list->count = list->capacity = 0;
            break;
        }
        case FLAG_FLOAT_LIST:
        {
            da_float *list = (da_float *)fs->items[i].ptr;
            FDS_FREE(list->items);
            list->items = NULL;
            list->count = list->capacity = 0;
            break;
        }
        default:
            break;
        }

        FDS_FREE(fs->items[i].name);
        FDS_FREE(fs->items[i].defval);
        FDS_FREE(fs->items[i].usage);
    }

    // We release the flag array itself
    if (fs->items)
    {
        FDS_FREE(fs->items);
    }

    // If args was allocated dynamically
    if (fs->args)
    {
        FDS_FREE(fs->args);
    }

    // We release the structure
    FDS_FREE(fs);
}

void flagset_var(FlagSet *fs, FlagType type, void *ptr, const char *name,
                 const char *defval, const char *usage)
{
    Flag f;
    size_t name_len = strlen(name) + 1;
    f.name = (char *)FDS_MALLOC(name_len);
    f.defval = strdup(defval);
    f.usage = strdup(usage);
    if (!f.name || !f.defval || !f.usage)
    {
        FDS_FREE(f.name);
        FDS_FREE(f.defval);
        FDS_FREE(f.usage);
        fds_log(FFATAL, "Out of memory");
    }
    memcpy((void *)f.name, name, name_len);
    f.type = type;
    f.ptr = ptr;
    f.set = false;
    f.required = false;

    // For non-list types, set the default value
    if (type == FLAG_STRING)
    {
        // We explicitly allocate the default value in the heap so that set remains false
        char **str = (char **)f.ptr;
        *str = strdup(f.defval);
    }
    else if (type != FLAG_STRING_LIST && type != FLAG_INT_LIST && type != FLAG_FLOAT_LIST)
    {
        SV def_sv = sv_from_cstr(f.defval);
        set_flag_value(&f, def_sv);
        // For bool, int, float, after setting the default, reset set,
        // so that it can be determined whether the flag was explicitly passed.
        f.set = false;
    }

    da_push(fs, f);
}

void flagset_required(FlagSet *fs)
{
    if (fs->count > 0)
    {
        fs->items[fs->count - 1].required = true;
    }
}

void flagset_parse(FlagSet *fs, int argc, char **argv)
{
    fs->name = argv[0];

    size_t args_max = argc;
    fs->args = (SV *)FDS_MALLOC(args_max * sizeof(SV));
    if (!fs->args)
    {
        fds_log(FFATAL, "Out of memory");
    }
    fs->args_cap = args_max;
    fs->args_count = 0;

    int i = 1;
    bool end_of_flags = false;

    while (i < argc)
    {
        SV arg = sv_from_cstr(argv[i]);

        if (!end_of_flags && sv_eq_cstr(arg, "--"))
        {
            end_of_flags = true;
            i++;
            continue;
        }

        if (!end_of_flags && sv_starts_with_char(arg, '-') && arg.count > 1)
        {
            SV rest = arg;

            while (sv_starts_with_char(rest, '-'))
            {
                sv_consume_char(&rest, '-');
            }

            if (sv_eq_cstr(rest, "help") || sv_eq_cstr(rest, "h"))
            {
                flagset_usage(fs);
                exit(0);
            }

            SV name_sv, value_sv = sv_new();
            size_t eq_pos = sv_find_char(rest, '=');
            if (eq_pos != SV_NPOS)
            {
                name_sv = sv_from_parts(rest.data, eq_pos);
                value_sv = sv_from_parts(rest.data + eq_pos + 1, rest.count - eq_pos - 1);
            }
            else
            {
                name_sv = rest;
                value_sv = sv_new();
            }

            Flag *f = find_flag(fs, name_sv);
            if (!f)
            {
                flagset_usage(fs);
                fds_log(FERROR, "flag provided but not defined: -%.*s", (int)name_sv.count, name_sv.data);
            }

            if (f->type == FLAG_BOOL && eq_pos == SV_NPOS)
            {
                set_flag_value(f, sv_new()); // true
            }
            else
            {
                if (eq_pos == SV_NPOS)
                {
                    i++;
                    if (i >= argc)
                    {
                        fds_log(FERROR, "flag needs an argument: -%s", f->name);
                    }
                    value_sv = sv_from_cstr(argv[i]);
                }
                set_flag_value(f, value_sv);
            }
        }
        else
        {
            if (fs->args_count >= fs->args_cap)
            {
                size_t new_cap = fs->args_cap * 2;
                SV *new_args = (SV *)FDS_REALLOC(fs->args, new_cap * sizeof(SV));
                if (!new_args)
                {
                    fds_log(FFATAL, "Out of memory");
                }
                fs->args = new_args;
                fs->args_cap = new_cap;
            }
            fs->args[fs->args_count++] = arg;
        }
        i++;
    }

    // Checking mandatory flags
    for (size_t j = 0; j < fs->count; j++)
    {
        Flag *f = &fs->items[j];
        if (f->required && !f->set)
        {
            flagset_usage(fs);
            fds_log(FERROR, "Error: required flag -%s not provided", f->name);
        }
    }
}

size_t flagset_narg(FlagSet *fs)
{
    return fs->args_count;
}

SV flagset_arg(FlagSet *fs, size_t i)
{
    if (i >= fs->args_count)
        return sv_new();
    return fs->args[i];
}

void flagset_usage(FlagSet *fs)
{
    if (fs->usage_func)
    {
        fs->usage_func();
        return;
    }

    printf("Usage: %s [options] ...\n", fs->name ? fs->name : "program");
    printf("Options:\n");
    for (size_t i = 0; i < fs->count; i++)
    {
        Flag *f = &fs->items[i];
        printf("  -%s", f->name);
        if (f->type == FLAG_STRING || f->type == FLAG_INT || f->type == FLAG_FLOAT)
            printf(" <value>");
        else if (f->type == FLAG_STRING_LIST || f->type == FLAG_INT_LIST || f->type == FLAG_FLOAT_LIST)
            printf(" <value> (repeatable)");
        if (f->required)
            printf(" [required]");
        printf("\n\t%s (default: %s)\n", f->usage, f->defval);
    }
    printf("  -h, --help\n\tshow this help message\n");
}

// Flag parser fuctions End ================================================================================================================

// Compression fuctions Start ================================================================================================================
FdsCompressStatus fds_ext_compress_lz(FdsBytesView input, FdsBytesBuilder *out_builder)
{
    if (!out_builder)
        return FDS_CMP_ERROR;
    if (input.size > 0 && !input.data)
        return FDS_CMP_ERROR;
    if (input.size > UINT32_MAX)
        return FDS_CMP_ERROR;

    // Примусово очищуємо білдер, щоб уникнути проблеми "+17 байт" при повторному використанні
    out_builder->size = 0;
    size_t start_pos = 0;

    if (input.size == 0)
    {
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
        fds_bb_append_byte(out_builder, FDS_LZ_MODE_RAW);
        fds_bb_append_byte(out_builder, 0x00);
        fds_bb_append_u32_le(out_builder, 0);
        return FDS_CMP_STORED;
    }

    // 1. Заголовок
    fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
    fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
    fds_bb_append_byte(out_builder, FDS_LZ_MODE_LZSS);
    fds_bb_append_byte(out_builder, 0x00);
    fds_bb_append_u32_le(out_builder, (uint32_t)input.size);

    size_t header_size = 8;

    // Ланцюжки хешів для глибокого пошуку (забезпечує максимальне стиснення)
    int32_t hash_head[FDS_LZ_HASH_SIZE];
    int32_t hash_prev[FDS_LZ_WINDOW_SIZE];
    for (int i = 0; i < FDS_LZ_HASH_SIZE; ++i)
        hash_head[i] = -1;

    size_t pos = 0;

    // 2. Стиснення LZSS
    while (pos < input.size)
    {
        size_t flags_offset = out_builder->size;
        fds_bb_append_byte(out_builder, 0x00);
        uint8_t flags = 0;

        for (int bit = 0; bit < 8 && pos < input.size; ++bit)
        {
            size_t match_len = 0;
            size_t match_dist = 0;

            if (pos + FDS_LZ_MIN_MATCH <= input.size)
            {
                uint32_t h = ((uint32_t)input.data[pos] * 251u) ^
                             ((uint32_t)input.data[pos + 1] * 509u) ^
                             (uint32_t)input.data[pos + 2];
                h &= (FDS_LZ_HASH_SIZE - 1);

                int32_t candidate = hash_head[h];
                int limit = 256; // Шукаємо до 256 вузлів вглиб історії

                while (candidate >= 0 && (pos - candidate) <= FDS_LZ_WINDOW_SIZE && limit-- > 0)
                {
                    size_t dist = pos - candidate;
                    size_t max_len = input.size - pos;
                    if (max_len > FDS_LZ_MAX_MATCH)
                        max_len = FDS_LZ_MAX_MATCH;

                    size_t len = 0;
                    while (len < max_len && input.data[candidate + len] == input.data[pos + len])
                    {
                        len++;
                    }

                    if (len > match_len)
                    {
                        match_len = len;
                        match_dist = dist;
                        if (match_len == FDS_LZ_MAX_MATCH)
                            break; // Знайшли ідеал - виходимо
                    }
                    candidate = hash_prev[candidate % FDS_LZ_WINDOW_SIZE];
                }
            }

            if (match_len >= FDS_LZ_MIN_MATCH)
            {
                flags |= (uint8_t)(1 << bit);
                uint16_t dist_enc = (uint16_t)(match_dist - 1);
                uint16_t len_enc = (uint16_t)(match_len - FDS_LZ_MIN_MATCH);
                uint16_t token = (dist_enc << 4) | (len_enc & 0x0F);

                fds_bb_append_u16_le(out_builder, token);

                // Записуємо пропущені байти в словник
                for (size_t k = 0; k < match_len; ++k)
                {
                    if (pos + k + FDS_LZ_MIN_MATCH <= input.size)
                    {
                        uint32_t h = ((uint32_t)input.data[pos + k] * 251u) ^
                                     ((uint32_t)input.data[pos + k + 1] * 509u) ^
                                     (uint32_t)input.data[pos + k + 2];
                        h &= (FDS_LZ_HASH_SIZE - 1);
                        hash_prev[(pos + k) % FDS_LZ_WINDOW_SIZE] = hash_head[h];
                        hash_head[h] = (int32_t)(pos + k);
                    }
                }
                pos += match_len;
            }
            else
            {
                if (pos + FDS_LZ_MIN_MATCH <= input.size)
                {
                    uint32_t h = ((uint32_t)input.data[pos] * 251u) ^
                                 ((uint32_t)input.data[pos + 1] * 509u) ^
                                 (uint32_t)input.data[pos + 2];
                    h &= (FDS_LZ_HASH_SIZE - 1);
                    hash_prev[pos % FDS_LZ_WINDOW_SIZE] = hash_head[h];
                    hash_head[h] = (int32_t)pos;
                }
                fds_bb_append_byte(out_builder, input.data[pos++]);
            }
        }

        out_builder->data[flags_offset] = flags;

        // Ранній вихід, якщо стиснення неефективне
        if ((out_builder->size - start_pos) >= (input.size + header_size))
        {
            break;
        }
    }

    size_t total_compressed_size = out_builder->size - start_pos;

    // 3. Fallback (збереження RAW)
    if (total_compressed_size >= (input.size + header_size))
    {
        out_builder->size = start_pos;

        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_0);
        fds_bb_append_byte(out_builder, FDS_LZ_MAGIC_1);
        fds_bb_append_byte(out_builder, FDS_LZ_MODE_RAW);
        fds_bb_append_byte(out_builder, 0x00);
        fds_bb_append_u32_le(out_builder, (uint32_t)input.size);
        fds_bb_append(out_builder, input.data, input.size);

        return FDS_CMP_STORED;
    }

    return FDS_CMP_COMPRESSED;
}

bool fds_ext_decompress_lz(FdsBytesView input, FdsBytesBuilder *out_builder)
{
    if (!out_builder)
        return false;

    // Примусово обнуляємо для точного співпадіння розмірів з оригіналом (без append)
    out_builder->size = 0;
    size_t start_size = 0;

#define FDS_DECOMP_FAIL()               \
    do                                  \
    {                                   \
        out_builder->size = start_size; \
        return false;                   \
    } while (0)

    // 1. Читання заголовка
    uint8_t m0, m1, mode, reserved;
    uint32_t orig_size;

    if (!fds_bv_pop_byte(&input, &m0) || m0 != FDS_LZ_MAGIC_0)
        FDS_DECOMP_FAIL();
    if (!fds_bv_pop_byte(&input, &m1) || m1 != FDS_LZ_MAGIC_1)
        FDS_DECOMP_FAIL();
    if (!fds_bv_pop_byte(&input, &mode))
        FDS_DECOMP_FAIL();
    if (!fds_bv_pop_byte(&input, &reserved))
        FDS_DECOMP_FAIL();
    if (!fds_bv_read_u32_le(&input, &orig_size))
        FDS_DECOMP_FAIL();

    if (reserved != 0)
        FDS_DECOMP_FAIL();

    size_t target_size = start_size + orig_size;

    // 2. Декомпресія RAW
    if (mode == FDS_LZ_MODE_RAW)
    {
        if (input.size != orig_size)
            FDS_DECOMP_FAIL();

        fds_bb_reserve(out_builder, orig_size);
        fds_bb_append(out_builder, input.data, orig_size);
        return true;
    }

    // 3. Декомпресія LZSS
    if (mode == FDS_LZ_MODE_LZSS)
    {
        fds_bb_reserve(out_builder, orig_size);

        while (input.size > 0 && out_builder->size < target_size)
        {
            uint8_t flags;
            if (!fds_bv_pop_byte(&input, &flags))
                FDS_DECOMP_FAIL();

            for (int bit = 0; bit < 8; ++bit)
            {
                if (out_builder->size >= target_size)
                    break;

                if ((flags & (1 << bit)) == 0)
                {
                    uint8_t byte;
                    if (!fds_bv_pop_byte(&input, &byte))
                        FDS_DECOMP_FAIL();
                    fds_bb_append_byte(out_builder, byte);
                }
                else
                {
                    uint16_t token;
                    if (!fds_bv_read_u16_le(&input, &token))
                        FDS_DECOMP_FAIL();

                    size_t dist = (token >> 4) + 1;
                    size_t len = (token & 0x0F) + FDS_LZ_MIN_MATCH;

                    if (len > target_size - out_builder->size)
                        FDS_DECOMP_FAIL();
                    if (dist > (out_builder->size - start_size))
                        FDS_DECOMP_FAIL();

                    size_t src_start = out_builder->size - dist;
                    fds_bb_reserve(out_builder, len);
                    for (size_t i = 0; i < len; ++i)
                    {
                        out_builder->data[out_builder->size++] = out_builder->data[src_start + i];
                    }
                }
            }
        }

        if (out_builder->size != target_size || input.size > 0)
        {
            FDS_DECOMP_FAIL();
        }
        return true;
    }

    FDS_DECOMP_FAIL();

#undef FDS_DECOMP_FAIL
}
// Compression fuctions End ================================================================================================================



// Fixed arena fuctions Start ================================================================================================================

FixedArena fixed_arena_create(size_t capacity)
{
    FixedArena arena = {0};
    arena.data = (unsigned char *)FDS_MALLOC(capacity);
    FDS_ASSERT(arena.data, "arena.data"); // malloc returned NULL – programmer error (out of memory)
    arena.capacity = capacity;
    return arena;
}

void fixed_arena_free(FixedArena *arena)
{
    FDS_ASSERT(arena, "arena");
    FDS_FREE(arena->data);
    arena->data = NULL;
    arena->offset = 0;
    arena->capacity = 0;
}

void fixed_arena_reset(FixedArena *arena)
{
    FDS_ASSERT(arena, "arena");
    arena->offset = 0;
}

// status information
size_t fixed_arena_used(const FixedArena *arena)
{
    FDS_ASSERT(arena, "arena");
    return arena->offset;
}

size_t fixed_arena_available(const FixedArena *arena)
{
    FDS_ASSERT(arena, "arena");
    return arena->capacity - arena->offset;
}

int fixed_arena_is_empty(const FixedArena *arena)
{
    FDS_ASSERT(arena, "arena");
    return arena->offset == 0;
}

int fixed_arena_contains(const FixedArena *arena, const void *ptr)
{
    FDS_ASSERT(arena, "arena");
    const unsigned char *p = (const unsigned char *)ptr;
    return (p >= arena->data) && (p < arena->data + arena->offset);
}

FixedArenaMark fixed_arena_mark(const FixedArena *arena)
{
    FDS_ASSERT(arena, "arena");
    return arena->offset;
}

void fixed_arena_restore(FixedArena *arena, FixedArenaMark mark)
{
    FDS_ASSERT(arena, "arena");
    FDS_ASSERT(mark <= arena->offset, "mark <= arena->offset");
    arena->offset = mark;
}

// Main allocators
void *fixed_arena_alloc_align(FixedArena *arena, size_t size, size_t alignment)
{
    FDS_ASSERT(arena, "arena");
    FDS_ASSERT(size > 0, "size > 0");
    FDS_ASSERT(alignment > 0, "alignment > 0");
    FDS_ASSERT((alignment & (alignment - 1)) == 0, "(alignment & (alignment - 1)) == 0"); // power of two

    uintptr_t ptr = (uintptr_t)(arena->data + arena->offset);
    uintptr_t aligned = (ptr + alignment - 1) & ~(uintptr_t)(alignment - 1);
    size_t padding = aligned - ptr;

    // Protection against overflow
    if (padding > arena->capacity - arena->offset ||
        size > arena->capacity - arena->offset - padding)
    {
        fds_log(FFATAL, "FixedArena out of memory (capacity %zu, needed %zu)",
                arena->capacity, arena->offset + padding + size);
    }

    arena->offset += padding;
    void *result = arena->data + arena->offset;
    arena->offset += size;
    return result;
}

void *fixed_arena_alloc(FixedArena *arena, size_t size)
{
    return fixed_arena_alloc_align(arena, size, sizeof(void *));
}

void *fixed_arena_alloc_zero(FixedArena *arena, size_t size)
{
    void *ptr = fixed_arena_alloc(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void *fixed_arena_alloc_array(FixedArena *arena, size_t count, size_t element_size)
{
    // Multiplication overflow check
    size_t total;
    if (count > 0 && element_size > SIZE_MAX / count)
    {
        fds_log(FFATAL, "Array size overflow");
    }
    total = count * element_size;
    return fixed_arena_alloc(arena, total);
}

// data copying
void *fixed_arena_memdup(FixedArena *arena, const void *src, size_t size)
{
    void *dst = fixed_arena_alloc(arena, size);
    memcpy(dst, src, size);
    return dst;
}

char *fixed_arena_strndup(FixedArena *arena, const char *str, size_t len)
{
    char *dst = (char *)fixed_arena_alloc(arena, len + 1);
    memcpy(dst, str, len);
    dst[len] = '\0';
    return dst;
}

char *fixed_arena_strdup(FixedArena *arena, const char *str)
{
    return fixed_arena_strndup(arena, str, strlen(str));
}

// Fixed arena fuctions End ================================================================================================================

// Temp arena fuctions Start ================================================================================================================

FixedArena *temp_arena_get(void)
{
    if (!temp_arena_initialized)
    {
        temp_arena_instance = fixed_arena_create(TEMP_ARENA_SIZE);
        temp_arena_initialized = 1;
    }
    return &temp_arena_instance;
}
static inline void _temp_arena_thread_cleanup(int *dummy)
{
    (void)dummy; // Avoid the unused variable warning
    temp_arena_destroy();
}

void temp_arena_reset(void)
{
    if (temp_arena_initialized)
    {
        fixed_arena_reset(&temp_arena_instance);
    }
}

void temp_arena_destroy(void)
{
    if (temp_arena_initialized)
    {
        fixed_arena_free(&temp_arena_instance);
        temp_arena_initialized = 0;
    }
}

char *temp_arena_sprintf(FixedArena *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    // We find out the length
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0)
        return NULL;

    size_t size = (size_t)needed + 1; // +1 for '\0'
    char *buf = (char *)fixed_arena_alloc(arena, size);
    if (!buf)
        return NULL; // If the arena cannot return NULL, then abort

    vsnprintf(buf, size, fmt, args);
    va_end(args);
    return buf;
}
static inline void temp_arena_restore_mark(FixedArenaMark *mark)
{
    if (mark)
        fixed_arena_restore(temp_arena_get(), *mark);
}

// Temp arena fuctions End ================================================================================================================

// INI parse fuctions Start ================================================================================================================

IniConfig ini_parse_sv(SV content)
{
    IniConfig config = {0};

    // We allocate an arena for reading the file. 2 MB for INI is enough with a margin
    config.arena = fixed_arena_create(content.count + 2 * MB);

    // We create a global section (for keys that go to the first [Section])
    IniSection global_sec = {0};
    global_sec.name = sv_from_cstr("GLOBAL");
    da_push(&config, global_sec);

    SV line;
    // sv_next_line independently splits text by '\n' and discards '\r'
    while (sv_next_line(&content, &line) == 0)
    {
        sv_trim(&line); // We remove spaces from both sides

        // We skip empty lines and comments (; or #)
        if (line.count == 0 || line.data[0] == ';' || line.data[0] == '#')
        {
            continue;
        }

        // Check if this is the section title: [SectionName]
        if (line.data[0] == '[' && line.data[line.count - 1] == ']')
        {
            SV sec_name = line;
            sv_remove_prefix(&sec_name, 1);
            sv_remove_suffix(&sec_name, 1);
            sv_trim(&sec_name);

            IniSection new_sec = {0};
            new_sec.name = sec_name;
            da_push(&config, new_sec);
        }
        // Otherwise, it is a key-value of the type: key = value
        else
        {
            if (sv_find_char(line, '=') != SIZE_MAX)
            {
                // sv_split_left cuts off the left part to '=', and in line leaves the right part
                SV key = sv_split_left(&line, '=');
                SV value = line;

                sv_trim(&key);
                sv_trim(&value);

                IniKV kv = {key, value};

                // We add KV to the current (last added) section
                IniSection *current_sec = &config.items[config.count - 1];
                da_push(current_sec, kv);
            }
        }
    }

    return config;
}

// The main INI parsing function
IniConfig ini_parse(const char *filepath)
{
    IniConfig config = {0};
    size_t file_size = fds_get_file_size(filepath);
    // We allocate an arena for reading the file. 2 MB for INI is enough with a margin
    config.arena = fixed_arena_create(file_size + 2 * MB);

    SV content;
    if (fds_file_read_to_arena(filepath, &config.arena, &content) != 0)
    {
        fds_log(FERROR, "Cannot read a file %s", filepath);
        return config;
    }

    // We create a global section (for keys that go to the first [Section])
    IniSection global_sec = {0};
    global_sec.name = sv_from_cstr("GLOBAL");
    da_push(&config, global_sec);

    SV line;
    // sv_next_line independently splits text by '\n' and discards '\r'
    while (sv_next_line(&content, &line) == 0)
    {
        sv_trim(&line); // We remove spaces from both sides

        // We skip empty lines and comments (; or #)
        if (line.count == 0 || line.data[0] == ';' || line.data[0] == '#')
        {
            continue;
        }

        // Check if this is the section title: [SectionName]
        if (line.data[0] == '[' && line.data[line.count - 1] == ']')
        {
            SV sec_name = line;
            sv_remove_prefix(&sec_name, 1);
            sv_remove_suffix(&sec_name, 1);
            sv_trim(&sec_name);

            IniSection new_sec = {0};
            new_sec.name = sec_name;
            da_push(&config, new_sec);
        }
        // Otherwise, it is a key-value of the type: key = value
        else
        {
            if (sv_find_char(line, '=') != SIZE_MAX)
            {
                // sv_split_left cuts off the left part to '=', and in line leaves the right part
                SV key = sv_split_left(&line, '=');
                SV value = line;

                sv_trim(&key);
                sv_trim(&value);

                IniKV kv = {key, value};

                // We add KV to the current (last added) section
                IniSection *current_sec = &config.items[config.count - 1];
                da_push(current_sec, kv);
            }
        }
    }

    return config;
}

// Memory release function (we use a direct loop) INI
void ini_free(IniConfig *config)
{
    // We release key arrays inside each section
    for (size_t i = 0; i < config->count; i++)
    {
        da_free(&config->items[i]);
    }
    // We release the array of the sections themselves
    da_free(config);
    // We release the arena with the original text
    fixed_arena_free(&config->arena);
}

// Helper function for debugging INI output
void ini_print(const IniConfig *config)
{
    // We use your da_foreach iterator
    da_foreach(IniSection, sec, config)
    {
        // We skip the output of the global section if it is empty
        if (sv_eq_cstr(sec->name, "GLOBAL") && sec->count == 0)
            continue;

        printf("[" SV_FMT "]\n", SV_ARGS(sec->name));

        da_foreach(IniKV, kv, sec)
        {
            printf("  " SV_FMT " = " SV_FMT "\n", SV_ARGS(kv->key), SV_ARGS(kv->value));
        }
    }
}

// Get value from config return  SV
SV ini_get(const IniConfig *config, const char *section, const char *key)
{
    SV target_sec = sv_from_cstr(section);
    SV target_key = sv_from_cstr(key);

    da_foreach(IniSection, sec, config)
    {
        if (sv_eq(sec->name, target_sec))
        {
            da_foreach(IniKV, kv, sec)
            {
                if (sv_eq(kv->key, target_key))
                {
                    return kv->value;
                }
            }
        }
    }

    // If not found, we return an empty SV
    return sv_new();
}
// Return vulue by sv but if not found value in config return default  INI
SV ini_get_sv(const IniConfig *config, const char *section, const char *key, const char *default_val)
{
    SV target_sec = sv_from_cstr(section);
    SV target_key = sv_from_cstr(key);

    da_foreach(IniSection, sec, config)
    {
        if (sv_eq(sec->name, target_sec))
        {
            da_foreach(IniKV, kv, sec)
            {
                if (sv_eq(kv->key, target_key))
                {
                    return kv->value;
                }
            }
        }
    }

    // If not found - return the default value (or an empty SV)
    return default_val ? sv_from_cstr(default_val) : sv_new();
}

// 2. Obtaining an integer (int) INI
int ini_get_int(const IniConfig *config, const char *section, const char *key, int default_val)
{
    SV val = ini_get_sv(config, section, key, NULL);
    if (val.count == 0)
        return default_val;

    // SV does not have a null terminator, so we copy it to a safe local buffer
    char buf[128] = {0};
    size_t len = val.count < sizeof(buf) - 1 ? val.count : sizeof(buf) - 1;
    memcpy(buf, val.data, len);

    return atoi(buf);
}

// 3. Obtaining a number with a floating point (float) INI
float ini_get_float(const IniConfig *config, const char *section, const char *key, float default_val)
{
    SV val = ini_get_sv(config, section, key, NULL);
    if (val.count == 0)
        return default_val;

    char buf[128] = {0};
    size_t len = val.count < sizeof(buf) - 1 ? val.count : sizeof(buf) - 1;
    memcpy(buf, val.data, len);

    return strtof(buf, NULL);
}

// 4. Getting the boolean value (true/1/yes) of the INI
int ini_get_bool(const IniConfig *config, const char *section, const char *key, int default_val)
{
    SV val = ini_get_sv(config, section, key, NULL);
    if (val.count == 0)
        return default_val;

    // Support for various recording options
    if (sv_eq_cstr(val, "1") || sv_eq_cstr(val, "true") || sv_eq_cstr(val, "yes"))
        return 1;
    if (sv_eq_cstr(val, "0") || sv_eq_cstr(val, "false") || sv_eq_cstr(val, "no"))
        return 0;

    return default_val;
}

// 5. Obtaining the C-line (allocated in the temporary arena) of the INI
// Memory will be freed automatically when temp_arena is reset or thread is terminated!
char *ini_get_temp_cstr(const IniConfig *config, const char *section, const char *key, const char *default_val)
{
    SV val = ini_get_sv(config, section, key, default_val);
    if (val.count == 0)
        return NULL;

    // We use your temp_arena for zero-friction allocation
    return fixed_arena_strndup(temp_arena_get(), val.data, val.count);
}

// INI parse fuctions End ================================================================================================================

// Time utils fuctions Start ================================================================================================================
// Повертає монотонний час у секундах з високою точністю (мікро/наносекунди)
double fds_time_now(void)
{
#ifdef _WIN32
    if (fds_g_timer_frequency == 0.0)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        fds_g_timer_frequency = (double)freq.QuadPart;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / fds_g_timer_frequency;
#else
    struct timespec ts;
    // CLOCK_MONOTONIC гарантує, що час завжди йде тільки вперед
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

// Додаткова зручна функція для затримки (sleep) у мілісекундах
void fds_sleep_ms(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}
// Time utils fuctions End ================================================================================================================

// FDS files and folders fuctions Start ================================================================================================================

size_t fds_get_file_size(const char *filepath)
{
#if defined(_WIN32) || defined(_WIN64)
    struct _stat64 st;
    if (_stat64(filepath, &st) != 0)
    {
        return (size_t)-1; // Error (file not found or not accessible)
    }
#else
    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        return (size_t)-1; // Error
    }
#endif
    return (size_t)st.st_size;
}

int fds_file_read_to_arena(const char *filepath, FixedArena *arena, SV *out_sv)
{
    if (!filepath || !arena || !out_sv)
        return 1;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return 1;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0)
    {
        fclose(f);
        return 1;
    }

    /* We allocate memory in the arena (+1 for the null terminator, if you suddenly need it) */
    char *buffer = (char *)fixed_arena_alloc(arena, (size_t)fsize + 1);

    size_t read_bytes = fread(buffer, 1, (size_t)fsize, f);
    if (read_bytes != (size_t)fsize && ferror(f))
    {
        fclose(f);
        return 1;
    }

    buffer[read_bytes] = '\0'; /* For C-string compatibility if you have to */

    out_sv->data = buffer;
    out_sv->count = read_bytes;

    fclose(f);
    return 0;
}

int fds_file_read_to_sb(const char *filepath, SB *out_sb)
{
    if (!filepath || !out_sb)
        return 1;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return 1;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0)
    {
        fclose(f);
        return 1;
    }

    /* We guarantee that SB will have enough space for the file */
    sb_grow(out_sb, (size_t)fsize + 1);

    size_t read_bytes = fread(out_sb->items + out_sb->count, 1, (size_t)fsize, f);
    if (read_bytes != (size_t)fsize && ferror(f))
    {
        fclose(f);
        return 1;
    }

    out_sb->count += read_bytes;
    out_sb->items[out_sb->count] = '\0'; /* Secure zero-termination */

    fclose(f);
    return 0;
}

int fds_file_write_sv(const char *filepath, SV content)
{
    if (!filepath)
        return 1;

    FILE *f = fopen(filepath, "wb");
    if (!f)
        return 1;

    if (content.count > 0 && content.data)
    {
        size_t written = fwrite(content.data, 1, content.count, f);
        if (written != content.count)
        {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

int fds_file_append_sv(const char *filepath, SV content)
{
    if (!filepath)
        return 1;

    FILE *f = fopen(filepath, "ab");
    if (!f)
        return 1;

    if (content.count > 0 && content.data)
    {
        size_t written = fwrite(content.data, 1, content.count, f);
        if (written != content.count)
        {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

/* Returns 0 and writes the extension (no dot) to out_ext, or 1 if there is no extension */
int fds_path_extension(SV filepath, SV *out_ext)
{
    if (!out_ext)
        return 1;

    size_t dot_idx = sv_rfind_char(filepath, '.');
    size_t slash_idx = sv_rfind_char(filepath, '/');   /* For Linux/compiler paths */
    size_t bslash_idx = sv_rfind_char(filepath, '\\'); /* For Windows/Win32 API */

    /* If there is no dot, or it is BEFORE the slash (for example, folder.a/file) */
    if (dot_idx == SIZE_MAX ||
        (slash_idx != SIZE_MAX && dot_idx < slash_idx) ||
        (bslash_idx != SIZE_MAX && dot_idx < bslash_idx))
    {
        *out_ext = sv_new();
        return 1;
    }

    *out_ext = sv_from_parts(filepath.data + dot_idx + 1, filepath.count - dot_idx - 1);
    return 0;
}
int fds_file_write_sb(const char *filepath, const SB *sb)
{
    if (!filepath || !sb)
        return 1;
    /* We use the existing logic through zero-copy conversion */
    return fds_file_write_sv(filepath, sb_to_sv(sb));
}

int fds_file_append_sb(const char *filepath, const SB *sb)
{
    if (!filepath || !sb)
        return 1;
    return fds_file_append_sv(filepath, sb_to_sv(sb));
}
#ifdef _WIN32

#else
#include <sys/stat.h>
#endif

// Returns the time the file was last modified. Returns (time_t)-1 on error.
time_t get_file_mtime(const char *path)
{
    if (path == NULL)
    {
        return (time_t)-1;
    }

#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA file_info;

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &file_info))
    {
        return (time_t)-1;
    }

    ULARGE_INTEGER ull;
    ull.LowPart = file_info.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = file_info.ftLastWriteTime.dwHighDateTime;

    /* FILETIME counts in 100-nanosecond intervals
       dated January 1, 1601 (UTC).
       time_t is usually counted in seconds since January 1, 1970 (UTC). */
    const unsigned long long FILE_TIME_OFFSET = 116444736000000000ULL;

    return (time_t)((ull.QuadPart - FILE_TIME_OFFSET) / 10000000ULL);
#else
    struct stat file_stat;

    if (stat(path, &file_stat) != 0)
    {
        return (time_t)-1;
    }

    return file_stat.st_mtime;
#endif
}
#ifdef _WIN32
wchar_t *fds_win32_utf8_to_utf16(const char *utf8_str)
{
    if (!utf8_str)
        return NULL;

    /* We find out the required size of the buffer */
    int req_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (req_len == 0)
        return NULL;

    /* We allocate memory */
    wchar_t *wstr = (wchar_t *)FDS_MALLOC(req_len * sizeof(wchar_t));
    if (!wstr)
        return NULL;

    /* We carry out the conversion */
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, req_len);
    return wstr;
}
int fds_file_delete_force(const char *filepath)
{
    if (!filepath)
        return 1;

#ifdef _WIN32
    /* We convert the path to UTF-16 so that the Cyrillic alphabet works */
    wchar_t *wpath = fds_win32_utf8_to_utf16(filepath);
    if (!wpath)
        return 1;

    /* Remove the "Read-Only" attribute.
     * This is often needed to remove files from folders like .git/ */
    SetFileAttributesW(wpath, FILE_ATTRIBUTE_NORMAL);

    int result = (DeleteFileW(wpath) != 0) ? 0 : 1;
    FDS_FREE(wpath);
    return result;
#else
    /* In Linux/POSIX, everything is much easier, because the system natively works with UTF-8 */
    return fds_file_delete(filepath);
#endif
}
int fds_dir_delete_force(const char *dirpath)
{
    if (!dirpath)
        return 1;

    FdsDirIter iter;
    /* Open the folder iterator */
    if (fds_dir_iter_open(dirpath, &iter) != 0)
    {
        return 1; /* The folder does not exist or cannot be accessed */
    }

    SV item_name;
    int is_dir;
    int has_errors = 0;

    /* We go through all the content */
    while (fds_dir_iter_next(&iter, &item_name, &is_dir) == 0)
    {
        /* Form the full path to the file/folder: dirpath + "/" + item_name */
        SB full_path = sb_from_cstr(dirpath);

        /* Add a separator if there is none */
        if (full_path.count > 0 &&
            full_path.items[full_path.count - 1] != '/' &&
            full_path.items[full_path.count - 1] != '\\')
        {
            sb_append_char(&full_path, '/');
        }
        sb_append_sv(&full_path, item_name);

        /* We convert SB into a classic C-string for further transmission */
        char *path_cstr = sb_to_cstr(&full_path);

        if (is_dir)
        {
            /* We recursively dive into the subfolder */
            if (fds_dir_delete_force(path_cstr) != 0)
            {
                has_errors = 1;
            }
        }
        else
        {
            /* We delete the file */
            if (fds_file_delete_force(path_cstr) != 0)
            {
                has_errors = 1;
            }
        }

        /* Be sure to free the builder's memory at each iteration */
        sb_free(&full_path);
    }
    fds_dir_iter_close(&iter);

    /* Now that the folder is empty, delete it itself.
     * Let's apply the Wide-API for Windows so that the Cyrillic alphabet in the name of the folder itself also works. */
#ifdef _WIN32
    wchar_t *wdir = fds_win32_utf8_to_utf16(dirpath);
    if (!wdir)
        return 1;

    SetFileAttributesW(wdir, FILE_ATTRIBUTE_NORMAL);
    int dir_result = (RemoveDirectoryW(wdir) != 0) ? 0 : 1;
    FDS_FREE(wdir);

    return (has_errors == 0 && dir_result == 0) ? 0 : 1;
#else
    int dir_result = fds_dir_delete(dirpath);
    return (has_errors == 0 && dir_result == 0) ? 0 : 1;
#endif
}
int fds_file_delete(const char *filepath)
{
    if (!filepath)
        return 1;
    if (DeleteFileA(filepath) != 0)
    {
        return 0;
    }
    return 1;
}

int fds_dir_delete(const char *dirpath)
{
    if (!dirpath)
        return 1;
    if (RemoveDirectoryA(dirpath) != 0)
    {
        return 0;
    }
    return 1;
}
int fds_dir_create(const char *dirpath)
{
    if (!dirpath)
        return 1;

    /* CreateDirectoryA returns a non-zero value on success */
    if (CreateDirectoryA(dirpath, NULL) != 0)
    {
        return 0;
    }

    /* If necessary, here you can check GetLastError() == ERROR_ALREADY_EXISTS,
     * but in a strict paradigm, if it failed to create - it's an error. */
    return 1;
}

int fds_rename(const char *oldpath, const char *newpath)
{
    if (!oldpath || !newpath)
        return 1;

    /* MoveFileA works for both files and folders.
     * Returns a non-zero value on success. */
    if (MoveFileA(oldpath, newpath) != 0)
    {
        return 0;
    }

    return 1;
}
int fds_file_exists(const char *filepath)
{
    if (!filepath)
        return 1;
    DWORD attr = GetFileAttributesA(filepath);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 1;
    /* If it's a directory, it's not a file */
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

int fds_dir_exists(const char *dirpath)
{
    if (!dirpath)
        return 1;
    DWORD attr = GetFileAttributesA(dirpath);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 1;
    /* We check the flag of the directory */
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
}

int fds_dir_iter_open(const char *dirpath, FdsDirIter *iter)
{
    if (!dirpath || !iter)
        return 1;

    char search_path[MAX_PATH];
    /* Windows FindFirstFile requires the format "C:\Folder\*" */
    snprintf(search_path, sizeof(search_path), "%s\\*", dirpath);

    WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)FDS_MALLOC(sizeof(WIN32_FIND_DATAA));
    if (!fd)
        return 1;

    HANDLE hFind = FindFirstFileA(search_path, fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        FDS_FREE(fd);
        return 1;
    }

    iter->internal_handle = hFind;
    iter->internal_find_data = fd;
    iter->is_first = 1;
    return 0;
}

int fds_dir_iter_next(FdsDirIter *iter, SV *out_name, int *out_is_dir)
{
    if (!iter || !out_name)
        return 1;

    HANDLE hFind = (HANDLE)iter->internal_handle;
    WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)iter->internal_find_data;

    if (!iter->is_first)
    {
        if (!FindNextFileA(hFind, fd))
            return 1;
    }
    iter->is_first = 0;

    /* Skip system navigation directories "." and ".." */
    while (strcmp(fd->cFileName, ".") == 0 || strcmp(fd->cFileName, "..") == 0)
    {
        if (!FindNextFileA(hFind, fd))
            return 1;
    }

    *out_name = sv_from_cstr(fd->cFileName);

    if (out_is_dir)
    {
        *out_is_dir = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
    }
    return 0;
}

void fds_dir_iter_close(FdsDirIter *iter)
{
    if (!iter)
        return;
    if (iter->internal_handle && iter->internal_handle != INVALID_HANDLE_VALUE)
    {
        FindClose((HANDLE)iter->internal_handle);
    }
    if (iter->internal_find_data)
    {
        FDS_FREE(iter->internal_find_data);
    }
    iter->internal_handle = NULL;
    iter->internal_find_data = NULL;
}

#else // UNIX
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

int fds_file_delete(const char *filepath)
{
    if (!filepath)
        return 1;
    /* unlink() returns 0 on success */
    if (unlink(filepath) == 0)
    {
        return 0;
    }
    return 1;
}

int fds_dir_delete(const char *dirpath)
{
    if (!dirpath)
        return 1;
    /* rmdir() returns 0 on success */
    if (rmdir(dirpath) == 0)
    {
        return 0;
    }
    return 1;
}

int fds_file_exists(const char *filepath)
{
    if (!filepath)
        return 1;
    struct stat st;
    if (stat(filepath, &st) != 0)
        return 1;
    return S_ISREG(st.st_mode) ? 0 : 1;
}

int fds_dir_exists(const char *dirpath)
{
    if (!dirpath)
        return 1;
    struct stat st;
    if (stat(dirpath, &st) != 0)
        return 1;
    return S_ISDIR(st.st_mode) ? 0 : 1;
}

int fds_dir_iter_open(const char *dirpath, FdsDirIter *iter)
{
    if (!dirpath || !iter)
        return 1;

    DIR *d = opendir(dirpath);
    if (!d)
        return 1;

    iter->internal_handle = d;
    iter->internal_find_data = NULL; /* No additional structure is required on Linux */
    return 0;
}

int fds_dir_iter_next(FdsDirIter *iter, SV *out_name, int *out_is_dir)
{
    if (!iter || !out_name)
        return 1;

    DIR *d = (DIR *)iter->internal_handle;
    struct dirent *dir;

    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
        {
            continue; /* We skip it */
        }

        *out_name = sv_from_cstr(dir->d_name);

        if (out_is_dir)
        {
            /* DT_DIR is supported by most modern file systems (ext4, btrfs) */
            *out_is_dir = (dir->d_type == DT_DIR) ? 1 : 0;
        }
        return 0;
    }
    return 1; /* Items have run out */
}

void fds_dir_iter_close(FdsDirIter *iter)
{
    if (!iter)
        return;
    if (iter->internal_handle)
    {
        closedir((DIR *)iter->internal_handle);
    }
    iter->internal_handle = NULL;
}
#endif



static block_header *header_from_ptr(void *ptr) {
    return (block_header*)((char*)ptr - HEADER_SIZE);
}

static void *ptr_from_header(block_header *h) {
    return (void*)((char*)h + HEADER_SIZE);
}

static block_header *raw_alloc_block(size_t size) {
    block_header *h = (block_header*)malloc(HEADER_SIZE + size);
    if (!h) return NULL;
    h->size = size;
    h->next = NULL;
    h->prev = NULL;
    h->is_tmp = 0;
#ifdef DEBUG_MEM
    h->file = NULL;
    h->line = 0;
    h->is_permanent = 0;
#endif
    return h;
}

static void raw_free_block(block_header *h) {
    free(h);
}

static void list_push(block_header **head, block_header *h) {
    h->next = *head;
    h->prev = NULL;
    if (*head) {
        (*head)->prev = h;
    }
    *head = h;
}

static void list_remove(block_header **head, block_header *target) {
    if (target->prev) {
        target->prev->next = target->next;
    } else {
        *head = target->next;
    }
    if (target->next) {
        target->next->prev = target->prev;
    }
    target->prev = NULL;
    target->next = NULL;
}

void fds_allocator_clear_tmp(fds_allocator *a) {
    if (!a) a = fds_allocator_current();
    if (!a || !a->tmp_active) return;
    
    block_header *active = (block_header*)a->tmp_active;
    block_header *last = active;
    while (last->next) last = last->next;
    
    last->next = (block_header*)a->tmp_free;
    if (a->tmp_free) {
        ((block_header*)a->tmp_free)->prev = last;
    }
    
    a->tmp_free = active;
    active->prev = NULL;
    a->tmp_active = NULL;
}

#ifdef DEBUG_MEM
static void update_peak(fds_allocator *a) {
    if (a->stats_current_allocated > a->stats_peak_allocated) {
        a->stats_peak_allocated = a->stats_current_allocated;
    }
}
#endif

static void *alloc_internal(fds_allocator *a, size_t size, const char *file, int line, int is_permanent) {
    if (!a) a = fds_allocator_current();

    block_header *h = raw_alloc_block(size);
    if (!h) return NULL;
    
    h->is_tmp = 0;
    list_push((block_header**)&a->all_blocks, h);
    a->live_blocks_count++;

#ifdef DEBUG_MEM
    h->file = file;
    h->line = line;
    h->is_permanent = is_permanent;
    if (is_permanent) a->stats_permanent_count++;
    a->stats_alloc_count++;
    a->stats_current_allocated += size;
    a->stats_total_allocated += size;
    update_peak(a);
#else
    (void)file; (void)line; (void)is_permanent;
#endif

    return ptr_from_header(h);
}

void *fds_alloc_impl(fds_allocator *a, size_t size) {
    return alloc_internal(a, size, NULL, 0, 0);
}

void *fds_calloc_impl(fds_allocator *a, size_t num, size_t size) {
    if (!a) a = fds_allocator_current();
    size_t total = num * size;
    void *ptr = alloc_internal(a, total, NULL, 0, 0);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *fds_realloc_impl(fds_allocator *a, void *ptr, size_t new_size) {
    if (!a) a = fds_allocator_current();
    if (ptr == NULL) return alloc_internal(a, new_size, NULL, 0, 0);
    if (new_size == 0) {
        fds_free_impl(a, ptr);
        return NULL;
    }
    
    block_header *h = header_from_ptr(ptr);
    if (h->is_tmp) {
        list_remove((block_header**)&a->tmp_active, h);
    } else {
        list_remove((block_header**)&a->all_blocks, h);
    }
    
    block_header *new_h = (block_header*)realloc(h, HEADER_SIZE + new_size);
    if (!new_h) {
        if (h->is_tmp) {
            list_push((block_header**)&a->tmp_active, h);
        } else {
            list_push((block_header**)&a->all_blocks, h);
        }
        return NULL;
    }
    
    new_h->size = new_size;
    if (new_h->is_tmp) {
        list_push((block_header**)&a->tmp_active, new_h);
    } else {
        list_push((block_header**)&a->all_blocks, new_h);
    }
    return ptr_from_header(new_h);
}

void fds_free_impl(fds_allocator *a, void *ptr) {
    if (!a) a = fds_allocator_current();
    if (!ptr) return;

    block_header *h = header_from_ptr(ptr);
    if (h->is_tmp) {
        list_remove((block_header**)&a->tmp_active, h);
    } else {
        list_remove((block_header**)&a->all_blocks, h);
        a->live_blocks_count--;
#ifdef DEBUG_MEM
        a->stats_free_count++;
        a->stats_current_allocated -= h->size;
        a->stats_total_freed += h->size;
#endif
    }
    raw_free_block(h);
}

void *fds_alloc_tmp_impl(fds_allocator *a, size_t size) {
    if (!a) a = fds_allocator_current();
    block_header *h = NULL;
    block_header **indirect = (block_header**)&a->tmp_free;
    
    while (*indirect) {
        if ((*indirect)->size >= size) {
            h = *indirect;
            *indirect = h->next;
            if (*indirect) (*indirect)->prev = NULL;
            break;
        }
        indirect = &(*indirect)->next;
    }
    
    if (!h) {
        h = raw_alloc_block(size);
        if (!h) return NULL;
    } else {
        h->size = size;
    }
    
    h->is_tmp = 1;
    list_push((block_header**)&a->tmp_active, h);
    return ptr_from_header(h);
}

void *fds_alloc_permanent_impl(fds_allocator *a, size_t size) {
    return alloc_internal(a, size, NULL, 0, 1);
}

#ifdef DEBUG_MEM
void *fds_alloc_impl_tracked(fds_allocator *a, size_t size, const char *file, int line) {
    return alloc_internal(a, size, file, line, 0);
}

void *fds_calloc_impl_tracked(fds_allocator *a, size_t num, size_t size, const char *file, int line) {
    if (!a) a = fds_allocator_current();
    size_t total = num * size;
    void *ptr = alloc_internal(a, total, file, line, 0);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *fds_realloc_impl_tracked(fds_allocator *a, void *ptr, size_t new_size, const char *file, int line) {
    if (!a) a = fds_allocator_current();
    if (ptr == NULL) return alloc_internal(a, new_size, file, line, 0);
    if (new_size == 0) {
        fds_free_impl(a, ptr);
        return NULL;
    }
    
    block_header *h = header_from_ptr(ptr);
    if (h->is_tmp) {
        list_remove((block_header**)&a->tmp_active, h);
    } else {
        list_remove((block_header**)&a->all_blocks, h);
    }
    
    size_t old_size = h->size;
    block_header *new_h = (block_header*)realloc(h, HEADER_SIZE + new_size);
    if (!new_h) {
        if (h->is_tmp) {
            list_push((block_header**)&a->tmp_active, h);
        } else {
            list_push((block_header**)&a->all_blocks, h);
        }
        return NULL;
    }
    
    new_h->size = new_size;
    new_h->file = file;
    new_h->line = line;
    
    if (new_h->is_tmp) {
        list_push((block_header**)&a->tmp_active, new_h);
    } else {
        list_push((block_header**)&a->all_blocks, new_h);
        a->stats_realloc_count++;
        if (new_size > old_size) {
            a->stats_current_allocated += (new_size - old_size);
            a->stats_total_allocated += (new_size - old_size);
        } else {
            a->stats_current_allocated -= (old_size - new_size);
            a->stats_total_freed += (old_size - new_size);
        }
        update_peak(a);
    }

    return ptr_from_header(new_h);
}

void *fds_alloc_tmp_impl_tracked(fds_allocator *a, size_t size, const char *file, int line) {
    if (!a) a = fds_allocator_current();
    block_header *h = NULL;
    block_header **indirect = (block_header**)&a->tmp_free;
    
    while (*indirect) {
        if ((*indirect)->size >= size) {
            h = *indirect;
            *indirect = h->next;
            if (*indirect) (*indirect)->prev = NULL;
            break;
        }
        indirect = &(*indirect)->next;
    }

    int new_block_created = 0;
    if (!h) {
        h = raw_alloc_block(size);
        if (!h) return NULL;
        new_block_created = 1;
    } else {
        h->size = size;
    }
    
    h->is_tmp = 1;
    h->file = file;
    h->line = line;
    h->is_permanent = 0; 
    list_push((block_header**)&a->tmp_active, h);

    a->stats_tmp_alloc_calls++;
    if (new_block_created) {
        a->stats_tmp_new_blocks++;
        a->stats_current_allocated += size;
        a->stats_total_allocated += size;
        update_peak(a);
    }

    return ptr_from_header(h);
}

void *fds_alloc_permanent_impl_tracked(fds_allocator *a, size_t size, const char *file, int line) {
    return alloc_internal(a, size, file, line, 1);
}
#endif

fds_allocator *fds_allocator_create(void) {
    fds_allocator *a = (fds_allocator*)calloc(1, sizeof(fds_allocator));
    if (!a) return NULL;

    a->alloc_fn = fds_alloc_impl;
    a->calloc_fn = fds_calloc_impl;
    a->realloc_fn = fds_realloc_impl;
    a->free_fn = fds_free_impl;
    a->alloc_tmp_fn = fds_alloc_tmp_impl;
    a->alloc_permanent_fn = fds_alloc_permanent_impl;

    return a;
}

void fds_allocator_destroy(fds_allocator *a) {
    if (!a) return;

#ifdef DEBUG_MEM
    block_header *h_leak = (block_header*)a->all_blocks;
    int leak_count = 0;
    fprintf(stderr, "=== Leak report for allocator %p ===\n", (void*)a);
    while (h_leak) {
        if (!h_leak->is_permanent) {
            fprintf(stderr, "LEAK: block of %zu bytes allocated at %s:%d\n",
                    h_leak->size, h_leak->file ? h_leak->file : "unknown", h_leak->line);
            leak_count++;
        }
        h_leak = h_leak->next;
    }
    if (leak_count == 0) {
        fprintf(stderr, "No non-permanent leaks detected.\n");
    } else {
        fprintf(stderr, "Total non-permanent leaks: %d\n", leak_count);
    }
    fprintf(stderr, "===================================\n");
#endif

    void *lists[] = { a->all_blocks, a->tmp_active, a->tmp_free };
    for (int i = 0; i < 3; i++) {
        block_header *curr = (block_header*)lists[i];
        while (curr) {
            block_header *next = curr->next;
            raw_free_block(curr);
            curr = next;
        }
    }

    free(a);
}

#define FDS_MAX_ALLOC_STACK 16

static FDS_THREAD_LOCAL fds_allocator *tls_current_allocator = NULL;
static FDS_THREAD_LOCAL fds_allocator *tls_alloc_stack[FDS_MAX_ALLOC_STACK];
static FDS_THREAD_LOCAL int tls_alloc_stack_depth = 0;

fds_allocator *fds_allocator_current(void) {
    if (tls_current_allocator == NULL) {
        static FDS_THREAD_LOCAL fds_allocator *default_allocator = NULL;
        if (default_allocator == NULL) {
            default_allocator = fds_allocator_create();
        }
        return default_allocator;
    }
    return tls_current_allocator;
}

void fds_allocator_push(fds_allocator *a) {
    if (tls_alloc_stack_depth < FDS_MAX_ALLOC_STACK) {
        tls_alloc_stack[tls_alloc_stack_depth++] = tls_current_allocator;
    }
    tls_current_allocator = a;
}

void fds_allocator_pop(void) {
    if (tls_alloc_stack_depth > 0) {
        tls_current_allocator = tls_alloc_stack[--tls_alloc_stack_depth];
    } else {
        tls_current_allocator = NULL;
    }
}

size_t fds_allocator_live_blocks_count(fds_allocator *a) {
    if (!a) a = fds_allocator_current();
    return a->live_blocks_count;
}

#ifdef DEBUG_MEM
void fds_allocator_get_stats(fds_allocator *a, fds_allocator_stats *out_stats) {
    if (!a) a = fds_allocator_current();
    if (!out_stats) return;
    
    out_stats->alloc_count = a->stats_alloc_count;
    out_stats->realloc_count = a->stats_realloc_count;
    out_stats->free_count = a->stats_free_count;
    out_stats->tmp_alloc_calls = a->stats_tmp_alloc_calls;
    out_stats->tmp_new_blocks = a->stats_tmp_new_blocks;
    out_stats->permanent_count = a->stats_permanent_count;
    out_stats->current_allocated = a->stats_current_allocated;
    out_stats->peak_allocated = a->stats_peak_allocated;
    out_stats->total_allocated = a->stats_total_allocated;
    out_stats->total_freed = a->stats_total_freed;
}

void fds_allocator_print_stats(fds_allocator *a) {
    if (!a) a = fds_allocator_current();
    fds_allocator_stats s;
    fds_allocator_get_stats(a, &s);
    
    fprintf(stderr, "=== Allocator stats ===\n");
    fprintf(stderr, "Alloc count:        %zu\n", s.alloc_count);
    fprintf(stderr, "Realloc count:      %zu\n", s.realloc_count);
    fprintf(stderr, "Free count:         %zu\n", s.free_count);
    fprintf(stderr, "Temp alloc calls:   %zu\n", s.tmp_alloc_calls);
    fprintf(stderr, "Temp new blocks:    %zu\n", s.tmp_new_blocks);
    fprintf(stderr, "Temp reuse count:   %zu\n", s.tmp_alloc_calls - s.tmp_new_blocks);
    fprintf(stderr, "Permanent blocks:   %zu\n", s.permanent_count);
    fprintf(stderr, "Current allocated:  %zu bytes\n", s.current_allocated);
    fprintf(stderr, "Peak allocated:     %zu bytes\n", s.peak_allocated);
    fprintf(stderr, "Total allocated:    %zu bytes\n", s.total_allocated);
    fprintf(stderr, "Total freed:        %zu bytes\n", s.total_freed);
    fprintf(stderr, "=======================\n");
}
#endif


// FDS files and folders fuctions End ================================================================================================================
#endif // FDS_IMPL
#ifdef __cplusplus
}
#endif
#endif // FDS_H