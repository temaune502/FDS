/*
    fds_bytes_builder.h — STB-style dynamic binary buffer (FDS).
    
    Features:
    - Dynamic memory growth for writing raw binary data safely.
    - Endian-independent integer writing (LE/BE).
    - Seamless conversion to/from FdsBytesView.
    - Customizable memory allocators via FDS_MALLOC / FDS_REALLOC / FDS_FREE.

    Usage:
    1. Make sure "fds_bytes_view.h" is available in your project.
    2. #include "fds_bytes_builder.h"
    3. In EXACTLY ONE C file, add:
       #define FDS_BYTES_BUILDER_IMPLEMENTATION
       #include "fds_bytes_builder.h"
*/

#ifndef FDS_BYTES_BUILDER_H
#define FDS_BYTES_BUILDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>



#ifndef FDS_ASSERT
    #include <assert.h>
    #define FDS_ASSERT(cond, msg) assert((cond) && (msg))
#endif

// Memory allocator overrides (easy integration with FDS Arena Allocators)
#ifndef FDS_MALLOC
    #include <stdlib.h>
    #define FDS_MALLOC(sz)       malloc(sz)
    #define FDS_REALLOC(ptr, sz) realloc(ptr, sz)
    #define FDS_FREE(ptr)        free(ptr)
#endif

#ifndef FDS_BYTES
    #define FDS_BYTES extern
#endif

// A dynamically growing buffer for writing binary data.
typedef struct {
    uint8_t *data;    // Pointer to allocated memory
    size_t size;      // Current number of bytes written
    size_t capacity;  // Total allocated capacity
} FdsBytesBuilder;

typedef struct {
    const uint8_t *data;
    size_t size;
} FdsBytesView;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LIFECYCLE & MEMORY
// ============================================================================

// Creates a new builder with an optional initial capacity (0 is fine).
// e.g., FdsBytesBuilder bb = fds_bb_create(1024);
FDS_BYTES FdsBytesBuilder fds_bb_create(size_t initial_capacity);

// Frees the underlying memory of the builder.
// e.g., fds_bb_destroy(&bb);
FDS_BYTES void            fds_bb_destroy(FdsBytesBuilder *bb);

// Ensures the builder has enough capacity to add 'additional_size' bytes.
// Automatically called by append functions, but useful for pre-allocating.
FDS_BYTES void            fds_bb_reserve(FdsBytesBuilder *bb, size_t additional_size);

// Resets the size to 0 but keeps the allocated memory (capacity remains).
FDS_BYTES void            fds_bb_clear(FdsBytesBuilder *bb);


// ============================================================================
// BYTESVIEW INTEROP (Conversion)
// ============================================================================

// Converts a Builder into a non-owning View. 
// Note: The returned View becomes invalid if the Builder is destroyed or grows.
// e.g., FdsBytesView final_data = fds_bb_to_view(&bb);
FDS_BYTES FdsBytesView fds_bb_to_view(const FdsBytesBuilder *bb);

// Creates a new Builder by copying all data from an existing View.
// e.g., FdsBytesBuilder bb = fds_bb_from_view(my_view);
FDS_BYTES FdsBytesBuilder fds_bb_from_view(FdsBytesView view);

// Appends all data from a View into the Builder.
// e.g., fds_bb_append_view(&bb, file_header_view);
FDS_BYTES void fds_bb_append_view(FdsBytesBuilder *bb, FdsBytesView view);


// ============================================================================
// WRITING / APPENDING
// ============================================================================

// Appends a raw memory block to the builder.
// e.g., fds_bb_append(&bb, &my_struct, sizeof(my_struct));
FDS_BYTES void fds_bb_append(FdsBytesBuilder *bb, const void *data, size_t size);

// Appends a single byte.
// e.g., fds_bb_append_byte(&bb, 0xFF);
FDS_BYTES void fds_bb_append_byte(FdsBytesBuilder *bb, uint8_t byte);

// Appends a 16-bit unsigned integer (Little-Endian).
FDS_BYTES void fds_bb_append_u16_le(FdsBytesBuilder *bb, uint16_t val);

// Appends a 16-bit unsigned integer (Big-Endian / Network Order).
FDS_BYTES void fds_bb_append_u16_be(FdsBytesBuilder *bb, uint16_t val);

// Appends a 32-bit unsigned integer (Little-Endian).
FDS_BYTES void fds_bb_append_u32_le(FdsBytesBuilder *bb, uint32_t val);

// Appends a 32-bit unsigned integer (Big-Endian / Network Order).
FDS_BYTES void fds_bb_append_u32_be(FdsBytesBuilder *bb, uint32_t val);

// Appends a 64-bit unsigned integer
FDS_BYTES void fds_bb_append_u64_le(FdsBytesBuilder *bb, uint64_t val);
FDS_BYTES void fds_bb_append_u64_be(FdsBytesBuilder *bb, uint64_t val);

// Appends a 32-bit float (IEEE 754) safely
FDS_BYTES void fds_bb_append_f32_le(FdsBytesBuilder *bb, float val);
FDS_BYTES void fds_bb_append_f32_be(FdsBytesBuilder *bb, float val);

// Appends a 64-bit double (IEEE 754) safely
FDS_BYTES void fds_bb_append_f64_le(FdsBytesBuilder *bb, double val);
FDS_BYTES void fds_bb_append_f64_be(FdsBytesBuilder *bb, double val);

// Повертає поточну позицію (offset) для майбутнього патчінгу
FDS_BYTES size_t fds_bb_get_pos(const FdsBytesBuilder *bb);

// Перезаписує 32-бітне число за вказаним зміщенням (без зміни розміру буфера)
FDS_BYTES void fds_bb_patch_u32_le(FdsBytesBuilder *bb, size_t offset, uint32_t val);

// Додає класичний C-рядок (з нуль-термінатором на кінці)
FDS_BYTES void fds_bb_append_cstr(FdsBytesBuilder *bb, const char *str);

// Додає рядок з префіксом довжини (u16)
FDS_BYTES void fds_bb_append_string_u16(FdsBytesBuilder *bb, const char *str);

FDS_BYTES bool fds_bb_save_to_file(const FdsBytesBuilder *bb, const char *filepath);


// ============================================================================
// CONSTRUCTORS
// ============================================================================

// Creates a BytesView from a raw data pointer and size.
// e.g., fds_bv(buffer, 1024);
FDS_BYTES FdsBytesView fds_bv(const void *data, size_t size);

// Returns an empty BytesView (data = NULL, size = 0).
FDS_BYTES FdsBytesView fds_bv_empty(void);

// Creates a BytesView from a null-terminated C string. 
// Note: It calculates length up to, but not including, the '\0'.
// e.g., fds_bv_from_cstr("PNG_MAGIC");
FDS_BYTES FdsBytesView fds_bv_from_cstr(const char *str);


// ============================================================================
// SLICING & REGIONS (Returns a new view, does not mutate the original)
// ============================================================================

// Safely extracts a sub-region from the view. Clamps length to available size.
// e.g., FdsBytesView payload = fds_bv_subview(packet, 12, 100); // skip 12 byte header
FDS_BYTES FdsBytesView fds_bv_subview(FdsBytesView view, size_t offset, size_t length);

// Returns a view containing the first 'count' bytes. Clamps to view.size.
FDS_BYTES FdsBytesView fds_bv_take(FdsBytesView view, size_t count);

// Returns a view skipping the first 'count' bytes. Clamps to view.size.
// e.g., view = fds_bv_skip(view, 4); // skip 4 bytes of magic number
FDS_BYTES FdsBytesView fds_bv_skip(FdsBytesView view, size_t count);


// ============================================================================
// COMPARISON & SEARCH
// ============================================================================

// Deep comparison of two byte views. Returns true if sizes and contents match exactly.
FDS_BYTES bool         fds_bv_equals(FdsBytesView a, FdsBytesView b);

// Checks if the view starts with the given prefix.
// e.g., fds_bv_has_prefix(file_data, fds_bv_from_cstr("PK\x03\x04")); // check ZIP magic
FDS_BYTES bool         fds_bv_has_prefix(FdsBytesView view, FdsBytesView prefix);

// Checks if the view ends with the given suffix.
FDS_BYTES bool         fds_bv_has_suffix(FdsBytesView view, FdsBytesView suffix);

// Finds the first occurrence of a specific byte. Returns index, or -1 if not found.
// e.g., intptr_t null_pos = fds_bv_find_byte(view, 0x00);
FDS_BYTES intptr_t     fds_bv_find_byte(FdsBytesView view, uint8_t byte);

// Finds the first occurrence of a byte pattern. Returns index, or -1 if not found.
// e.g., intptr_t sig_pos = fds_bv_find_subview(view, fds_bv(signature, 4));
FDS_BYTES intptr_t     fds_bv_find_subview(FdsBytesView view, FdsBytesView pattern);


// ============================================================================
// STREAM PARSING (Mutates the view pointer/size directly to advance through data)
// ============================================================================

// Pops 1 byte from the front of the view and advances it. Returns false if empty.
// e.g., uint8_t type; if (fds_bv_pop_byte(&stream, &type)) { ... }
FDS_BYTES bool         fds_bv_pop_byte(FdsBytesView *view, uint8_t *out_byte);

// Pops 'count' bytes from the front, returning them as a new view, and advances the original view.
// e.g., FdsBytesView header = fds_bv_pop_bytes(&stream, 16);
FDS_BYTES FdsBytesView fds_bv_pop_bytes(FdsBytesView *view, size_t count);

// Reads a 16-bit unsigned integer (Little-Endian) and advances the view by 2 bytes.
FDS_BYTES bool         fds_bv_read_u16_le(FdsBytesView *view, uint16_t *out_val);

// Reads a 16-bit unsigned integer (Big-Endian / Network Order) and advances the view by 2 bytes.
// e.g., uint16_t port; fds_bv_read_u16_be(&tcp_packet, &port);
FDS_BYTES bool         fds_bv_read_u16_be(FdsBytesView *view, uint16_t *out_val);

// Reads a 32-bit unsigned integer (Little-Endian) and advances the view by 4 bytes.
// e.g., uint32_t chunk_size; fds_bv_read_u32_le(&png_stream, &chunk_size);
FDS_BYTES bool         fds_bv_read_u32_le(FdsBytesView *view, uint32_t *out_val);

// Reads a 32-bit unsigned integer (Big-Endian / Network Order) and advances the view by 4 bytes.
FDS_BYTES bool         fds_bv_read_u32_be(FdsBytesView *view, uint32_t *out_val);

// Reads a 64-bit unsigned integer (Little-Endian / Big-Endian)
FDS_BYTES bool fds_bv_read_u64_le(FdsBytesView *view, uint64_t *out_val);
FDS_BYTES bool fds_bv_read_u64_be(FdsBytesView *view, uint64_t *out_val);

// Reads a 32-bit float (IEEE 754) safely avoiding strict-aliasing UB
FDS_BYTES bool fds_bv_read_f32_le(FdsBytesView *view, float *out_val);
FDS_BYTES bool fds_bv_read_f32_be(FdsBytesView *view, float *out_val);

// Reads a 64-bit double (IEEE 754)
FDS_BYTES bool fds_bv_read_f64_le(FdsBytesView *view, double *out_val);
FDS_BYTES bool fds_bv_read_f64_be(FdsBytesView *view, double *out_val);

// Читає рядок, очікуючи u16 префікс довжини. 
// Повертає View, що вказує лише на текст (без копіювання!).
FDS_BYTES bool fds_bv_read_string_u16(FdsBytesView *view, FdsBytesView *out_str_view);


#ifdef __cplusplus
}
#endif

#endif // FDS_BYTES_BUILDER_H

/* ============================================================================
   IMPLEMENTATION
   ============================================================================ */
#ifdef FDS_EXT_BYTES_IMPL

#include <string.h> // For memcpy

FDS_BYTES FdsBytesBuilder fds_bb_create(size_t initial_capacity) {
    FdsBytesBuilder bb = {0};
    if (initial_capacity > 0) {
        bb.data = (uint8_t*)FDS_MALLOC(initial_capacity);
        FDS_ASSERT(bb.data != NULL, "BytesBuilder: memory allocation failed");
        bb.capacity = initial_capacity;
    }
    return bb;
}

FDS_BYTES void fds_bb_destroy(FdsBytesBuilder *bb) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    if (bb->data) {
        FDS_FREE(bb->data);
        bb->data = NULL;
    }
    bb->size = 0;
    bb->capacity = 0;
}

FDS_BYTES void fds_bb_reserve(FdsBytesBuilder *bb, size_t additional_size) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    
    if (bb->size + additional_size <= bb->capacity) {
        return; // Already have enough space
    }
    
    // Standard growth strategy: double the capacity or match exact needs
    size_t new_capacity = bb->capacity == 0 ? 16 : bb->capacity;
    while (new_capacity < bb->size + additional_size) {
        new_capacity *= 2;
    }
    
    uint8_t *new_data = (uint8_t*)FDS_REALLOC(bb->data, new_capacity);
    FDS_ASSERT(new_data != NULL, "BytesBuilder: memory reallocation failed");
    
    bb->data = new_data;
    bb->capacity = new_capacity;
}

FDS_BYTES void fds_bb_clear(FdsBytesBuilder *bb) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    bb->size = 0;
}

// --- BytesView Interop ---

FDS_BYTES FdsBytesView fds_bb_to_view(const FdsBytesBuilder *bb) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    return fds_bv(bb->data, bb->size);
}

FDS_BYTES FdsBytesBuilder fds_bb_from_view(FdsBytesView view) {
    FdsBytesBuilder bb = fds_bb_create(view.size);
    if (view.size > 0 && view.data != NULL) {
        fds_bb_append(&bb, view.data, view.size);
    }
    return bb;
}

FDS_BYTES void fds_bb_append_view(FdsBytesBuilder *bb, FdsBytesView view) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    if (view.size > 0 && view.data != NULL) {
        fds_bb_append(bb, view.data, view.size);
    }
}

// --- Appending ---

FDS_BYTES void fds_bb_append(FdsBytesBuilder *bb, const void *data, size_t size) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    if (size == 0 || data == NULL) return;
    
    fds_bb_reserve(bb, size);
    memcpy(bb->data + bb->size, data, size);
    bb->size += size;
}

FDS_BYTES void fds_bb_append_byte(FdsBytesBuilder *bb, uint8_t byte) {
    FDS_ASSERT(bb != NULL, "BytesBuilder pointer is NULL");
    fds_bb_reserve(bb, 1);
    bb->data[bb->size++] = byte;
}

FDS_BYTES void fds_bb_append_u16_le(FdsBytesBuilder *bb, uint16_t val) {
    fds_bb_reserve(bb, 2);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
}

FDS_BYTES void fds_bb_append_u16_be(FdsBytesBuilder *bb, uint16_t val) {
    fds_bb_reserve(bb, 2);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
}

FDS_BYTES void fds_bb_append_u32_le(FdsBytesBuilder *bb, uint32_t val) {
    fds_bb_reserve(bb, 4);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 24) & 0xFF);
}

FDS_BYTES void fds_bb_append_u32_be(FdsBytesBuilder *bb, uint32_t val) {
    fds_bb_reserve(bb, 4);
    bb->data[bb->size++] = (uint8_t)((val >> 24) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[bb->size++] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[bb->size++] = (uint8_t)(val & 0xFF);
}
FDS_BYTES void fds_bb_append_u64_le(FdsBytesBuilder *bb, uint64_t val) {
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

FDS_BYTES void fds_bb_append_u64_be(FdsBytesBuilder *bb, uint64_t val) {
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

FDS_BYTES void fds_bb_append_f32_le(FdsBytesBuilder *bb, float val) {
    uint32_t temp;
    memcpy(&temp, &val, sizeof(float));
    fds_bb_append_u32_le(bb, temp);
}

FDS_BYTES void fds_bb_append_f32_be(FdsBytesBuilder *bb, float val) {
    uint32_t temp;
    memcpy(&temp, &val, sizeof(float));
    fds_bb_append_u32_be(bb, temp);
}

FDS_BYTES void fds_bb_append_f64_le(FdsBytesBuilder *bb, double val) {
    uint64_t temp;
    memcpy(&temp, &val, sizeof(double));
    fds_bb_append_u64_le(bb, temp);
}

FDS_BYTES void fds_bb_append_f64_be(FdsBytesBuilder *bb, double val) {
    uint64_t temp;
    memcpy(&temp, &val, sizeof(double));
    fds_bb_append_u64_be(bb, temp);
}


FDS_BYTES size_t fds_bb_get_pos(const FdsBytesBuilder *bb) {
    FDS_ASSERT(bb != NULL, "BytesBuilder is NULL");
    return bb->size;
}

FDS_BYTES void fds_bb_patch_u32_le(FdsBytesBuilder *bb, size_t offset, uint32_t val) {
    FDS_ASSERT(bb != NULL, "BytesBuilder is NULL");
    FDS_ASSERT(offset + 4 <= bb->size, "Patch offset out of bounds");
    
    bb->data[offset]     = (uint8_t)(val & 0xFF);
    bb->data[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    bb->data[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    bb->data[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}


// --- У fds_bytes_builder.h ---
FDS_BYTES void fds_bb_append_cstr(FdsBytesBuilder *bb, const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    fds_bb_append(bb, str, len + 1); // +1 для '\0'
}

FDS_BYTES void fds_bb_append_string_u16(FdsBytesBuilder *bb, const char *str) {
    if (!str) {
        fds_bb_append_u16_le(bb, 0);
        return;
    }
    size_t len = strlen(str);
    FDS_ASSERT(len <= 0xFFFF, "String too long for u16 prefix");
    fds_bb_append_u16_le(bb, (uint16_t)len);
    fds_bb_append(bb, str, len);
}

FDS_BYTES bool fds_bb_save_to_file(const FdsBytesBuilder *bb, const char *filepath) {
    FDS_ASSERT(bb != NULL, "BytesBuilder is NULL");
    FDS_ASSERT(filepath != NULL, "Filepath is NULL");
    
    if (bb->size == 0) return true; // Нічого зберігати
    
    FILE *file = fopen(filepath, "wb");
    if (!file) return false;
    
    size_t written = fwrite(bb->data, 1, bb->size, file);
    fclose(file);
    
    return written == bb->size;
}
FDS_BYTES FdsBytesView fds_bv(const void *data, size_t size) {
    return (FdsBytesView){ .data = (const uint8_t*)data, .size = size };
}

FDS_BYTES FdsBytesView fds_bv_empty(void) {
    return (FdsBytesView){ .data = NULL, .size = 0 };
}

FDS_BYTES FdsBytesView fds_bv_from_cstr(const char *str) {
    if (!str) return fds_bv_empty();
    return fds_bv(str, strlen(str));
}

FDS_BYTES FdsBytesView fds_bv_subview(FdsBytesView view, size_t offset, size_t length) {
    if (offset >= view.size) return fds_bv_empty();
    size_t available = view.size - offset;
    size_t actual_len = length < available ? length : available;
    return fds_bv(view.data + offset, actual_len);
}

FDS_BYTES FdsBytesView fds_bv_take(FdsBytesView view, size_t count) {
    return fds_bv_subview(view, 0, count);
}

FDS_BYTES FdsBytesView fds_bv_skip(FdsBytesView view, size_t count) {
    if (count >= view.size) return fds_bv_empty();
    return fds_bv(view.data + count, view.size - count);
}

FDS_BYTES bool fds_bv_equals(FdsBytesView a, FdsBytesView b) {
    if (a.size != b.size) return false;
    if (a.data == b.data) return true;
    if (!a.data || !b.data) return false;
    return memcmp(a.data, b.data, a.size) == 0;
}

FDS_BYTES bool fds_bv_has_prefix(FdsBytesView view, FdsBytesView prefix) {
    if (prefix.size > view.size) return false;
    return fds_bv_equals(fds_bv_take(view, prefix.size), prefix);
}

FDS_BYTES bool fds_bv_has_suffix(FdsBytesView view, FdsBytesView suffix) {
    if (suffix.size > view.size) return false;
    return fds_bv_equals(fds_bv_subview(view, view.size - suffix.size, suffix.size), suffix);
}

FDS_BYTES intptr_t fds_bv_find_byte(FdsBytesView view, uint8_t byte) {
    if (!view.data || view.size == 0) return -1;
    const void *ptr = memchr(view.data, byte, view.size);
    if (!ptr) return -1;
    return (intptr_t)((const uint8_t*)ptr - view.data);
}

FDS_BYTES intptr_t fds_bv_find_subview(FdsBytesView view, FdsBytesView pattern) {
    if (pattern.size == 0 || pattern.size > view.size) return -1;
    if (!view.data || !pattern.data) return -1;

    size_t max_idx = view.size - pattern.size;
    for (size_t i = 0; i <= max_idx; ++i) {
        if (memcmp(view.data + i, pattern.data, pattern.size) == 0) {
            return (intptr_t)i;
        }
    }
    return -1;
}

FDS_BYTES bool fds_bv_pop_byte(FdsBytesView *view, uint8_t *out_byte) {
    FDS_ASSERT(view != NULL, "BytesView pointer is NULL");
    if (view->size == 0 || !view->data) return false;
    if (out_byte) *out_byte = view->data[0];
    view->data++;
    view->size--;
    return true;
}

FDS_BYTES FdsBytesView fds_bv_pop_bytes(FdsBytesView *view, size_t count) {
    FDS_ASSERT(view != NULL, "BytesView pointer is NULL");
    size_t actual_count = count < view->size ? count : view->size;
    FdsBytesView result = fds_bv(view->data, actual_count);
    view->data += actual_count;
    view->size -= actual_count;
    return result;
}

FDS_BYTES bool fds_bv_read_u16_le(FdsBytesView *view, uint16_t *out_val) {
    if (view->size < 2) return false;
    if (out_val) {
        *out_val = (uint16_t)view->data[0] | ((uint16_t)view->data[1] << 8);
    }
    view->data += 2;
    view->size -= 2;
    return true;
}

FDS_BYTES bool fds_bv_read_u16_be(FdsBytesView *view, uint16_t *out_val) {
    if (view->size < 2) return false;
    if (out_val) {
        *out_val = ((uint16_t)view->data[0] << 8) | (uint16_t)view->data[1];
    }
    view->data += 2;
    view->size -= 2;
    return true;
}

FDS_BYTES bool fds_bv_read_u32_le(FdsBytesView *view, uint32_t *out_val) {
    if (view->size < 4) return false;
    if (out_val) {
        *out_val = (uint32_t)view->data[0]
                 | ((uint32_t)view->data[1] << 8)
                 | ((uint32_t)view->data[2] << 16)
                 | ((uint32_t)view->data[3] << 24);
    }
    view->data += 4;
    view->size -= 4;
    return true;
}

FDS_BYTES bool fds_bv_read_u32_be(FdsBytesView *view, uint32_t *out_val) {
    if (view->size < 4) return false;
    if (out_val) {
        *out_val = ((uint32_t)view->data[0] << 24)
                 | ((uint32_t)view->data[1] << 16)
                 | ((uint32_t)view->data[2] << 8)
                 | (uint32_t)view->data[3];
    }
    view->data += 4;
    view->size -= 4;
    return true;
}

FDS_BYTES bool fds_bv_read_u64_le(FdsBytesView *view, uint64_t *out_val) {
    if (view->size < 8) return false;
    if (out_val) {
        *out_val = (uint64_t)view->data[0]
                 | ((uint64_t)view->data[1] << 8)
                 | ((uint64_t)view->data[2] << 16)
                 | ((uint64_t)view->data[3] << 24)
                 | ((uint64_t)view->data[4] << 32)
                 | ((uint64_t)view->data[5] << 40)
                 | ((uint64_t)view->data[6] << 48)
                 | ((uint64_t)view->data[7] << 56);
    }
    view->data += 8;
    view->size -= 8;
    return true;
}

FDS_BYTES bool fds_bv_read_u64_be(FdsBytesView *view, uint64_t *out_val) {
    if (view->size < 8) return false;
    if (out_val) {
        *out_val = ((uint64_t)view->data[0] << 56)
                 | ((uint64_t)view->data[1] << 48)
                 | ((uint64_t)view->data[2] << 40)
                 | ((uint64_t)view->data[3] << 32)
                 | ((uint64_t)view->data[4] << 24)
                 | ((uint64_t)view->data[5] << 16)
                 | ((uint64_t)view->data[6] << 8)
                 | (uint64_t)view->data[7];
    }
    view->data += 8;
    view->size -= 8;
    return true;
}

FDS_BYTES bool fds_bv_read_f32_le(FdsBytesView *view, float *out_val) {
    uint32_t temp;
    if (!fds_bv_read_u32_le(view, &temp)) return false;
    if (out_val) memcpy(out_val, &temp, sizeof(float));
    return true;
}

FDS_BYTES bool fds_bv_read_f32_be(FdsBytesView *view, float *out_val) {
    uint32_t temp;
    if (!fds_bv_read_u32_be(view, &temp)) return false;
    if (out_val) memcpy(out_val, &temp, sizeof(float));
    return true;
}

FDS_BYTES bool fds_bv_read_f64_le(FdsBytesView *view, double *out_val) {
    uint64_t temp;
    if (!fds_bv_read_u64_le(view, &temp)) return false;
    if (out_val) memcpy(out_val, &temp, sizeof(double));
    return true;
}

FDS_BYTES bool fds_bv_read_f64_be(FdsBytesView *view, double *out_val) {
    uint64_t temp;
    if (!fds_bv_read_u64_be(view, &temp)) return false;
    if (out_val) memcpy(out_val, &temp, sizeof(double));
    return true;
}
FDS_BYTES bool fds_bv_read_string_u16(FdsBytesView *view, FdsBytesView *out_str_view) {
    uint16_t len;
    if (!fds_bv_read_u16_le(view, &len)) return false;
    
    if (view->size < len) return false; // Недостатньо даних
    
    if (out_str_view) {
        *out_str_view = fds_bv(view->data, len);
    }
    
    view->data += len;
    view->size -= len;
    return true;
}

#endif // FDS_BYTES_BUILDER_IMPL