#ifndef FDS2_DSL_H
#define FDS2_DSL_H

#include "fds2.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fds2_dsl_vm fds2_dsl_vm;
typedef struct fds2_dsl_type fds2_dsl_type;
typedef struct fds2_dsl_context fds2_dsl_context;
typedef struct fds2_dsl_object fds2_dsl_object;
typedef struct fds2_dsl_heap fds2_dsl_heap;
typedef struct fds2_dsl_state fds2_dsl_state;

typedef enum fds2_dsl_type_kind {
    FDS2_DSL_TYPE_VOID = 0,
    FDS2_DSL_TYPE_BOOL,
    FDS2_DSL_TYPE_I8,
    FDS2_DSL_TYPE_I16,
    FDS2_DSL_TYPE_I32,
    FDS2_DSL_TYPE_I64,
    FDS2_DSL_TYPE_U8,
    FDS2_DSL_TYPE_U16,
    FDS2_DSL_TYPE_U32,
    FDS2_DSL_TYPE_U64,
    FDS2_DSL_TYPE_F32,
    FDS2_DSL_TYPE_F64,
    FDS2_DSL_TYPE_STRING,
    FDS2_DSL_TYPE_POINTER,
    FDS2_DSL_TYPE_USER,
    FDS2_DSL_TYPE_REFERENCE,
} fds2_dsl_type_kind;

typedef struct fds2_dsl_string {
    const char *data;
    size_t length;
} fds2_dsl_string;

typedef struct fds2_dsl_value {
    const fds2_dsl_type *type;
    union {
        bool boolean;
        int64_t signed_integer;
        uint64_t unsigned_integer;
        double floating;
        fds2_dsl_string string;
        void *pointer;
        fds2_dsl_object *object;
    } as;
} fds2_dsl_value;

typedef enum fds2_dsl_object_kind {
    FDS2_DSL_OBJECT_USER,
    FDS2_DSL_OBJECT_ARRAY,
    FDS2_DSL_OBJECT_MAP,
    FDS2_DSL_OBJECT_TUPLE,
    FDS2_DSL_OBJECT_STRING,
} fds2_dsl_object_kind;

typedef void (*fds2_dsl_trace_fn)(fds2_dsl_heap *heap, fds2_dsl_object *object);
typedef void (*fds2_dsl_finalize_fn)(fds2_dsl_object *object, void *user_data);

struct fds2_dsl_object {
    fds2_dsl_object_kind kind;
    bool marked;
    size_t bytes;
    fds2_dsl_object *next;
    fds2_dsl_trace_fn trace;
    fds2_dsl_finalize_fn finalize;
    void *finalize_data;
    const fds2_dsl_type *type;
};

typedef struct fds2_dsl_array {
    fds2_dsl_object object;
    size_t count;
    fds2_dsl_value values[];
} fds2_dsl_array;

typedef struct fds2_dsl_map_entry {
    fds2_dsl_value key;
    fds2_dsl_value value;
} fds2_dsl_map_entry;

typedef struct fds2_dsl_map {
    fds2_dsl_object object;
    size_t count;
    fds2_dsl_map_entry entries[];
} fds2_dsl_map;

typedef struct fds2_dsl_tuple {
    fds2_dsl_object object;
    size_t count;
    fds2_dsl_value values[];
} fds2_dsl_tuple;

struct fds2_dsl_type {
    const char *name;
    size_t name_length;
    fds2_dsl_type_kind kind;
    size_t size;
    size_t alignment;
    bool owned_name;
    struct fds2_dsl_field *fields;
    size_t field_count;
    size_t field_capacity;
    const fds2_dsl_type *referenced_type;
};

typedef struct fds2_dsl_field {
    const char *name;
    size_t name_length;
    const fds2_dsl_type *type;
    size_t offset;
} fds2_dsl_field;

struct fds2_dsl_heap {
    fds2_allocator *allocator;
    fds2_dsl_object *objects;
    size_t bytes_allocated;
    size_t next_collection;
    size_t object_count;
};

static inline void fds2_dsl_heap_init(fds2_dsl_heap *heap, fds2_allocator *allocator) {
    if (!heap) return;
    memset(heap, 0, sizeof(*heap));
    heap->allocator = allocator ? allocator : fds2_allocator_current();
    heap->next_collection = 4096;
}

static inline void fds2_dsl_heap_mark_value(fds2_dsl_heap *heap, fds2_dsl_value value);

static inline void fds2_dsl_heap_mark_object(fds2_dsl_heap *heap, fds2_dsl_object *object) {
    if (!heap || !object || object->marked) return;
    object->marked = true;
    if (object->trace) object->trace(heap, object);
}

static inline void fds2_dsl_heap_trace_values(fds2_dsl_heap *heap, fds2_dsl_value *values, size_t count) {
    for (size_t index = 0; index < count; index++) fds2_dsl_heap_mark_value(heap, values[index]);
}

static inline void fds2_dsl_array_trace(fds2_dsl_heap *heap, fds2_dsl_object *object) {
    fds2_dsl_array *array = (fds2_dsl_array *)object;
    fds2_dsl_heap_trace_values(heap, array->values, array->count);
}

static inline void fds2_dsl_tuple_trace(fds2_dsl_heap *heap, fds2_dsl_object *object) {
    fds2_dsl_tuple *tuple = (fds2_dsl_tuple *)object;
    fds2_dsl_heap_trace_values(heap, tuple->values, tuple->count);
}

static inline void fds2_dsl_map_trace(fds2_dsl_heap *heap, fds2_dsl_object *object) {
    fds2_dsl_map *map = (fds2_dsl_map *)object;
    for (size_t index = 0; index < map->count; index++) {
        fds2_dsl_heap_mark_value(heap, map->entries[index].key);
        fds2_dsl_heap_mark_value(heap, map->entries[index].value);
    }
}

static inline void fds2_dsl_heap_mark_value(fds2_dsl_heap *heap, fds2_dsl_value value) {
    if (value.type && (value.type->kind == FDS2_DSL_TYPE_REFERENCE || value.type->kind == FDS2_DSL_TYPE_USER) && value.as.object) fds2_dsl_heap_mark_object(heap, value.as.object);
}

static inline fds2_dsl_object *fds2_dsl_heap_alloc(fds2_dsl_heap *heap,
                                                   fds2_dsl_object_kind kind,
                                                   size_t payload_size,
                                                   fds2_dsl_trace_fn trace,
                                                   fds2_dsl_finalize_fn finalize,
                                                   void *finalize_data) {
    size_t bytes;
    fds2_dsl_object *object;
    if (!heap || fds2_size_add_overflow(sizeof(*object), payload_size, &bytes)) return NULL;
    object = (fds2_dsl_object *)fds2_alloc_a(heap->allocator, bytes);
    if (!object) return NULL;
    *object = (fds2_dsl_object){ kind, false, bytes, heap->objects, trace, finalize, finalize_data, NULL };
    heap->objects = object;
    heap->bytes_allocated += bytes;
    heap->object_count++;
    return object;
}

static inline void fds2_dsl_heap_deinit(fds2_dsl_heap *heap) {
    if (!heap) return;
    fds2_dsl_object *object = heap->objects;
    while (object) {
        fds2_dsl_object *next = object->next;
        if (object->finalize) object->finalize(object, object->finalize_data);
        fds2_free_a(heap->allocator, object);
        object = next;
    }
    memset(heap, 0, sizeof(*heap));
}

static inline void fds2_dsl_heap_collect(fds2_dsl_heap *heap, const fds2_dsl_value *roots, size_t root_count) {
    fds2_dsl_object **cursor;
    if (!heap) return;
    fds2_dsl_heap_trace_values(heap, (fds2_dsl_value *)roots, root_count);
    cursor = &heap->objects;
    while (*cursor) {
        fds2_dsl_object *object = *cursor;
        if (object->marked) {
            object->marked = false;
            cursor = &object->next;
        } else {
            *cursor = object->next;
            if (object->finalize) object->finalize(object, object->finalize_data);
            heap->bytes_allocated -= object->bytes;
            heap->object_count--;
            fds2_free_a(heap->allocator, object);
        }
    }
    heap->next_collection = heap->bytes_allocated + heap->bytes_allocated / 2 + 1;
}

static inline fds2_dsl_array *fds2_dsl_array_new(fds2_dsl_heap *heap, size_t count) {
    fds2_dsl_array *array;
    size_t payload;
    if (fds2_size_mul_overflow(count, sizeof(array->values[0]), &payload)) return NULL;
    array = (fds2_dsl_array *)fds2_dsl_heap_alloc(heap, FDS2_DSL_OBJECT_ARRAY, payload, fds2_dsl_array_trace, NULL, NULL);
    if (!array) return NULL;
    array->count = count;
    for (size_t index = 0; index < count; index++) array->values[index] = (fds2_dsl_value){ 0 };
    return array;
}

static inline fds2_dsl_tuple *fds2_dsl_tuple_new(fds2_dsl_heap *heap, size_t count) {
    fds2_dsl_tuple *tuple;
    size_t payload;
    if (fds2_size_mul_overflow(count, sizeof(tuple->values[0]), &payload)) return NULL;
    tuple = (fds2_dsl_tuple *)fds2_dsl_heap_alloc(heap, FDS2_DSL_OBJECT_TUPLE, payload, fds2_dsl_tuple_trace, NULL, NULL);
    if (!tuple) return NULL;
    tuple->count = count;
    for (size_t index = 0; index < count; index++) tuple->values[index] = (fds2_dsl_value){ 0 };
    return tuple;
}

static inline fds2_dsl_map *fds2_dsl_map_new(fds2_dsl_heap *heap, size_t count) {
    fds2_dsl_map *map;
    size_t payload;
    if (fds2_size_mul_overflow(count, sizeof(map->entries[0]), &payload)) return NULL;
    map = (fds2_dsl_map *)fds2_dsl_heap_alloc(heap, FDS2_DSL_OBJECT_MAP, payload, fds2_dsl_map_trace, NULL, NULL);
    if (!map) return NULL;
    map->count = count;
    for (size_t index = 0; index < count; index++) map->entries[index] = (fds2_dsl_map_entry){ { 0 }, { 0 } };
    return map;
}

static inline fds2_dsl_object *fds2_dsl_user_object_new(fds2_dsl_heap *heap,
                                                         const fds2_dsl_type *type,
                                                         fds2_dsl_finalize_fn finalize,
                                                         void *finalize_data) {
    fds2_dsl_object *object;
    if (!heap || !type || type->kind != FDS2_DSL_TYPE_USER) return NULL;
    object = fds2_dsl_heap_alloc(heap, FDS2_DSL_OBJECT_USER, type->size, NULL, finalize, finalize_data);
    if (object) object->type = type;
    return object;
}

typedef enum fds2_dsl_error {
    FDS2_DSL_OK = 0,
    FDS2_DSL_ERROR_BAD_ARGUMENT,
    FDS2_DSL_ERROR_OUT_OF_MEMORY,
    FDS2_DSL_ERROR_UNKNOWN_TYPE,
    FDS2_DSL_ERROR_TYPE_MISMATCH,
    FDS2_DSL_ERROR_BAD_ARITY,
    FDS2_DSL_ERROR_NATIVE,
} fds2_dsl_error;

static inline const char *fds2_dsl_error_string(fds2_dsl_error error) {
    switch (error) {
        case FDS2_DSL_OK: return "ok";
        case FDS2_DSL_ERROR_BAD_ARGUMENT: return "bad argument";
        case FDS2_DSL_ERROR_OUT_OF_MEMORY: return "out of memory";
        case FDS2_DSL_ERROR_UNKNOWN_TYPE: return "unknown type";
        case FDS2_DSL_ERROR_TYPE_MISMATCH: return "type mismatch";
        case FDS2_DSL_ERROR_BAD_ARITY: return "bad argument count";
        case FDS2_DSL_ERROR_NATIVE: return "native function failed";
    }
    return "unknown error";
}

typedef enum fds2_dsl_token_kind {
    FDS2_DSL_TOKEN_EOF = 0,
    FDS2_DSL_TOKEN_ERROR,
    FDS2_DSL_TOKEN_IDENTIFIER,
    FDS2_DSL_TOKEN_INTEGER,
    FDS2_DSL_TOKEN_FLOAT,
    FDS2_DSL_TOKEN_STRING,
    FDS2_DSL_TOKEN_FN,
    FDS2_DSL_TOKEN_RETURN,
    FDS2_DSL_TOKEN_IF,
    FDS2_DSL_TOKEN_ELSE,
    FDS2_DSL_TOKEN_WHILE,
    FDS2_DSL_TOKEN_TRUE,
    FDS2_DSL_TOKEN_FALSE,
    FDS2_DSL_TOKEN_LEFT_PAREN,
    FDS2_DSL_TOKEN_RIGHT_PAREN,
    FDS2_DSL_TOKEN_LEFT_BRACE,
    FDS2_DSL_TOKEN_RIGHT_BRACE,
    FDS2_DSL_TOKEN_COMMA,
    FDS2_DSL_TOKEN_SEMICOLON,
    FDS2_DSL_TOKEN_PLUS,
    FDS2_DSL_TOKEN_MINUS,
    FDS2_DSL_TOKEN_STAR,
    FDS2_DSL_TOKEN_SLASH,
    FDS2_DSL_TOKEN_EQUAL,
    FDS2_DSL_TOKEN_EQUAL_EQUAL,
    FDS2_DSL_TOKEN_BANG_EQUAL,
    FDS2_DSL_TOKEN_LESS,
    FDS2_DSL_TOKEN_LESS_EQUAL,
    FDS2_DSL_TOKEN_GREATER,
    FDS2_DSL_TOKEN_GREATER_EQUAL,
    FDS2_DSL_TOKEN_AND_AND,
    FDS2_DSL_TOKEN_OR_OR,
    FDS2_DSL_TOKEN_BANG,
} fds2_dsl_token_kind;

typedef struct fds2_dsl_token {
    fds2_dsl_token_kind kind;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
    int64_t integer;
    double floating;
} fds2_dsl_token;

typedef struct fds2_dsl_lexer {
    const char *current;
    size_t line;
    size_t column;
} fds2_dsl_lexer;

static inline bool fds2_dsl_read_file(fds2_allocator *allocator,
                                      const char *path,
                                      char **source,
                                      size_t *length) {
    FILE *file;
    long file_size;
    char *buffer;
    size_t read_count;
    if (!path || !source) return false;
    *source = NULL;
    if (length) *length = 0;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return false; }
    file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return false; }
    if ((size_t)file_size == SIZE_MAX) { fclose(file); return false; }
    buffer = (char *)fds2_alloc_a(allocator, (size_t)file_size + 1);
    if (!buffer) { fclose(file); return false; }
    read_count = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    if (read_count != (size_t)file_size) {
        fds2_free_a(allocator, buffer);
        return false;
    }
    buffer[read_count] = '\0';
    *source = buffer;
    if (length) *length = read_count;
    return true;
}

static inline void fds2_dsl_lexer_init(fds2_dsl_lexer *lexer, const char *source) {
    if (!lexer) return;
    lexer->current = source ? source : "";
    lexer->line = 1;
    lexer->column = 1;
}

static inline bool fds2_dsl_lexer_is_alpha(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';
}

static inline bool fds2_dsl_lexer_is_digit(char character) {
    return character >= '0' && character <= '9';
}

static inline fds2_dsl_token fds2_dsl_lexer_token(const fds2_dsl_lexer *lexer,
                                                   fds2_dsl_token_kind kind,
                                                   const char *start,
                                                   size_t line,
                                                   size_t column) {
    return (fds2_dsl_token){ kind, start, (size_t)(lexer->current - start), line, column, 0, 0.0 };
}

static inline fds2_dsl_token fds2_dsl_lexer_next(fds2_dsl_lexer *lexer) {
    const char *start;
    size_t line;
    size_t column;
    if (!lexer) return (fds2_dsl_token){ FDS2_DSL_TOKEN_ERROR, NULL, 0, 0, 0, 0, 0.0 };
    for (;;) {
        while (*lexer->current == ' ' || *lexer->current == '\t' || *lexer->current == '\r' || *lexer->current == '\n') {
            if (*lexer->current++ == '\n') { lexer->line++; lexer->column = 1; }
            else lexer->column++;
        }
        if (lexer->current[0] != '/' || lexer->current[1] != '/') break;
        while (*lexer->current && *lexer->current != '\n') { lexer->current++; lexer->column++; }
    }
    start = lexer->current;
    line = lexer->line;
    column = lexer->column;
    if (!*start) return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_EOF, start, line, column);
    if (fds2_dsl_lexer_is_alpha(*start)) {
        while (fds2_dsl_lexer_is_alpha(*lexer->current) || fds2_dsl_lexer_is_digit(*lexer->current)) { lexer->current++; lexer->column++; }
        fds2_dsl_token token = fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_IDENTIFIER, start, line, column);
        if (token.length == 2 && memcmp(start, "fn", 2) == 0) token.kind = FDS2_DSL_TOKEN_FN;
        else if (token.length == 6 && memcmp(start, "return", 6) == 0) token.kind = FDS2_DSL_TOKEN_RETURN;
        else if (token.length == 2 && memcmp(start, "if", 2) == 0) token.kind = FDS2_DSL_TOKEN_IF;
        else if (token.length == 4 && memcmp(start, "else", 4) == 0) token.kind = FDS2_DSL_TOKEN_ELSE;
        else if (token.length == 5 && memcmp(start, "while", 5) == 0) token.kind = FDS2_DSL_TOKEN_WHILE;
        else if (token.length == 4 && memcmp(start, "true", 4) == 0) token.kind = FDS2_DSL_TOKEN_TRUE;
        else if (token.length == 5 && memcmp(start, "false", 5) == 0) token.kind = FDS2_DSL_TOKEN_FALSE;
        return token;
    }
    if (fds2_dsl_lexer_is_digit(*start)) {
        int64_t integer = 0;
        while (fds2_dsl_lexer_is_digit(*lexer->current)) { integer = integer * 10 + (*lexer->current++ - '0'); lexer->column++; }
        if (*lexer->current == '.' && fds2_dsl_lexer_is_digit(lexer->current[1])) {
            double fraction = 0.0;
            double divisor = 1.0;
            lexer->current++; lexer->column++;
            while (fds2_dsl_lexer_is_digit(*lexer->current)) { fraction = fraction * 10.0 + (*lexer->current++ - '0'); divisor *= 10.0; lexer->column++; }
            fds2_dsl_token token = fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_FLOAT, start, line, column);
            token.floating = (double)integer + fraction / divisor;
            return token;
        }
        fds2_dsl_token token = fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_INTEGER, start, line, column);
        token.integer = integer;
        return token;
    }
    if (*start == '"') {
        lexer->current++; lexer->column++; start = lexer->current;
        while (*lexer->current && *lexer->current != '"' && *lexer->current != '\n') { lexer->current++; lexer->column++; }
        if (*lexer->current != '"') return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_ERROR, start, line, column);
        fds2_dsl_token token = fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_STRING, start, line, column);
        lexer->current++; lexer->column++;
        return token;
    }
    lexer->current++; lexer->column++;
    if ((*start == '=' || *start == '!' || *start == '<' || *start == '>' || *start == '&' || *start == '|') && lexer->current[0] == *start) {
        lexer->current++; lexer->column++;
        if (*start == '=') return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_EQUAL_EQUAL, start, line, column);
        if (*start == '!') return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_BANG_EQUAL, start, line, column);
        if (*start == '<') return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_LESS_EQUAL, start, line, column);
        if (*start == '>') return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_GREATER_EQUAL, start, line, column);
        if (*start == '&') return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_AND_AND, start, line, column);
        return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_OR_OR, start, line, column);
    }
    switch (*start) {
        case '(': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_LEFT_PAREN, start, line, column);
        case ')': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_RIGHT_PAREN, start, line, column);
        case '{': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_LEFT_BRACE, start, line, column);
        case '}': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_RIGHT_BRACE, start, line, column);
        case ',': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_COMMA, start, line, column);
        case ';': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_SEMICOLON, start, line, column);
        case '+': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_PLUS, start, line, column);
        case '-': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_MINUS, start, line, column);
        case '*': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_STAR, start, line, column);
        case '/': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_SLASH, start, line, column);
        case '=': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_EQUAL, start, line, column);
        case '<': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_LESS, start, line, column);
        case '>': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_GREATER, start, line, column);
        case '!': return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_BANG, start, line, column);
        default: return fds2_dsl_lexer_token(lexer, FDS2_DSL_TOKEN_ERROR, start, line, column);
    }
}

typedef enum fds2_dsl_node_kind {
    FDS2_DSL_NODE_PROGRAM,
    FDS2_DSL_NODE_FUNCTION,
    FDS2_DSL_NODE_BLOCK,
    FDS2_DSL_NODE_VARIABLE,
    FDS2_DSL_NODE_ASSIGN,
    FDS2_DSL_NODE_RETURN,
    FDS2_DSL_NODE_IF,
    FDS2_DSL_NODE_WHILE,
    FDS2_DSL_NODE_EXPRESSION,
    FDS2_DSL_NODE_BINARY,
    FDS2_DSL_NODE_UNARY,
    FDS2_DSL_NODE_CALL,
    FDS2_DSL_NODE_IDENTIFIER,
    FDS2_DSL_NODE_INTEGER,
    FDS2_DSL_NODE_FLOAT,
    FDS2_DSL_NODE_STRING,
    FDS2_DSL_NODE_BOOLEAN,
} fds2_dsl_node_kind;

typedef struct fds2_dsl_node fds2_dsl_node;

typedef struct fds2_dsl_parameter {
    fds2_dsl_token type;
    fds2_dsl_token name;
} fds2_dsl_parameter;

struct fds2_dsl_node {
    fds2_dsl_node_kind kind;
    fds2_dsl_token token;
    union {
        struct { fds2_dsl_node **items; size_t count; } list;
        struct { fds2_dsl_token return_type; fds2_dsl_token name; fds2_dsl_parameter *parameters; size_t parameter_count; fds2_dsl_node *body; } function;
        struct { fds2_dsl_token type; fds2_dsl_token name; fds2_dsl_node *initializer; } variable;
        struct { fds2_dsl_node *condition; fds2_dsl_node *then_branch; fds2_dsl_node *else_branch; } conditional;
        struct { fds2_dsl_node *condition; fds2_dsl_node *body; } loop;
        struct { fds2_dsl_node *left; fds2_dsl_node *right; } binary;
        struct { fds2_dsl_node *value; } unary;
        struct { fds2_dsl_node **values; size_t count; } returns;
        struct { fds2_dsl_token name; fds2_dsl_node **arguments; size_t argument_count; } call;
        struct { int64_t integer; double floating; bool boolean; } literal;
    } as;
};

typedef enum fds2_dsl_parse_error {
    FDS2_DSL_PARSE_OK = 0,
    FDS2_DSL_PARSE_UNEXPECTED_TOKEN,
    FDS2_DSL_PARSE_EXPECTED_TYPE,
    FDS2_DSL_PARSE_EXPECTED_NAME,
    FDS2_DSL_PARSE_OUT_OF_MEMORY,
} fds2_dsl_parse_error;

typedef struct fds2_dsl_parser {
    fds2_allocator *allocator;
    fds2_dsl_lexer lexer;
    fds2_dsl_token current;
    fds2_dsl_token next;
    fds2_dsl_parse_error error;
    const char *error_message;
    size_t error_line;
    size_t error_column;
} fds2_dsl_parser;

static inline void fds2_dsl_parser_fail(fds2_dsl_parser *parser,
                                        fds2_dsl_parse_error error,
                                        const char *message) {
    if (parser->error == FDS2_DSL_PARSE_OK) {
        parser->error = error;
        parser->error_message = message;
        parser->error_line = parser->current.line;
        parser->error_column = parser->current.column;
    }
}

static inline void fds2_dsl_parser_advance(fds2_dsl_parser *parser) {
    parser->current = parser->next;
    parser->next = fds2_dsl_lexer_next(&parser->lexer);
}

static inline bool fds2_dsl_parser_match(fds2_dsl_parser *parser, fds2_dsl_token_kind kind) {
    if (parser->current.kind != kind) return false;
    fds2_dsl_parser_advance(parser);
    return true;
}

static inline bool fds2_dsl_parser_expect(fds2_dsl_parser *parser,
                                          fds2_dsl_token_kind kind,
                                          const char *message) {
    if (fds2_dsl_parser_match(parser, kind)) return true;
    fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_UNEXPECTED_TOKEN, message);
    return false;
}

static inline fds2_dsl_node *fds2_dsl_parser_node(fds2_dsl_parser *parser,
                                                  fds2_dsl_node_kind kind,
                                                  fds2_dsl_token token) {
    fds2_dsl_node *node = (fds2_dsl_node *)fds2_alloc_a(parser->allocator, sizeof(*node));
    if (!node) {
        fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "parser out of memory");
        return NULL;
    }
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->token = token;
    return node;
}

static inline bool fds2_dsl_parser_push_node(fds2_dsl_parser *parser,
                                             fds2_dsl_node ***items,
                                             size_t *count,
                                             fds2_dsl_node *node) {
    fds2_dsl_node **grown;
    size_t capacity = *count ? *count * 2 : 8;
    if (*count && capacity < *count) return false;
    if (*count == 0 || (*count & (*count - 1)) == 0) {
        if (fds2_size_mul_overflow(capacity, sizeof(*grown), NULL)) return false;
        grown = (fds2_dsl_node **)fds2_realloc_a(parser->allocator, *items, capacity * sizeof(*grown));
        if (!grown) return false;
        *items = grown;
    }
    (*items)[(*count)++] = node;
    return true;
}

static inline fds2_dsl_node *fds2_dsl_parse_expression(fds2_dsl_parser *parser);

static inline fds2_dsl_node *fds2_dsl_parse_primary(fds2_dsl_parser *parser) {
    fds2_dsl_token token = parser->current;
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_INTEGER)) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_INTEGER, token);
        if (node) node->as.literal.integer = token.integer;
        return node;
    }
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_FLOAT)) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_FLOAT, token);
        if (node) node->as.literal.floating = token.floating;
        return node;
    }
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_STRING)) return fds2_dsl_parser_node(parser, FDS2_DSL_NODE_STRING, token);
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_TRUE) || fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_FALSE)) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_BOOLEAN, token);
        if (node) node->as.literal.boolean = token.kind == FDS2_DSL_TOKEN_TRUE;
        return node;
    }
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_LEFT_PAREN)) {
        fds2_dsl_node *node = fds2_dsl_parse_expression(parser);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_RIGHT_PAREN, "expected ')' after expression");
        return node;
    }
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_IDENTIFIER)) {
        if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_LEFT_PAREN)) {
            fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_CALL, token);
            if (!node) return NULL;
            while (parser->current.kind != FDS2_DSL_TOKEN_RIGHT_PAREN && parser->current.kind != FDS2_DSL_TOKEN_EOF) {
                fds2_dsl_node *argument = fds2_dsl_parse_expression(parser);
                if (!argument || !fds2_dsl_parser_push_node(parser, &node->as.call.arguments, &node->as.call.argument_count, argument)) {
                    fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "argument list out of memory");
                    return node;
                }
                if (!fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_COMMA)) break;
            }
            fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_RIGHT_PAREN, "expected ')' after arguments");
            node->as.call.name = token;
            return node;
        }
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_IDENTIFIER, token);
        return node;
    }
    fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_UNEXPECTED_TOKEN, "expected expression");
    return NULL;
}

static inline fds2_dsl_node *fds2_dsl_parse_unary(fds2_dsl_parser *parser) {
    if (parser->current.kind == FDS2_DSL_TOKEN_MINUS || parser->current.kind == FDS2_DSL_TOKEN_BANG) {
        fds2_dsl_token token = parser->current;
        fds2_dsl_parser_advance(parser);
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_UNARY, token);
        if (node) node->as.unary.value = fds2_dsl_parse_unary(parser);
        return node;
    }
    return fds2_dsl_parse_primary(parser);
}

static inline fds2_dsl_node *fds2_dsl_parse_binary_level(fds2_dsl_parser *parser,
                                                         fds2_dsl_node *left,
                                                         fds2_dsl_token_kind first,
                                                         fds2_dsl_token_kind last) {
    while (parser->current.kind >= first && parser->current.kind <= last) {
        fds2_dsl_token operator_token = parser->current;
        fds2_dsl_parser_advance(parser);
        fds2_dsl_node *right = fds2_dsl_parse_unary(parser);
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_BINARY, operator_token);
        if (!node) return left;
        node->as.binary.left = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

static inline int fds2_dsl_binary_precedence(fds2_dsl_token_kind kind) {
    switch (kind) {
        case FDS2_DSL_TOKEN_OR_OR: return 1;
        case FDS2_DSL_TOKEN_AND_AND: return 2;
        case FDS2_DSL_TOKEN_EQUAL_EQUAL:
        case FDS2_DSL_TOKEN_BANG_EQUAL: return 3;
        case FDS2_DSL_TOKEN_LESS:
        case FDS2_DSL_TOKEN_LESS_EQUAL:
        case FDS2_DSL_TOKEN_GREATER:
        case FDS2_DSL_TOKEN_GREATER_EQUAL: return 4;
        case FDS2_DSL_TOKEN_PLUS:
        case FDS2_DSL_TOKEN_MINUS: return 5;
        case FDS2_DSL_TOKEN_STAR:
        case FDS2_DSL_TOKEN_SLASH: return 6;
        default: return 0;
    }
}

static inline fds2_dsl_node *fds2_dsl_parse_binary(fds2_dsl_parser *parser, int minimum_precedence) {
    fds2_dsl_node *left = fds2_dsl_parse_unary(parser);
    while (fds2_dsl_binary_precedence(parser->current.kind) >= minimum_precedence) {
        fds2_dsl_token operator_token = parser->current;
        int precedence = fds2_dsl_binary_precedence(operator_token.kind);
        fds2_dsl_parser_advance(parser);
        fds2_dsl_node *right = fds2_dsl_parse_binary(parser, precedence + 1);
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_BINARY, operator_token);
        if (!node) return left;
        node->as.binary.left = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

static inline fds2_dsl_node *fds2_dsl_parse_expression(fds2_dsl_parser *parser) {
    return fds2_dsl_parse_binary(parser, 1);
}

static inline fds2_dsl_node *fds2_dsl_parse_statement(fds2_dsl_parser *parser);

static inline fds2_dsl_node *fds2_dsl_parse_block(fds2_dsl_parser *parser) {
    fds2_dsl_node *block = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_BLOCK, parser->current);
    if (!block) return NULL;
    if (!fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_LEFT_BRACE, "expected '{'")) return block;
    while (parser->current.kind != FDS2_DSL_TOKEN_RIGHT_BRACE && parser->current.kind != FDS2_DSL_TOKEN_EOF) {
        fds2_dsl_node *statement = fds2_dsl_parse_statement(parser);
        if (!statement || !fds2_dsl_parser_push_node(parser, &block->as.list.items, &block->as.list.count, statement)) {
            fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "block out of memory");
            return block;
        }
    }
    fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_RIGHT_BRACE, "expected '}'");
    return block;
}

static inline fds2_dsl_node *fds2_dsl_parse_statement(fds2_dsl_parser *parser) {
    fds2_dsl_token token = parser->current;
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_RETURN)) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_RETURN, token);
        if (parser->current.kind != FDS2_DSL_TOKEN_SEMICOLON) {
            fds2_dsl_node *value = fds2_dsl_parse_expression(parser);
            if (!value || !fds2_dsl_parser_push_node(parser, &node->as.returns.values, &node->as.returns.count, value)) fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "return values out of memory");
            while (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_COMMA)) {
                value = fds2_dsl_parse_expression(parser);
                if (!value || !fds2_dsl_parser_push_node(parser, &node->as.returns.values, &node->as.returns.count, value)) { fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "return values out of memory"); break; }
            }
        }
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_SEMICOLON, "expected ';' after return");
        return node;
    }
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_IF)) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_IF, token);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_LEFT_PAREN, "expected '(' after if");
        node->as.conditional.condition = fds2_dsl_parse_expression(parser);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_RIGHT_PAREN, "expected ')' after if condition");
        node->as.conditional.then_branch = fds2_dsl_parse_block(parser);
        if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_ELSE)) node->as.conditional.else_branch = fds2_dsl_parse_block(parser);
        return node;
    }
    if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_WHILE)) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_WHILE, token);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_LEFT_PAREN, "expected '(' after while");
        node->as.loop.condition = fds2_dsl_parse_expression(parser);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_RIGHT_PAREN, "expected ')' after while condition");
        node->as.loop.body = fds2_dsl_parse_block(parser);
        return node;
    }
    if (parser->current.kind == FDS2_DSL_TOKEN_LEFT_BRACE) return fds2_dsl_parse_block(parser);
    if (parser->current.kind == FDS2_DSL_TOKEN_IDENTIFIER && parser->next.kind == FDS2_DSL_TOKEN_EQUAL) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_ASSIGN, parser->current);
        node->as.binary.left = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_IDENTIFIER, parser->current);
        fds2_dsl_parser_advance(parser);
        fds2_dsl_parser_advance(parser);
        node->as.binary.right = fds2_dsl_parse_expression(parser);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_SEMICOLON, "expected ';' after assignment");
        return node;
    }
    if (parser->current.kind == FDS2_DSL_TOKEN_IDENTIFIER && parser->next.kind == FDS2_DSL_TOKEN_IDENTIFIER) {
        fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_VARIABLE, parser->current);
        node->as.variable.type = parser->current;
        fds2_dsl_parser_advance(parser);
        node->as.variable.name = parser->current;
        fds2_dsl_parser_advance(parser);
        if (fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_EQUAL)) node->as.variable.initializer = fds2_dsl_parse_expression(parser);
        fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_SEMICOLON, "expected ';' after declaration");
        return node;
    }
    fds2_dsl_node *node = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_EXPRESSION, token);
    if (node) node->as.unary.value = fds2_dsl_parse_expression(parser);
    fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_SEMICOLON, "expected ';' after expression");
    return node;
}

static inline fds2_dsl_node *fds2_dsl_parse_function(fds2_dsl_parser *parser) {
    fds2_dsl_token function_token = parser->current;
    fds2_dsl_node *function;
    fds2_dsl_parser_advance(parser);
    if (parser->current.kind != FDS2_DSL_TOKEN_IDENTIFIER) {
        fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_EXPECTED_TYPE, "expected return type");
        return NULL;
    }
    function = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_FUNCTION, function_token);
    if (!function) return NULL;
    function->as.function.return_type = parser->current;
    fds2_dsl_parser_advance(parser);
    if (parser->current.kind != FDS2_DSL_TOKEN_IDENTIFIER) {
        fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_EXPECTED_NAME, "expected function name");
        return function;
    }
    function->as.function.name = parser->current;
    fds2_dsl_parser_advance(parser);
    fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_LEFT_PAREN, "expected '(' after function name");
    while (parser->current.kind != FDS2_DSL_TOKEN_RIGHT_PAREN && parser->current.kind != FDS2_DSL_TOKEN_EOF) {
        fds2_dsl_parameter *parameters;
        if (parser->current.kind != FDS2_DSL_TOKEN_IDENTIFIER || parser->next.kind != FDS2_DSL_TOKEN_IDENTIFIER) {
            fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_EXPECTED_TYPE, "expected parameter type and name");
            return function;
        }
        parameters = (fds2_dsl_parameter *)fds2_realloc_a(parser->allocator, function->as.function.parameters,
            (function->as.function.parameter_count + 1) * sizeof(*parameters));
        if (!parameters) {
            fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "parameter list out of memory");
            return function;
        }
        function->as.function.parameters = parameters;
        parameters[function->as.function.parameter_count].type = parser->current;
        fds2_dsl_parser_advance(parser);
        parameters[function->as.function.parameter_count].name = parser->current;
        function->as.function.parameter_count++;
        fds2_dsl_parser_advance(parser);
        if (!fds2_dsl_parser_match(parser, FDS2_DSL_TOKEN_COMMA)) break;
    }
    fds2_dsl_parser_expect(parser, FDS2_DSL_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
    function->as.function.body = fds2_dsl_parse_block(parser);
    return function;
}

static inline void fds2_dsl_parser_init(fds2_dsl_parser *parser,
                                        fds2_allocator *allocator,
                                        const char *source) {
    memset(parser, 0, sizeof(*parser));
    parser->allocator = allocator ? allocator : fds2_allocator_current();
    fds2_dsl_lexer_init(&parser->lexer, source);
    parser->current = fds2_dsl_lexer_next(&parser->lexer);
    parser->next = fds2_dsl_lexer_next(&parser->lexer);
}

static inline fds2_dsl_node *fds2_dsl_parse(fds2_dsl_parser *parser) {
    fds2_dsl_node *program = fds2_dsl_parser_node(parser, FDS2_DSL_NODE_PROGRAM, parser->current);
    if (!program) return NULL;
    while (parser->current.kind != FDS2_DSL_TOKEN_EOF && parser->error == FDS2_DSL_PARSE_OK) {
        fds2_dsl_node *item = parser->current.kind == FDS2_DSL_TOKEN_FN ? fds2_dsl_parse_function(parser) : fds2_dsl_parse_statement(parser);
        if (!item || !fds2_dsl_parser_push_node(parser, &program->as.list.items, &program->as.list.count, item)) {
            fds2_dsl_parser_fail(parser, FDS2_DSL_PARSE_OUT_OF_MEMORY, "program out of memory");
            break;
        }
    }
    return program;
}

static inline void fds2_dsl_node_delete(fds2_allocator *allocator, fds2_dsl_node *node) {
    if (!node) return;
    switch (node->kind) {
        case FDS2_DSL_NODE_PROGRAM:
        case FDS2_DSL_NODE_BLOCK:
            for (size_t index = 0; index < node->as.list.count; index++) fds2_dsl_node_delete(allocator, node->as.list.items[index]);
            fds2_free_a(allocator, node->as.list.items);
            break;
        case FDS2_DSL_NODE_FUNCTION:
            fds2_free_a(allocator, node->as.function.parameters);
            fds2_dsl_node_delete(allocator, node->as.function.body);
            break;
        case FDS2_DSL_NODE_VARIABLE: fds2_dsl_node_delete(allocator, node->as.variable.initializer); break;
        case FDS2_DSL_NODE_ASSIGN:
            fds2_dsl_node_delete(allocator, node->as.binary.left);
            fds2_dsl_node_delete(allocator, node->as.binary.right);
            break;
        case FDS2_DSL_NODE_RETURN:
            for (size_t index = 0; index < node->as.returns.count; index++) fds2_dsl_node_delete(allocator, node->as.returns.values[index]);
            fds2_free_a(allocator, node->as.returns.values);
            break;
        case FDS2_DSL_NODE_EXPRESSION:
        case FDS2_DSL_NODE_UNARY: fds2_dsl_node_delete(allocator, node->as.unary.value); break;
        case FDS2_DSL_NODE_IF:
            fds2_dsl_node_delete(allocator, node->as.conditional.condition);
            fds2_dsl_node_delete(allocator, node->as.conditional.then_branch);
            fds2_dsl_node_delete(allocator, node->as.conditional.else_branch);
            break;
        case FDS2_DSL_NODE_WHILE:
            fds2_dsl_node_delete(allocator, node->as.loop.condition);
            fds2_dsl_node_delete(allocator, node->as.loop.body);
            break;
        case FDS2_DSL_NODE_BINARY:
            fds2_dsl_node_delete(allocator, node->as.binary.left);
            fds2_dsl_node_delete(allocator, node->as.binary.right);
            break;
        case FDS2_DSL_NODE_CALL:
            for (size_t index = 0; index < node->as.call.argument_count; index++) fds2_dsl_node_delete(allocator, node->as.call.arguments[index]);
            fds2_free_a(allocator, node->as.call.arguments);
            break;
        default: break;
    }
    fds2_free_a(allocator, node);
}

typedef bool (*fds2_dsl_native_wrapper_fn)(fds2_dsl_vm *vm,
                                            const fds2_dsl_value *arguments,
                                            size_t argument_count,
                                            fds2_dsl_value *result,
                                            void *user_data);

typedef struct fds2_dsl_native {
    const char *name;
    size_t name_length;
    const fds2_dsl_type *return_type;
    const fds2_dsl_type **argument_types;
    size_t argument_count;
    fds2_dsl_native_wrapper_fn wrapper;
    void *user_data;
    bool owned_name;
} fds2_dsl_native;

typedef struct fds2_dsl_native_definition {
    const char *name;
    const fds2_dsl_type *return_type;
    const fds2_dsl_type *const *argument_types;
    size_t argument_count;
    fds2_dsl_native_wrapper_fn wrapper;
    void *user_data;
} fds2_dsl_native_definition;

struct fds2_dsl_context {
    fds2_allocator *allocator;
    fds2_dsl_heap heap;
    fds2_dsl_type *types;
    size_t type_count;
    size_t type_capacity;
    fds2_dsl_native *natives;
    size_t native_count;
    size_t native_capacity;
    fds2_dsl_type builtin_void;
    fds2_dsl_type builtin_bool;
    fds2_dsl_type builtin_i8;
    fds2_dsl_type builtin_i16;
    fds2_dsl_type builtin_i32;
    fds2_dsl_type builtin_i64;
    fds2_dsl_type builtin_u8;
    fds2_dsl_type builtin_u16;
    fds2_dsl_type builtin_u32;
    fds2_dsl_type builtin_u64;
    fds2_dsl_type builtin_f32;
    fds2_dsl_type builtin_f64;
    fds2_dsl_type builtin_string;
    fds2_dsl_type builtin_pointer;
    fds2_dsl_type builtin_tuple;
};

static inline fds2_dsl_type fds2_dsl_make_builtin(const char *name,
                                                   fds2_dsl_type_kind kind,
                                                   size_t size,
                                                   size_t alignment) {
    return (fds2_dsl_type){ name, strlen(name), kind, size, alignment, false, NULL, 0, 0, NULL };
}

static inline void fds2_dsl_context_init(fds2_dsl_context *context, fds2_allocator *allocator) {
    if (!context) return;
    memset(context, 0, sizeof(*context));
    context->allocator = allocator ? allocator : fds2_allocator_current();
    fds2_dsl_heap_init(&context->heap, context->allocator);
    context->builtin_void = fds2_dsl_make_builtin("void", FDS2_DSL_TYPE_VOID, 0, 1);
    context->builtin_bool = fds2_dsl_make_builtin("bool", FDS2_DSL_TYPE_BOOL, sizeof(bool), _Alignof(bool));
    context->builtin_i8 = fds2_dsl_make_builtin("int8", FDS2_DSL_TYPE_I8, sizeof(int8_t), _Alignof(int8_t));
    context->builtin_i16 = fds2_dsl_make_builtin("int16", FDS2_DSL_TYPE_I16, sizeof(int16_t), _Alignof(int16_t));
    context->builtin_i32 = fds2_dsl_make_builtin("int32", FDS2_DSL_TYPE_I32, sizeof(int32_t), _Alignof(int32_t));
    context->builtin_i64 = fds2_dsl_make_builtin("int64", FDS2_DSL_TYPE_I64, sizeof(int64_t), _Alignof(int64_t));
    context->builtin_u8 = fds2_dsl_make_builtin("uint8", FDS2_DSL_TYPE_U8, sizeof(uint8_t), _Alignof(uint8_t));
    context->builtin_u16 = fds2_dsl_make_builtin("uint16", FDS2_DSL_TYPE_U16, sizeof(uint16_t), _Alignof(uint16_t));
    context->builtin_u32 = fds2_dsl_make_builtin("uint32", FDS2_DSL_TYPE_U32, sizeof(uint32_t), _Alignof(uint32_t));
    context->builtin_u64 = fds2_dsl_make_builtin("uint64", FDS2_DSL_TYPE_U64, sizeof(uint64_t), _Alignof(uint64_t));
    context->builtin_f32 = fds2_dsl_make_builtin("float32", FDS2_DSL_TYPE_F32, sizeof(float), _Alignof(float));
    context->builtin_f64 = fds2_dsl_make_builtin("float64", FDS2_DSL_TYPE_F64, sizeof(double), _Alignof(double));
    context->builtin_string = fds2_dsl_make_builtin("string", FDS2_DSL_TYPE_STRING, sizeof(fds2_dsl_string), _Alignof(fds2_dsl_string));
    context->builtin_pointer = fds2_dsl_make_builtin("pointer", FDS2_DSL_TYPE_POINTER, sizeof(void *), _Alignof(void *));
    context->builtin_tuple = fds2_dsl_make_builtin("tuple", FDS2_DSL_TYPE_USER, sizeof(fds2_dsl_tuple), _Alignof(fds2_dsl_tuple));
}

static inline void fds2_dsl_context_deinit(fds2_dsl_context *context) {
    if (!context) return;
    for (size_t index = 0; index < context->type_count; index++) {
        if (context->types[index].owned_name) fds2_free_a(context->allocator, (void *)context->types[index].name);
        for (size_t field = 0; field < context->types[index].field_count; field++) fds2_free_a(context->allocator, (void *)context->types[index].fields[field].name);
        fds2_free_a(context->allocator, context->types[index].fields);
    }
    fds2_free_a(context->allocator, context->types);
    for (size_t index = 0; index < context->native_count; index++) {
        if (context->natives[index].owned_name) fds2_free_a(context->allocator, (void *)context->natives[index].name);
        fds2_free_a(context->allocator, (void *)context->natives[index].argument_types);
    }
    fds2_free_a(context->allocator, context->natives);
    fds2_dsl_heap_deinit(&context->heap);
    memset(context, 0, sizeof(*context));
}

static inline bool fds2_dsl_name_equal(const char *left, size_t left_length,
                                       const char *right, size_t right_length) {
    return left && right && left_length == right_length && memcmp(left, right, left_length) == 0;
}

static inline const fds2_dsl_type *fds2_dsl_builtin_type(fds2_dsl_context *context,
                                                          fds2_dsl_type_kind kind) {
    if (!context) return NULL;
    switch (kind) {
        case FDS2_DSL_TYPE_VOID: return &context->builtin_void;
        case FDS2_DSL_TYPE_BOOL: return &context->builtin_bool;
        case FDS2_DSL_TYPE_I8: return &context->builtin_i8;
        case FDS2_DSL_TYPE_I16: return &context->builtin_i16;
        case FDS2_DSL_TYPE_I32: return &context->builtin_i32;
        case FDS2_DSL_TYPE_I64: return &context->builtin_i64;
        case FDS2_DSL_TYPE_U8: return &context->builtin_u8;
        case FDS2_DSL_TYPE_U16: return &context->builtin_u16;
        case FDS2_DSL_TYPE_U32: return &context->builtin_u32;
        case FDS2_DSL_TYPE_U64: return &context->builtin_u64;
        case FDS2_DSL_TYPE_F32: return &context->builtin_f32;
        case FDS2_DSL_TYPE_F64: return &context->builtin_f64;
        case FDS2_DSL_TYPE_STRING: return &context->builtin_string;
        case FDS2_DSL_TYPE_POINTER: return &context->builtin_pointer;
        default: return NULL;
    }
}

static inline const fds2_dsl_type *fds2_dsl_find_type(const fds2_dsl_context *context,
                                                       const char *name,
                                                       size_t name_length) {
    if (!context || !name) return NULL;
    if (fds2_dsl_name_equal(name, name_length, "int", 3)) return &context->builtin_i32;
    if (fds2_dsl_name_equal(name, name_length, "uint", 4)) return &context->builtin_u32;
    if (fds2_dsl_name_equal(name, name_length, "float", 5)) return &context->builtin_f64;
    if (fds2_dsl_name_equal(name, name_length, "tuple", 5)) return &context->builtin_tuple;
    for (size_t index = 0; index < context->type_count; index++) {
        if (fds2_dsl_name_equal(context->types[index].name, context->types[index].name_length, name, name_length)) return &context->types[index];
    }
    for (fds2_dsl_type_kind kind = FDS2_DSL_TYPE_VOID; kind <= FDS2_DSL_TYPE_POINTER; kind++) {
        const fds2_dsl_type *type = fds2_dsl_builtin_type((fds2_dsl_context *)context, kind);
        if (type && fds2_dsl_name_equal(type->name, type->name_length, name, name_length)) return type;
    }
    return NULL;
}

static inline const fds2_dsl_type *fds2_dsl_register_type(fds2_dsl_context *context,
                                                           const char *name,
                                                           size_t size,
                                                           size_t alignment) {
    fds2_dsl_type *types;
    char *owned_name;
    size_t name_length;
    size_t new_capacity;
    if (!context || !name || !name[0] || alignment == 0) return NULL;
    name_length = strlen(name);
    if (fds2_dsl_find_type(context, name, name_length)) return NULL;
    if (context->type_count == context->type_capacity) {
        new_capacity = context->type_capacity ? context->type_capacity * 2 : 16;
        if (new_capacity < context->type_capacity || fds2_size_mul_overflow(new_capacity, sizeof(*types), NULL)) return NULL;
        types = (fds2_dsl_type *)fds2_realloc_a(context->allocator, context->types, new_capacity * sizeof(*types));
        if (!types) return NULL;
        context->types = types;
        context->type_capacity = new_capacity;
    }
    owned_name = (char *)fds2_alloc_a(context->allocator, name_length + 1);
    if (!owned_name) return NULL;
    memcpy(owned_name, name, name_length + 1);
    context->types[context->type_count] = (fds2_dsl_type){ owned_name, name_length, FDS2_DSL_TYPE_USER, size, alignment, true, NULL, 0, 0, NULL };
    return &context->types[context->type_count++];
}

static inline const fds2_dsl_field *fds2_dsl_register_field(fds2_dsl_context *context,
                                                             fds2_dsl_type *owner,
                                                             const char *name,
                                                             const fds2_dsl_type *type,
                                                             size_t offset) {
    fds2_dsl_field *fields;
    char *owned_name;
    size_t length;
    if (!context || !owner || owner->kind != FDS2_DSL_TYPE_USER || !name || !type) return NULL;
    length = strlen(name);
    if (owner->field_count == owner->field_capacity) {
        size_t capacity = owner->field_capacity ? owner->field_capacity * 2 : 8;
        if (capacity < owner->field_capacity || fds2_size_mul_overflow(capacity, sizeof(*fields), NULL)) return NULL;
        fields = (fds2_dsl_field *)fds2_realloc_a(context->allocator, owner->fields, capacity * sizeof(*fields));
        if (!fields) return NULL;
        owner->fields = fields;
        owner->field_capacity = capacity;
    }
    owned_name = (char *)fds2_alloc_a(context->allocator, length + 1);
    if (!owned_name) return NULL;
    memcpy(owned_name, name, length + 1);
    owner->fields[owner->field_count] = (fds2_dsl_field){ owned_name, length, type, offset };
    return &owner->fields[owner->field_count++];
}

static inline const fds2_dsl_type *fds2_dsl_register_reference(fds2_dsl_context *context,
                                                                const char *name,
                                                                const fds2_dsl_type *referenced_type) {
    fds2_dsl_type *type;
    if (!context || !referenced_type) return NULL;
    type = (fds2_dsl_type *)fds2_dsl_register_type(context, name, sizeof(void *), _Alignof(void *));
    if (!type) return NULL;
    type->kind = FDS2_DSL_TYPE_REFERENCE;
    type->referenced_type = referenced_type;
    return type;
}

static inline const fds2_dsl_field *fds2_dsl_find_field(const fds2_dsl_type *owner,
                                                         const char *name,
                                                         size_t length) {
    if (!owner || !name) return NULL;
    for (size_t index = 0; index < owner->field_count; index++) {
        if (fds2_dsl_name_equal(owner->fields[index].name, owner->fields[index].name_length, name, length)) return &owner->fields[index];
    }
    return NULL;
}

static inline void *fds2_dsl_user_field_ptr(fds2_dsl_object *object,
                                            const fds2_dsl_type *owner,
                                            const fds2_dsl_field *field) {
    if (!object || !owner || !field || object->kind != FDS2_DSL_OBJECT_USER || object->type != owner ||
        field < owner->fields || field >= owner->fields + owner->field_count || field->offset > owner->size ||
        field->type->size > owner->size - field->offset) return NULL;
    return (unsigned char *)(object + 1) + field->offset;
}

static inline bool fds2_dsl_user_field_set(fds2_dsl_object *object,
                                           const fds2_dsl_type *owner,
                                           const fds2_dsl_field *field,
                                           const void *data,
                                           size_t size) {
    void *destination = fds2_dsl_user_field_ptr(object, owner, field);
    if (!destination || !data || size != field->type->size) return false;
    memcpy(destination, data, size);
    return true;
}

static inline bool fds2_dsl_user_field_get(const fds2_dsl_object *object,
                                           const fds2_dsl_type *owner,
                                           const fds2_dsl_field *field,
                                           void *data,
                                           size_t size) {
    void *source = fds2_dsl_user_field_ptr((fds2_dsl_object *)object, owner, field);
    if (!source || !data || size != field->type->size) return false;
    memcpy(data, source, size);
    return true;
}

static inline fds2_dsl_value fds2_dsl_object_value(const fds2_dsl_type *type, fds2_dsl_object *object) {
    fds2_dsl_value value;
    memset(&value, 0, sizeof(value));
    value.type = type;
    value.as.object = object;
    return value;
}

static inline bool fds2_dsl_array_set(fds2_dsl_array *array, size_t index, fds2_dsl_value value) {
    if (!array || index >= array->count) return false;
    array->values[index] = value;
    return true;
}

static inline bool fds2_dsl_array_get(const fds2_dsl_array *array, size_t index, fds2_dsl_value *value) {
    if (!array || !value || index >= array->count) return false;
    *value = array->values[index];
    return true;
}

static inline bool fds2_dsl_map_set(fds2_dsl_map *map, size_t index, fds2_dsl_value key, fds2_dsl_value value) {
    if (!map || index >= map->count) return false;
    map->entries[index] = (fds2_dsl_map_entry){ key, value };
    return true;
}

static inline bool fds2_dsl_map_get(const fds2_dsl_map *map, size_t index, fds2_dsl_map_entry *entry) {
    if (!map || !entry || index >= map->count) return false;
    *entry = map->entries[index];
    return true;
}

static inline bool fds2_dsl_tuple_set(fds2_dsl_tuple *tuple, size_t index, fds2_dsl_value value) {
    if (!tuple || index >= tuple->count) return false;
    tuple->values[index] = value;
    return true;
}

static inline bool fds2_dsl_tuple_get(const fds2_dsl_tuple *tuple, size_t index, fds2_dsl_value *value) {
    if (!tuple || !value || index >= tuple->count) return false;
    *value = tuple->values[index];
    return true;
}

static inline fds2_dsl_value fds2_dsl_bool_value(const fds2_dsl_context *context, bool value) {
    fds2_dsl_value result = { 0 };
    result.type = context ? &context->builtin_bool : NULL;
    result.as.boolean = value;
    return result;
}

static inline fds2_dsl_value fds2_dsl_i32_value(const fds2_dsl_context *context, int32_t value) {
    fds2_dsl_value result = { 0 };
    result.type = context ? &context->builtin_i32 : NULL;
    result.as.signed_integer = value;
    return result;
}

static inline fds2_dsl_value fds2_dsl_value_of(const fds2_dsl_type *type) {
    fds2_dsl_value value;
    memset(&value, 0, sizeof(value));
    value.type = type;
    return value;
}

static inline bool fds2_dsl_type_is_numeric(const fds2_dsl_type *type) {
    return type && type->kind >= FDS2_DSL_TYPE_I8 && type->kind <= FDS2_DSL_TYPE_F64;
}

static inline bool fds2_dsl_type_is_signed(const fds2_dsl_type *type) {
    return type && type->kind >= FDS2_DSL_TYPE_I8 && type->kind <= FDS2_DSL_TYPE_I64;
}

static inline const char *fds2_dsl_type_name(const fds2_dsl_type *type) {
    return type ? type->name : NULL;
}

static inline fds2_dsl_type_kind fds2_dsl_type_kind_of(const fds2_dsl_type *type) {
    return type ? type->kind : FDS2_DSL_TYPE_VOID;
}

static inline bool fds2_dsl_type_equal(const fds2_dsl_type *left, const fds2_dsl_type *right) {
    return left == right;
}

static inline bool fds2_dsl_cast_value(const fds2_dsl_value *source,
                                       const fds2_dsl_type *target,
                                       fds2_dsl_value *result) {
    if (!source || !source->type || !target || !result) return false;
    if (source->type == target) { *result = *source; return true; }
    if (!fds2_dsl_type_is_numeric(source->type) || !fds2_dsl_type_is_numeric(target)) return false;
    *result = fds2_dsl_value_of(target);
    if (target->kind >= FDS2_DSL_TYPE_F32) {
        result->as.floating = fds2_dsl_type_is_signed(source->type) ? (double)source->as.signed_integer : (double)source->as.unsigned_integer;
        if (source->type->kind >= FDS2_DSL_TYPE_F32) result->as.floating = source->as.floating;
    } else if (fds2_dsl_type_is_signed(target)) {
        result->as.signed_integer = fds2_dsl_type_is_signed(source->type) ? source->as.signed_integer : (int64_t)source->as.unsigned_integer;
        if (source->type->kind >= FDS2_DSL_TYPE_F32) result->as.signed_integer = (int64_t)source->as.floating;
    } else {
        result->as.unsigned_integer = fds2_dsl_type_is_signed(source->type) ? (uint64_t)source->as.signed_integer : source->as.unsigned_integer;
        if (source->type->kind >= FDS2_DSL_TYPE_F32) result->as.unsigned_integer = (uint64_t)source->as.floating;
    }
    return true;
}

static inline bool fds2_dsl_native_argument(const fds2_dsl_value *arguments,
                                            size_t argument_count,
                                            size_t index,
                                            const fds2_dsl_type *target,
                                            fds2_dsl_value *result) {
    if (!arguments || index >= argument_count) return false;
    return fds2_dsl_cast_value(&arguments[index], target, result);
}

static inline const fds2_dsl_native *fds2_dsl_register_native(fds2_dsl_context *context,
                                                               const char *name,
                                                               const fds2_dsl_type *return_type,
                                                               const fds2_dsl_type *const *argument_types,
                                                               size_t argument_count,
                                                               fds2_dsl_native_wrapper_fn wrapper,
                                                               void *user_data) {
    fds2_dsl_native *natives;
    const fds2_dsl_type **owned_arguments;
    char *owned_name;
    size_t name_length;
    size_t new_capacity;
    if (!context || !name || !return_type || !wrapper || (argument_count && !argument_types)) return NULL;
    name_length = strlen(name);
    if (context->native_count == context->native_capacity) {
        new_capacity = context->native_capacity ? context->native_capacity * 2 : 16;
        if (new_capacity < context->native_capacity || fds2_size_mul_overflow(new_capacity, sizeof(*natives), NULL)) return NULL;
        natives = (fds2_dsl_native *)fds2_realloc_a(context->allocator, context->natives, new_capacity * sizeof(*natives));
        if (!natives) return NULL;
        context->natives = natives;
        context->native_capacity = new_capacity;
    }
    owned_name = (char *)fds2_alloc_a(context->allocator, name_length + 1);
    if (!owned_name) return NULL;
    memcpy(owned_name, name, name_length + 1);
    owned_arguments = NULL;
    if (argument_count) {
        if (fds2_size_mul_overflow(argument_count, sizeof(*owned_arguments), NULL)) {
            fds2_free_a(context->allocator, owned_name);
            return NULL;
        }
        owned_arguments = (const fds2_dsl_type **)fds2_alloc_a(context->allocator, argument_count * sizeof(*owned_arguments));
        if (!owned_arguments) {
            fds2_free_a(context->allocator, owned_name);
            return NULL;
        }
        memcpy(owned_arguments, argument_types, argument_count * sizeof(*owned_arguments));
    }
    context->natives[context->native_count] = (fds2_dsl_native){
        owned_name, name_length, return_type, owned_arguments, argument_count, wrapper, user_data, true
    };
    return &context->natives[context->native_count++];
}

static inline bool fds2_dsl_register_natives(fds2_dsl_context *context,
                                             const fds2_dsl_native_definition *definitions,
                                             size_t count) {
    if (!context || (count && !definitions)) return false;
    for (size_t index = 0; index < count; index++) {
        const fds2_dsl_native_definition *definition = &definitions[index];
        if (!fds2_dsl_register_native(context, definition->name, definition->return_type,
                                      definition->argument_types, definition->argument_count,
                                      definition->wrapper, definition->user_data)) return false;
    }
    return true;
}

static inline fds2_dsl_error fds2_dsl_native_call(fds2_dsl_vm *vm,
                                                   const fds2_dsl_native *native,
                                                   const fds2_dsl_value *arguments,
                                                   size_t argument_count,
                                                   fds2_dsl_value *result) {
    if (!native || !native->wrapper || !result) return FDS2_DSL_ERROR_BAD_ARGUMENT;
    if (argument_count != native->argument_count) return FDS2_DSL_ERROR_BAD_ARITY;
    if (argument_count && !arguments) return FDS2_DSL_ERROR_BAD_ARGUMENT;
    if (!native->wrapper(vm, arguments, argument_count, result, native->user_data)) return FDS2_DSL_ERROR_NATIVE;
    if (native->return_type && result->type != native->return_type) return FDS2_DSL_ERROR_TYPE_MISMATCH;
    return FDS2_DSL_OK;
}

static inline bool fds2_dsl_core_abs(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    fds2_dsl_context *context = (fds2_dsl_context *)user_data;
    fds2_dsl_value value;
    (void)vm;
    if (!context || !fds2_dsl_native_argument(arguments, count, 0, &context->builtin_i32, &value)) return false;
    *result = fds2_dsl_value_of(&context->builtin_i32);
    result->as.signed_integer = value.as.signed_integer < 0 ? -value.as.signed_integer : value.as.signed_integer;
    return true;
}

static inline bool fds2_dsl_core_min(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    fds2_dsl_context *context = (fds2_dsl_context *)user_data;
    fds2_dsl_value left, right;
    (void)vm;
    if (!context || !fds2_dsl_native_argument(arguments, count, 0, &context->builtin_i32, &left) || !fds2_dsl_native_argument(arguments, count, 1, &context->builtin_i32, &right)) return false;
    *result = fds2_dsl_value_of(&context->builtin_i32);
    result->as.signed_integer = left.as.signed_integer < right.as.signed_integer ? left.as.signed_integer : right.as.signed_integer;
    return true;
}

static inline bool fds2_dsl_core_max(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    fds2_dsl_context *context = (fds2_dsl_context *)user_data;
    fds2_dsl_value left, right;
    (void)vm;
    if (!context || !fds2_dsl_native_argument(arguments, count, 0, &context->builtin_i32, &left) || !fds2_dsl_native_argument(arguments, count, 1, &context->builtin_i32, &right)) return false;
    *result = fds2_dsl_value_of(&context->builtin_i32);
    result->as.signed_integer = left.as.signed_integer > right.as.signed_integer ? left.as.signed_integer : right.as.signed_integer;
    return true;
}

static inline bool fds2_dsl_register_core(fds2_dsl_context *context) {
    const fds2_dsl_type *one[] = { &context->builtin_i32 };
    const fds2_dsl_type *two[] = { &context->builtin_i32, &context->builtin_i32 };
    return fds2_dsl_register_native(context, "abs", &context->builtin_i32, one, 1, fds2_dsl_core_abs, context) &&
           fds2_dsl_register_native(context, "min", &context->builtin_i32, two, 2, fds2_dsl_core_min, context) &&
           fds2_dsl_register_native(context, "max", &context->builtin_i32, two, 2, fds2_dsl_core_max, context);
}

typedef enum fds2_dsl_bytecode_opcode {
    FDS2_DSL_BC_CONST_INT,
    FDS2_DSL_BC_CONST_BOOL,
    FDS2_DSL_BC_CONST_STRING,
    FDS2_DSL_BC_LOAD_LOCAL,
    FDS2_DSL_BC_STORE_LOCAL,
    FDS2_DSL_BC_ADD_INT,
    FDS2_DSL_BC_SUB_INT,
    FDS2_DSL_BC_MUL_INT,
    FDS2_DSL_BC_DIV_INT,
    FDS2_DSL_BC_NEG_INT,
    FDS2_DSL_BC_POP,
    FDS2_DSL_BC_RETURN,
    FDS2_DSL_BC_JUMP,
    FDS2_DSL_BC_JUMP_IF_ZERO,
    FDS2_DSL_BC_EQUAL_INT,
    FDS2_DSL_BC_NOT_EQUAL_INT,
    FDS2_DSL_BC_LESS_INT,
    FDS2_DSL_BC_LESS_EQUAL_INT,
    FDS2_DSL_BC_GREATER_INT,
    FDS2_DSL_BC_GREATER_EQUAL_INT,
    FDS2_DSL_BC_CALL_FUNCTION,
    FDS2_DSL_BC_CALL_NATIVE,
    FDS2_DSL_BC_RETURN_VALUES,
} fds2_dsl_bytecode_opcode;

typedef struct fds2_dsl_instruction {
    fds2_dsl_bytecode_opcode opcode;
    uint32_t operand;
} fds2_dsl_instruction;

typedef struct fds2_dsl_bytecode_function {
    fds2_dsl_token name;
    size_t entry;
    size_t parameter_count;
    size_t local_count;
    const fds2_dsl_type *return_type;
} fds2_dsl_bytecode_function;

typedef struct fds2_dsl_bytecode {
    fds2_allocator *allocator;
    const fds2_dsl_context *context;
    fds2_dsl_instruction *code;
    size_t code_count;
    size_t code_capacity;
    int64_t *integer_constants;
    size_t constant_count;
    size_t constant_capacity;
    fds2_dsl_string *string_constants;
    size_t string_constant_count;
    size_t string_constant_capacity;
    size_t local_count;
    size_t parameter_count;
    const fds2_dsl_type *return_type;
    fds2_dsl_bytecode_function *functions;
    size_t function_count;
    size_t function_capacity;
} fds2_dsl_bytecode;

typedef enum fds2_dsl_compile_error {
    FDS2_DSL_COMPILE_OK = 0,
    FDS2_DSL_COMPILE_BAD_PROGRAM,
    FDS2_DSL_COMPILE_UNKNOWN_TYPE,
    FDS2_DSL_COMPILE_UNKNOWN_NAME,
    FDS2_DSL_COMPILE_TYPE_MISMATCH,
    FDS2_DSL_COMPILE_UNSUPPORTED,
    FDS2_DSL_COMPILE_OUT_OF_MEMORY,
} fds2_dsl_compile_error;

typedef struct fds2_dsl_compile_diagnostic {
    fds2_dsl_compile_error error;
    const char *message;
    size_t line;
    size_t column;
} fds2_dsl_compile_diagnostic;

typedef struct fds2_dsl_compile_local {
    fds2_dsl_token name;
    const fds2_dsl_type *type;
    uint32_t index;
} fds2_dsl_compile_local;

typedef struct fds2_dsl_compile_context {
    fds2_allocator *allocator;
    fds2_dsl_context *types;
    fds2_dsl_bytecode *bytecode;
    fds2_dsl_compile_local *locals;
    size_t local_count;
    size_t local_capacity;
    const fds2_dsl_type *return_type;
    fds2_dsl_compile_diagnostic diagnostic;
    bool emitted_return;
    size_t function_index;
} fds2_dsl_compile_context;

static inline int fds2_dsl_context_find_native(const fds2_dsl_context *context, fds2_dsl_token name) {
    for (size_t index = 0; index < context->native_count; index++) {
        if (fds2_dsl_name_equal(context->natives[index].name, context->natives[index].name_length, name.start, name.length)) return (int)index;
    }
    return -1;
}

static inline void fds2_dsl_bytecode_init(fds2_dsl_bytecode *bytecode,
                                          fds2_allocator *allocator,
                                          const fds2_dsl_context *context) {
    memset(bytecode, 0, sizeof(*bytecode));
    bytecode->allocator = allocator ? allocator : fds2_allocator_current();
    bytecode->context = context;
}

static inline void fds2_dsl_bytecode_deinit(fds2_dsl_bytecode *bytecode) {
    if (!bytecode) return;
    fds2_free_a(bytecode->allocator, bytecode->code);
    fds2_free_a(bytecode->allocator, bytecode->integer_constants);
    fds2_free_a(bytecode->allocator, bytecode->string_constants);
    fds2_free_a(bytecode->allocator, bytecode->functions);
    memset(bytecode, 0, sizeof(*bytecode));
}

static inline int fds2_dsl_bytecode_find_function(const fds2_dsl_bytecode *bytecode, fds2_dsl_token name) {
    for (size_t index = 0; index < bytecode->function_count; index++) {
        if (fds2_dsl_name_equal(bytecode->functions[index].name.start, bytecode->functions[index].name.length, name.start, name.length)) return (int)index;
    }
    return -1;
}

static inline bool fds2_dsl_bytecode_add_function(fds2_dsl_bytecode *bytecode,
                                                  fds2_dsl_node *function,
                                                  size_t *index) {
    if (bytecode->function_count == bytecode->function_capacity) {
        size_t capacity = bytecode->function_capacity ? bytecode->function_capacity * 2 : 8;
        fds2_dsl_bytecode_function *functions;
        if (capacity < bytecode->function_capacity || fds2_size_mul_overflow(capacity, sizeof(*functions), NULL)) return false;
        functions = (fds2_dsl_bytecode_function *)fds2_realloc_a(bytecode->allocator, bytecode->functions, capacity * sizeof(*functions));
        if (!functions) return false;
        bytecode->functions = functions;
        bytecode->function_capacity = capacity;
    }
    if (index) *index = bytecode->function_count;
    bytecode->functions[bytecode->function_count++] = (fds2_dsl_bytecode_function){ function->as.function.name, 0, function->as.function.parameter_count, 0, NULL };
    return true;
}

static inline bool fds2_dsl_bytecode_emit(fds2_dsl_bytecode *bytecode,
                                          fds2_dsl_bytecode_opcode opcode,
                                          uint32_t operand) {
    if (bytecode->code_count == bytecode->code_capacity) {
        size_t capacity = bytecode->code_capacity ? bytecode->code_capacity * 2 : 32;
        fds2_dsl_instruction *code;
        if (capacity < bytecode->code_capacity || fds2_size_mul_overflow(capacity, sizeof(*code), NULL)) return false;
        code = (fds2_dsl_instruction *)fds2_realloc_a(bytecode->allocator, bytecode->code, capacity * sizeof(*code));
        if (!code) return false;
        bytecode->code = code;
        bytecode->code_capacity = capacity;
    }
    bytecode->code[bytecode->code_count++] = (fds2_dsl_instruction){ opcode, operand };
    return true;
}

static inline bool fds2_dsl_bytecode_patch(fds2_dsl_bytecode *bytecode,
                                           size_t instruction_index,
                                           uint32_t operand) {
    if (!bytecode || instruction_index >= bytecode->code_count) return false;
    bytecode->code[instruction_index].operand = operand;
    return true;
}

static inline uint32_t fds2_dsl_bytecode_add_integer(fds2_dsl_bytecode *bytecode, int64_t value) {
    if (bytecode->constant_count == bytecode->constant_capacity) {
        size_t capacity = bytecode->constant_capacity ? bytecode->constant_capacity * 2 : 16;
        int64_t *constants;
        if (capacity < bytecode->constant_capacity || fds2_size_mul_overflow(capacity, sizeof(*constants), NULL)) return UINT32_MAX;
        constants = (int64_t *)fds2_realloc_a(bytecode->allocator, bytecode->integer_constants, capacity * sizeof(*constants));
        if (!constants) return UINT32_MAX;
        bytecode->integer_constants = constants;
        bytecode->constant_capacity = capacity;
    }
    bytecode->integer_constants[bytecode->constant_count] = value;
    return (uint32_t)bytecode->constant_count++;
}

static inline uint32_t fds2_dsl_bytecode_add_string(fds2_dsl_bytecode *bytecode, const char *data, size_t length) {
    if (bytecode->string_constant_count == bytecode->string_constant_capacity) {
        size_t capacity = bytecode->string_constant_capacity ? bytecode->string_constant_capacity * 2 : 16;
        fds2_dsl_string *constants;
        if (capacity < bytecode->string_constant_capacity || fds2_size_mul_overflow(capacity, sizeof(*constants), NULL)) return UINT32_MAX;
        constants = (fds2_dsl_string *)fds2_realloc_a(bytecode->allocator, bytecode->string_constants, capacity * sizeof(*constants));
        if (!constants) return UINT32_MAX;
        bytecode->string_constants = constants;
        bytecode->string_constant_capacity = capacity;
    }
    bytecode->string_constants[bytecode->string_constant_count] = (fds2_dsl_string){ data, length };
    return (uint32_t)bytecode->string_constant_count++;
}

static inline void fds2_dsl_compile_fail(fds2_dsl_compile_context *compiler,
                                         fds2_dsl_compile_error error,
                                         fds2_dsl_token token,
                                         const char *message) {
    if (compiler->diagnostic.error == FDS2_DSL_COMPILE_OK) {
        compiler->diagnostic = (fds2_dsl_compile_diagnostic){ error, message, token.line, token.column };
    }
}

static inline fds2_dsl_compile_local *fds2_dsl_compile_find_local(fds2_dsl_compile_context *compiler,
                                                                    fds2_dsl_token name) {
    for (size_t index = compiler->local_count; index > 0; index--) {
        fds2_dsl_compile_local *local = &compiler->locals[index - 1];
        if (fds2_dsl_name_equal(local->name.start, local->name.length, name.start, name.length)) return local;
    }
    return NULL;
}

static inline fds2_dsl_compile_local *fds2_dsl_compile_add_local(fds2_dsl_compile_context *compiler,
                                                                   fds2_dsl_token name,
                                                                   const fds2_dsl_type *type) {
    if (compiler->local_count == compiler->local_capacity) {
        size_t capacity = compiler->local_capacity ? compiler->local_capacity * 2 : 16;
        fds2_dsl_compile_local *locals;
        if (capacity < compiler->local_capacity || fds2_size_mul_overflow(capacity, sizeof(*locals), NULL)) return NULL;
        locals = (fds2_dsl_compile_local *)fds2_realloc_a(compiler->allocator, compiler->locals, capacity * sizeof(*locals));
        if (!locals) return NULL;
        compiler->locals = locals;
        compiler->local_capacity = capacity;
    }
    compiler->locals[compiler->local_count] = (fds2_dsl_compile_local){ name, type, (uint32_t)compiler->local_count };
    return &compiler->locals[compiler->local_count++];
}

static inline const fds2_dsl_type *fds2_dsl_compile_type(fds2_dsl_compile_context *compiler, fds2_dsl_token token) {
    const fds2_dsl_type *type = fds2_dsl_find_type(compiler->types, token.start, token.length);
    if (!type) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNKNOWN_TYPE, token, "unknown type");
    return type;
}

static inline const fds2_dsl_type *fds2_dsl_compile_expression(fds2_dsl_compile_context *compiler,
                                                                fds2_dsl_node *node) {
    const fds2_dsl_type *left_type;
    const fds2_dsl_type *right_type;
    if (!node) return NULL;
    if (node->kind == FDS2_DSL_NODE_INTEGER) {
        uint32_t constant = fds2_dsl_bytecode_add_integer(compiler->bytecode, node->as.literal.integer);
        if (constant == UINT32_MAX || !fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_CONST_INT, constant)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        return &compiler->types->builtin_i32;
    }
    if (node->kind == FDS2_DSL_NODE_BOOLEAN) {
        uint32_t constant = fds2_dsl_bytecode_add_integer(compiler->bytecode, node->as.literal.boolean ? 1 : 0);
        if (constant == UINT32_MAX || !fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_CONST_BOOL, constant)) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        }
        return &compiler->types->builtin_bool;
    }
    if (node->kind == FDS2_DSL_NODE_IDENTIFIER) {
        fds2_dsl_compile_local *local = fds2_dsl_compile_find_local(compiler, node->token);
        if (!local) { fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNKNOWN_NAME, node->token, "unknown name"); return NULL; }
        if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_LOAD_LOCAL, local->index)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        return local->type;
    }
    if (node->kind == FDS2_DSL_NODE_CALL) {
        int function_index = fds2_dsl_bytecode_find_function(compiler->bytecode, node->as.call.name);
        int native_index = fds2_dsl_context_find_native(compiler->types, node->as.call.name);
        const fds2_dsl_type *return_type = NULL;
        size_t expected_arguments = 0;
        for (size_t index = 0; index < node->as.call.argument_count; index++) fds2_dsl_compile_expression(compiler, node->as.call.arguments[index]);
        if (function_index >= 0) {
            return_type = compiler->bytecode->functions[function_index].return_type;
            expected_arguments = compiler->bytecode->functions[function_index].parameter_count;
        } else if (native_index >= 0) {
            return_type = compiler->types->natives[native_index].return_type;
            expected_arguments = compiler->types->natives[native_index].argument_count;
        } else {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNKNOWN_NAME, node->token, "unknown function");
            return NULL;
        }
        if (expected_arguments != node->as.call.argument_count) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "argument count mismatch");
        if (function_index >= 0) fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_CALL_FUNCTION, (uint32_t)function_index);
        else fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_CALL_NATIVE, (uint32_t)native_index);
        return return_type;
    }
    if (node->kind == FDS2_DSL_NODE_UNARY) {
        const fds2_dsl_type *type = fds2_dsl_compile_expression(compiler, node->as.unary.value);
        if (!type || type != &compiler->types->builtin_i32) { fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "unary operator expects int"); return NULL; }
        if (node->token.kind == FDS2_DSL_TOKEN_MINUS && !fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_NEG_INT, 0)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        return type;
    }
    if (node->kind == FDS2_DSL_NODE_BINARY) {
        left_type = fds2_dsl_compile_expression(compiler, node->as.binary.left);
        right_type = fds2_dsl_compile_expression(compiler, node->as.binary.right);
        if (left_type != &compiler->types->builtin_i32 || right_type != &compiler->types->builtin_i32) { fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "operator expects int operands"); return NULL; }
        fds2_dsl_bytecode_opcode opcode;
        switch (node->token.kind) {
            case FDS2_DSL_TOKEN_PLUS: opcode = FDS2_DSL_BC_ADD_INT; break;
            case FDS2_DSL_TOKEN_MINUS: opcode = FDS2_DSL_BC_SUB_INT; break;
            case FDS2_DSL_TOKEN_STAR: opcode = FDS2_DSL_BC_MUL_INT; break;
            case FDS2_DSL_TOKEN_SLASH: opcode = FDS2_DSL_BC_DIV_INT; break;
            case FDS2_DSL_TOKEN_EQUAL_EQUAL: opcode = FDS2_DSL_BC_EQUAL_INT; break;
            case FDS2_DSL_TOKEN_BANG_EQUAL: opcode = FDS2_DSL_BC_NOT_EQUAL_INT; break;
            case FDS2_DSL_TOKEN_LESS: opcode = FDS2_DSL_BC_LESS_INT; break;
            case FDS2_DSL_TOKEN_LESS_EQUAL: opcode = FDS2_DSL_BC_LESS_EQUAL_INT; break;
            case FDS2_DSL_TOKEN_GREATER: opcode = FDS2_DSL_BC_GREATER_INT; break;
            case FDS2_DSL_TOKEN_GREATER_EQUAL: opcode = FDS2_DSL_BC_GREATER_EQUAL_INT; break;
            default: fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNSUPPORTED, node->token, "operator is not supported by this VM"); return left_type;
        }
        if (!fds2_dsl_bytecode_emit(compiler->bytecode, opcode, 0)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        if (node->token.kind == FDS2_DSL_TOKEN_EQUAL_EQUAL || node->token.kind == FDS2_DSL_TOKEN_BANG_EQUAL ||
            node->token.kind == FDS2_DSL_TOKEN_LESS || node->token.kind == FDS2_DSL_TOKEN_LESS_EQUAL ||
            node->token.kind == FDS2_DSL_TOKEN_GREATER || node->token.kind == FDS2_DSL_TOKEN_GREATER_EQUAL) return &compiler->types->builtin_bool;
        return left_type;
    }
    fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNSUPPORTED, node->token, "expression is not supported by this VM");
    return NULL;
}

static inline void fds2_dsl_compile_statement(fds2_dsl_compile_context *compiler, fds2_dsl_node *node) {
    if (!node) return;
    if (node->kind == FDS2_DSL_NODE_BLOCK) {
        for (size_t index = 0; index < node->as.list.count; index++) fds2_dsl_compile_statement(compiler, node->as.list.items[index]);
        return;
    }
    if (node->kind == FDS2_DSL_NODE_VARIABLE) {
        const fds2_dsl_type *type = fds2_dsl_compile_type(compiler, node->as.variable.type);
        fds2_dsl_compile_local *local = fds2_dsl_compile_add_local(compiler, node->as.variable.name, type);
        if (!local) { fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "local allocation failed"); return; }
        if (node->as.variable.initializer) {
            const fds2_dsl_type *value_type = fds2_dsl_compile_expression(compiler, node->as.variable.initializer);
            if (value_type != type) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "initializer type mismatch");
            if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_STORE_LOCAL, local->index)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        }
        return;
    }
    if (node->kind == FDS2_DSL_NODE_ASSIGN) {
        fds2_dsl_compile_local *local = fds2_dsl_compile_find_local(compiler, node->as.binary.left->token);
        const fds2_dsl_type *value_type;
        if (!local) { fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNKNOWN_NAME, node->token, "unknown assignment target"); return; }
        value_type = fds2_dsl_compile_expression(compiler, node->as.binary.right);
        if (value_type != local->type || !fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_STORE_LOCAL, local->index)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "assignment type mismatch");
        return;
    }
    if (node->kind == FDS2_DSL_NODE_IF) {
        const fds2_dsl_type *condition_type = fds2_dsl_compile_expression(compiler, node->as.conditional.condition);
        size_t false_jump;
        size_t end_jump;
        if (condition_type != &compiler->types->builtin_i32 && condition_type != &compiler->types->builtin_bool) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "if condition expects bool");
            return;
        }
        false_jump = compiler->bytecode->code_count;
        if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_JUMP_IF_ZERO, UINT32_MAX)) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
            return;
        }
        fds2_dsl_compile_statement(compiler, node->as.conditional.then_branch);
        if (node->as.conditional.else_branch) {
            end_jump = compiler->bytecode->code_count;
            if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_JUMP, UINT32_MAX)) {
                fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
                return;
            }
            if (!fds2_dsl_bytecode_patch(compiler->bytecode, false_jump, (uint32_t)compiler->bytecode->code_count)) {
                fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_BAD_PROGRAM, node->token, "invalid jump patch");
                return;
            }
            fds2_dsl_compile_statement(compiler, node->as.conditional.else_branch);
            if (!fds2_dsl_bytecode_patch(compiler->bytecode, end_jump, (uint32_t)compiler->bytecode->code_count)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_BAD_PROGRAM, node->token, "invalid jump patch");
        } else if (!fds2_dsl_bytecode_patch(compiler->bytecode, false_jump, (uint32_t)compiler->bytecode->code_count)) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_BAD_PROGRAM, node->token, "invalid jump patch");
        }
        return;
    }
    if (node->kind == FDS2_DSL_NODE_WHILE) {
        size_t loop_start = compiler->bytecode->code_count;
        size_t exit_jump;
        const fds2_dsl_type *condition_type = fds2_dsl_compile_expression(compiler, node->as.loop.condition);
        if (condition_type != &compiler->types->builtin_i32 && condition_type != &compiler->types->builtin_bool) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "while condition expects bool");
            return;
        }
        exit_jump = compiler->bytecode->code_count;
        if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_JUMP_IF_ZERO, UINT32_MAX)) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
            return;
        }
        fds2_dsl_compile_statement(compiler, node->as.loop.body);
        if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_JUMP, (uint32_t)loop_start) ||
            !fds2_dsl_bytecode_patch(compiler->bytecode, exit_jump, (uint32_t)compiler->bytecode->code_count)) {
            fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_BAD_PROGRAM, node->token, "invalid loop patch");
        }
        return;
    }
    if (node->kind == FDS2_DSL_NODE_RETURN) {
        if (compiler->return_type == &compiler->types->builtin_tuple) {
            for (size_t index = 0; index < node->as.returns.count; index++) fds2_dsl_compile_expression(compiler, node->as.returns.values[index]);
            if (!node->as.returns.count || !fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_RETURN_VALUES, (uint32_t)node->as.returns.count)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "tuple return requires values");
        } else {
            const fds2_dsl_type *value_type = node->as.returns.count == 1 ? fds2_dsl_compile_expression(compiler, node->as.returns.values[0]) : NULL;
            if (value_type != compiler->return_type) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_TYPE_MISMATCH, node->token, "return type mismatch");
            if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_RETURN, 0)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        }
        compiler->emitted_return = true;
        return;
    }
    if (node->kind == FDS2_DSL_NODE_EXPRESSION) {
        fds2_dsl_compile_expression(compiler, node->as.unary.value);
        if (!fds2_dsl_bytecode_emit(compiler->bytecode, FDS2_DSL_BC_POP, 0)) fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, node->token, "bytecode allocation failed");
        return;
    }
    fds2_dsl_compile_fail(compiler, FDS2_DSL_COMPILE_UNSUPPORTED, node->token, "statement is not supported by this VM");
}

static inline fds2_dsl_compile_error fds2_dsl_compile_function(fds2_dsl_context *types,
                                                                fds2_dsl_node *function,
                                                                fds2_dsl_bytecode *bytecode,
                                                                fds2_dsl_compile_diagnostic *diagnostic) {
    fds2_dsl_compile_context compiler;
    size_t function_index;
    if (!types || !function || !bytecode || function->kind != FDS2_DSL_NODE_FUNCTION) return FDS2_DSL_COMPILE_BAD_PROGRAM;
    function_index = (size_t)fds2_dsl_bytecode_find_function(bytecode, function->as.function.name);
    if (function_index == SIZE_MAX && !fds2_dsl_bytecode_add_function(bytecode, function, &function_index)) return FDS2_DSL_COMPILE_OUT_OF_MEMORY;
    memset(&compiler, 0, sizeof(compiler));
    compiler.allocator = bytecode->allocator;
    compiler.types = types;
    compiler.bytecode = bytecode;
    compiler.function_index = function_index;
    if (compiler.function_index < bytecode->function_count) bytecode->functions[compiler.function_index].entry = bytecode->code_count;
    compiler.return_type = fds2_dsl_compile_type(&compiler, function->as.function.return_type);
    bytecode->return_type = compiler.return_type;
    for (size_t index = 0; index < function->as.function.parameter_count; index++) {
        const fds2_dsl_type *type = fds2_dsl_compile_type(&compiler, function->as.function.parameters[index].type);
        if (!fds2_dsl_compile_add_local(&compiler, function->as.function.parameters[index].name, type)) fds2_dsl_compile_fail(&compiler, FDS2_DSL_COMPILE_OUT_OF_MEMORY, function->token, "parameter allocation failed");
    }
    bytecode->parameter_count = function->as.function.parameter_count;
    fds2_dsl_compile_statement(&compiler, function->as.function.body);
    bytecode->local_count = compiler.local_count;
    if (compiler.function_index < bytecode->function_count) {
        bytecode->functions[compiler.function_index].entry = bytecode->functions[compiler.function_index].entry ? bytecode->functions[compiler.function_index].entry : 0;
        bytecode->functions[compiler.function_index].local_count = compiler.local_count;
        bytecode->functions[compiler.function_index].return_type = compiler.return_type;
    }
    fds2_free_a(compiler.allocator, compiler.locals);
    if (!compiler.emitted_return && compiler.diagnostic.error == FDS2_DSL_COMPILE_OK) fds2_dsl_compile_fail(&compiler, FDS2_DSL_COMPILE_BAD_PROGRAM, function->token, "function has no return");
    if (diagnostic) *diagnostic = compiler.diagnostic;
    return compiler.diagnostic.error;
}

static inline fds2_dsl_compile_error fds2_dsl_compile_program(fds2_dsl_context *types,
                                                              fds2_dsl_node *program,
                                                              fds2_dsl_bytecode *bytecode,
                                                              fds2_dsl_compile_diagnostic *diagnostic) {
    if (!types || !program || program->kind != FDS2_DSL_NODE_PROGRAM || !bytecode) return FDS2_DSL_COMPILE_BAD_PROGRAM;
    for (size_t index = 0; index < program->as.list.count; index++) {
        if (program->as.list.items[index]->kind != FDS2_DSL_NODE_FUNCTION || !fds2_dsl_bytecode_add_function(bytecode, program->as.list.items[index], NULL)) {
            return FDS2_DSL_COMPILE_BAD_PROGRAM;
        }
        bytecode->functions[index].return_type = fds2_dsl_find_type(types, program->as.list.items[index]->as.function.return_type.start, program->as.list.items[index]->as.function.return_type.length);
        if (!bytecode->functions[index].return_type) return FDS2_DSL_COMPILE_UNKNOWN_TYPE;
    }
    for (size_t index = 0; index < program->as.list.count; index++) {
        fds2_dsl_compile_error error = fds2_dsl_compile_function(types, program->as.list.items[index], bytecode, diagnostic);
        if (error != FDS2_DSL_COMPILE_OK) return error;
    }
    return FDS2_DSL_COMPILE_OK;
}

static inline fds2_dsl_compile_error fds2_dsl_compile_source(fds2_dsl_context *context,
                                                             const char *source,
                                                             fds2_dsl_bytecode *bytecode,
                                                             fds2_dsl_compile_diagnostic *diagnostic) {
    fds2_dsl_parser parser;
    fds2_dsl_node *program;
    fds2_dsl_compile_error error;
    if (!context || !source || !bytecode) return FDS2_DSL_COMPILE_BAD_PROGRAM;
    program = (fds2_dsl_parser_init(&parser, context->allocator, source), fds2_dsl_parse(&parser));
    if (!program || parser.error != FDS2_DSL_PARSE_OK) {
        fds2_dsl_node_delete(context->allocator, program);
        return FDS2_DSL_COMPILE_BAD_PROGRAM;
    }
    error = fds2_dsl_compile_program(context, program, bytecode, diagnostic);
    fds2_dsl_node_delete(context->allocator, program);
    return error;
}

typedef enum fds2_dsl_vm_error {
    FDS2_DSL_VM_OK = 0,
    FDS2_DSL_VM_BAD_ARGUMENT,
    FDS2_DSL_VM_STACK_UNDERFLOW,
    FDS2_DSL_VM_STACK_OVERFLOW,
    FDS2_DSL_VM_BAD_BYTECODE,
    FDS2_DSL_VM_TYPE_ERROR,
    FDS2_DSL_VM_DIVISION_BY_ZERO,
} fds2_dsl_vm_error;

static inline const char *fds2_dsl_vm_error_string(fds2_dsl_vm_error error) {
    switch (error) {
        case FDS2_DSL_VM_OK: return "ok";
        case FDS2_DSL_VM_BAD_ARGUMENT: return "bad argument";
        case FDS2_DSL_VM_STACK_UNDERFLOW: return "stack underflow";
        case FDS2_DSL_VM_STACK_OVERFLOW: return "stack overflow";
        case FDS2_DSL_VM_BAD_BYTECODE: return "bad bytecode";
        case FDS2_DSL_VM_TYPE_ERROR: return "type error";
        case FDS2_DSL_VM_DIVISION_BY_ZERO: return "division by zero";
    }
    return "unknown vm error";
}

struct fds2_dsl_vm {
    fds2_allocator *allocator;
    const fds2_dsl_bytecode *bytecode;
    fds2_dsl_value *stack;
    size_t stack_count;
    size_t stack_capacity;
    fds2_dsl_value *locals;
    size_t local_count;
    fds2_dsl_vm_error error;
    size_t error_instruction;
    struct {
        fds2_dsl_value *locals;
        size_t local_count;
        size_t return_instruction;
    } frames[64];
    size_t frame_count;
    size_t entry_function;
};

static inline bool fds2_dsl_arg_i32(fds2_dsl_vm *vm,
                                    const fds2_dsl_value *arguments,
                                    size_t argument_count,
                                    size_t index,
                                    int32_t *result) {
    fds2_dsl_value value;
    if (!vm || !vm->bytecode || !result || !fds2_dsl_native_argument(arguments, argument_count, index, &vm->bytecode->context->builtin_i32, &value)) return false;
    *result = (int32_t)value.as.signed_integer;
    return true;
}

static inline bool fds2_dsl_arg_bool(fds2_dsl_vm *vm,
                                     const fds2_dsl_value *arguments,
                                     size_t argument_count,
                                     size_t index,
                                     bool *result) {
    fds2_dsl_value value;
    if (!vm || !vm->bytecode || !result || !fds2_dsl_native_argument(arguments, argument_count, index, &vm->bytecode->context->builtin_bool, &value)) return false;
    *result = value.as.boolean;
    return true;
}

static inline bool fds2_dsl_return_i32(fds2_dsl_vm *vm, fds2_dsl_value *result, int32_t value) {
    if (!vm || !vm->bytecode || !result) return false;
    *result = fds2_dsl_i32_value(vm->bytecode->context, value);
    return true;
}

static inline bool fds2_dsl_return_bool(fds2_dsl_vm *vm, fds2_dsl_value *result, bool value) {
    if (!vm || !vm->bytecode || !result) return false;
    *result = fds2_dsl_bool_value(vm->bytecode->context, value);
    return true;
}

static inline bool fds2_dsl_arg_string(fds2_dsl_vm *vm,
                                       const fds2_dsl_value *arguments,
                                       size_t argument_count,
                                       size_t index,
                                       fds2_dsl_string *result) {
    if (!vm || !vm->bytecode || !result || index >= argument_count || !arguments || arguments[index].type != &vm->bytecode->context->builtin_string) return false;
    *result = arguments[index].as.string;
    return true;
}

static inline bool fds2_dsl_arg_object(fds2_dsl_vm *vm,
                                       const fds2_dsl_value *arguments,
                                       size_t argument_count,
                                       size_t index,
                                       const fds2_dsl_type *type,
                                       fds2_dsl_object **result) {
    if (!vm || !vm->bytecode || !result || index >= argument_count || !arguments || !arguments[index].as.object) return false;
    if (type && arguments[index].type != type) return false;
    *result = arguments[index].as.object;
    return true;
}

static inline bool fds2_dsl_arg_array(fds2_dsl_vm *vm,
                                      const fds2_dsl_value *arguments,
                                      size_t argument_count,
                                      size_t index,
                                      fds2_dsl_array **result) {
    fds2_dsl_object *object;
    if (!fds2_dsl_arg_object(vm, arguments, argument_count, index, NULL, &object) || object->kind != FDS2_DSL_OBJECT_ARRAY) return false;
    *result = (fds2_dsl_array *)object;
    return true;
}

static inline bool fds2_dsl_arg_tuple(fds2_dsl_vm *vm,
                                      const fds2_dsl_value *arguments,
                                      size_t argument_count,
                                      size_t index,
                                      fds2_dsl_tuple **result) {
    fds2_dsl_object *object;
    if (!fds2_dsl_arg_object(vm, arguments, argument_count, index, NULL, &object) || object->kind != FDS2_DSL_OBJECT_TUPLE) return false;
    *result = (fds2_dsl_tuple *)object;
    return true;
}

static inline bool fds2_dsl_return_object(fds2_dsl_vm *vm,
                                          fds2_dsl_value *result,
                                          const fds2_dsl_type *type,
                                          fds2_dsl_object *object) {
    if (!vm || !vm->bytecode || !result || !object) return false;
    *result = fds2_dsl_object_value(type, object);
    return true;
}

static inline bool fds2_dsl_return_string(fds2_dsl_vm *vm,
                                          fds2_dsl_value *result,
                                          const char *data,
                                          size_t length) {
    fds2_dsl_object *object;
    if (!vm || !vm->bytecode || !result || (!data && length)) return false;
    object = fds2_dsl_heap_alloc((fds2_dsl_heap *)&vm->bytecode->context->heap,
        FDS2_DSL_OBJECT_STRING, length + 1, NULL, NULL, NULL);
    if (!object) return false;
    memcpy(object + 1, data, length);
    ((char *)(object + 1))[length] = '\0';
    object->type = &vm->bytecode->context->builtin_string;
    *result = fds2_dsl_value_of(object->type);
    result->as.string = (fds2_dsl_string){ (const char *)(object + 1), length };
    return true;
}

static inline void fds2_dsl_vm_init(fds2_dsl_vm *vm,
                                    fds2_allocator *allocator,
                                    const fds2_dsl_bytecode *bytecode) {
    memset(vm, 0, sizeof(*vm));
    vm->allocator = allocator ? allocator : fds2_allocator_current();
    vm->bytecode = bytecode;
    vm->entry_function = 0;
}

static inline bool fds2_dsl_vm_set_entry(fds2_dsl_vm *vm, size_t function_index) {
    if (!vm || !vm->bytecode || function_index >= vm->bytecode->function_count) return false;
    vm->entry_function = function_index;
    return true;
}

static inline int fds2_dsl_bytecode_find_function_name(const fds2_dsl_bytecode *bytecode, const char *name) {
    return name ? fds2_dsl_bytecode_find_function(bytecode, (fds2_dsl_token){ FDS2_DSL_TOKEN_IDENTIFIER, name, strlen(name), 0, 0, 0, 0.0 }) : -1;
}

static inline void fds2_dsl_vm_deinit(fds2_dsl_vm *vm) {
    if (!vm) return;
    fds2_free_a(vm->allocator, vm->stack);
    fds2_free_a(vm->allocator, vm->locals);
    for (size_t index = 0; index < vm->frame_count; index++) fds2_free_a(vm->allocator, vm->frames[index].locals);
    memset(vm, 0, sizeof(*vm));
}

static inline bool fds2_dsl_vm_push(fds2_dsl_vm *vm, fds2_dsl_value value) {
    if (vm->stack_count == vm->stack_capacity) {
        size_t capacity = vm->stack_capacity ? vm->stack_capacity * 2 : 32;
        fds2_dsl_value *stack;
        if (capacity < vm->stack_capacity || fds2_size_mul_overflow(capacity, sizeof(*stack), NULL)) return false;
        stack = (fds2_dsl_value *)fds2_realloc_a(vm->allocator, vm->stack, capacity * sizeof(*stack));
        if (!stack) return false;
        vm->stack = stack;
        vm->stack_capacity = capacity;
    }
    vm->stack[vm->stack_count++] = value;
    return true;
}

static inline bool fds2_dsl_vm_pop(fds2_dsl_vm *vm, fds2_dsl_value *value) {
    if (!vm->stack_count) return false;
    *value = vm->stack[--vm->stack_count];
    return true;
}

static inline fds2_dsl_vm_error fds2_dsl_vm_run(fds2_dsl_vm *vm,
                                                const fds2_dsl_value *arguments,
                                                size_t argument_count,
                                                fds2_dsl_value *result) {
    const fds2_dsl_bytecode *bytecode;
    if (!vm || !vm->bytecode || !result || argument_count != vm->bytecode->parameter_count) return FDS2_DSL_VM_BAD_ARGUMENT;
    bytecode = vm->bytecode;
    if (argument_count && !arguments) return FDS2_DSL_VM_BAD_ARGUMENT;
    fds2_free_a(vm->allocator, vm->locals);
    vm->locals = NULL;
    vm->local_count = 0;
    vm->stack_count = 0;
    vm->locals = (fds2_dsl_value *)fds2_alloc_a(vm->allocator, bytecode->local_count * sizeof(*vm->locals));
    if (bytecode->local_count && !vm->locals) return FDS2_DSL_VM_STACK_OVERFLOW;
    vm->local_count = bytecode->local_count;
    for (size_t index = 0; index < argument_count; index++) {
        if (arguments[index].type != &bytecode->context->builtin_i32) { vm->error = FDS2_DSL_VM_TYPE_ERROR; return vm->error; }
        vm->locals[index] = arguments[index];
    }
    if (vm->entry_function >= bytecode->function_count) { vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error; }
    size_t instruction_index = bytecode->functions[vm->entry_function].entry;
    for (; instruction_index < bytecode->code_count; instruction_index++) {
        fds2_dsl_instruction instruction = bytecode->code[instruction_index];
        fds2_dsl_value left, right;
        vm->error_instruction = instruction_index;
        switch (instruction.opcode) {
            case FDS2_DSL_BC_CONST_INT:
                if (instruction.operand >= bytecode->constant_count || !fds2_dsl_vm_push(vm, (fds2_dsl_value){ &bytecode->context->builtin_i32, { .signed_integer = bytecode->integer_constants[instruction.operand] } })) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                break;
            case FDS2_DSL_BC_CONST_BOOL:
                if (instruction.operand >= bytecode->constant_count || !fds2_dsl_vm_push(vm, fds2_dsl_bool_value(bytecode->context, bytecode->integer_constants[instruction.operand] != 0))) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                break;
            case FDS2_DSL_BC_LOAD_LOCAL:
                if (instruction.operand >= vm->local_count || !fds2_dsl_vm_push(vm, vm->locals[instruction.operand])) { vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error; }
                break;
            case FDS2_DSL_BC_STORE_LOCAL:
                if (instruction.operand >= vm->local_count || !fds2_dsl_vm_pop(vm, &left)) { vm->error = FDS2_DSL_VM_STACK_UNDERFLOW; return vm->error; }
                vm->locals[instruction.operand] = left;
                break;
            case FDS2_DSL_BC_NEG_INT:
                if (!fds2_dsl_vm_pop(vm, &left) || left.type != &bytecode->context->builtin_i32 || !fds2_dsl_vm_push(vm, (fds2_dsl_value){ left.type, { .signed_integer = -left.as.signed_integer } })) { vm->error = FDS2_DSL_VM_TYPE_ERROR; return vm->error; }
                break;
            case FDS2_DSL_BC_ADD_INT:
            case FDS2_DSL_BC_SUB_INT:
            case FDS2_DSL_BC_MUL_INT:
            case FDS2_DSL_BC_DIV_INT:
                if (!fds2_dsl_vm_pop(vm, &right) || !fds2_dsl_vm_pop(vm, &left) || left.type != &bytecode->context->builtin_i32 || right.type != &bytecode->context->builtin_i32) { vm->error = FDS2_DSL_VM_TYPE_ERROR; return vm->error; }
                if (instruction.opcode == FDS2_DSL_BC_DIV_INT && right.as.signed_integer == 0) { vm->error = FDS2_DSL_VM_DIVISION_BY_ZERO; return vm->error; }
                if (instruction.opcode == FDS2_DSL_BC_ADD_INT) left.as.signed_integer += right.as.signed_integer;
                else if (instruction.opcode == FDS2_DSL_BC_SUB_INT) left.as.signed_integer -= right.as.signed_integer;
                else if (instruction.opcode == FDS2_DSL_BC_MUL_INT) left.as.signed_integer *= right.as.signed_integer;
                else left.as.signed_integer /= right.as.signed_integer;
                if (!fds2_dsl_vm_push(vm, left)) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                break;
            case FDS2_DSL_BC_EQUAL_INT:
            case FDS2_DSL_BC_NOT_EQUAL_INT:
            case FDS2_DSL_BC_LESS_INT:
            case FDS2_DSL_BC_LESS_EQUAL_INT:
            case FDS2_DSL_BC_GREATER_INT:
            case FDS2_DSL_BC_GREATER_EQUAL_INT:
                if (!fds2_dsl_vm_pop(vm, &right) || !fds2_dsl_vm_pop(vm, &left) || left.type != &bytecode->context->builtin_i32 || right.type != &bytecode->context->builtin_i32) { vm->error = FDS2_DSL_VM_TYPE_ERROR; return vm->error; }
                if (instruction.opcode == FDS2_DSL_BC_EQUAL_INT) left.as.signed_integer = left.as.signed_integer == right.as.signed_integer;
                else if (instruction.opcode == FDS2_DSL_BC_NOT_EQUAL_INT) left.as.signed_integer = left.as.signed_integer != right.as.signed_integer;
                else if (instruction.opcode == FDS2_DSL_BC_LESS_INT) left.as.signed_integer = left.as.signed_integer < right.as.signed_integer;
                else if (instruction.opcode == FDS2_DSL_BC_LESS_EQUAL_INT) left.as.signed_integer = left.as.signed_integer <= right.as.signed_integer;
                else if (instruction.opcode == FDS2_DSL_BC_GREATER_INT) left.as.signed_integer = left.as.signed_integer > right.as.signed_integer;
                else left.as.signed_integer = left.as.signed_integer >= right.as.signed_integer;
                if (instruction.opcode >= FDS2_DSL_BC_EQUAL_INT && instruction.opcode <= FDS2_DSL_BC_GREATER_EQUAL_INT) {
                    left.type = &bytecode->context->builtin_bool;
                    left.as.boolean = left.as.signed_integer != 0;
                }
                if (!fds2_dsl_vm_push(vm, left)) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                break;
            case FDS2_DSL_BC_POP:
                if (!fds2_dsl_vm_pop(vm, &left)) { vm->error = FDS2_DSL_VM_STACK_UNDERFLOW; return vm->error; }
                break;
            case FDS2_DSL_BC_JUMP:
                if (instruction.operand >= bytecode->code_count) { vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error; }
                instruction_index = instruction.operand - 1;
                break;
            case FDS2_DSL_BC_JUMP_IF_ZERO:
                if (!fds2_dsl_vm_pop(vm, &left) || (left.type != &bytecode->context->builtin_i32 && left.type != &bytecode->context->builtin_bool)) { vm->error = FDS2_DSL_VM_TYPE_ERROR; return vm->error; }
                if ((left.type == &bytecode->context->builtin_bool && !left.as.boolean) || (left.type == &bytecode->context->builtin_i32 && left.as.signed_integer == 0)) {
                    if (instruction.operand >= bytecode->code_count) { vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error; }
                    instruction_index = instruction.operand - 1;
                }
                break;
            case FDS2_DSL_BC_CALL_NATIVE: {
                fds2_dsl_native *native;
                fds2_dsl_value call_result;
                size_t argument_count;
                if (instruction.operand >= bytecode->context->native_count) { vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error; }
                native = &bytecode->context->natives[instruction.operand];
                argument_count = native->argument_count;
                if (vm->stack_count < argument_count || fds2_dsl_native_call(vm, native, vm->stack + vm->stack_count - argument_count, argument_count, &call_result) != FDS2_DSL_OK) { vm->error = FDS2_DSL_VM_TYPE_ERROR; return vm->error; }
                vm->stack_count -= argument_count;
                if (!fds2_dsl_vm_push(vm, call_result)) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                break;
            }
            case FDS2_DSL_BC_CALL_FUNCTION: {
                const fds2_dsl_bytecode_function *function;
                fds2_dsl_value *new_locals;
                size_t base;
                if (instruction.operand >= bytecode->function_count || vm->frame_count >= 64) { vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error; }
                function = &bytecode->functions[instruction.operand];
                if (vm->stack_count < function->parameter_count) { vm->error = FDS2_DSL_VM_STACK_UNDERFLOW; return vm->error; }
                base = vm->stack_count - function->parameter_count;
                new_locals = (fds2_dsl_value *)fds2_calloc_a(vm->allocator, function->local_count, sizeof(*new_locals));
                if (function->local_count && !new_locals) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                for (size_t index = 0; index < function->parameter_count; index++) new_locals[index] = vm->stack[base + index];
                vm->frames[vm->frame_count].locals = vm->locals;
                vm->frames[vm->frame_count].local_count = vm->local_count;
                vm->frames[vm->frame_count].return_instruction = instruction_index + 1;
                vm->frame_count++;
                vm->stack_count = base;
                vm->locals = new_locals;
                vm->local_count = function->local_count;
                instruction_index = function->entry - 1;
                break;
            }
            case FDS2_DSL_BC_RETURN_VALUES: {
                fds2_dsl_tuple *tuple;
                if (instruction.operand == 0 || vm->stack_count < instruction.operand) { vm->error = FDS2_DSL_VM_STACK_UNDERFLOW; return vm->error; }
                tuple = fds2_dsl_tuple_new((fds2_dsl_heap *)&bytecode->context->heap, instruction.operand);
                if (!tuple) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                tuple->object.type = &bytecode->context->builtin_tuple;
                for (size_t index = 0; index < instruction.operand; index++) tuple->values[instruction.operand - index - 1] = vm->stack[--vm->stack_count];
                *result = fds2_dsl_object_value(&bytecode->context->builtin_tuple, &tuple->object);
                if (vm->frame_count) {
                    size_t return_instruction = vm->frames[vm->frame_count - 1].return_instruction;
                    fds2_free_a(vm->allocator, vm->locals);
                    vm->locals = vm->frames[vm->frame_count - 1].locals;
                    vm->local_count = vm->frames[vm->frame_count - 1].local_count;
                    vm->frame_count--;
                    if (!fds2_dsl_vm_push(vm, *result)) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                    instruction_index = return_instruction - 1;
                    break;
                }
                vm->error = FDS2_DSL_VM_OK;
                return vm->error;
            }
            case FDS2_DSL_BC_RETURN:
                if (!fds2_dsl_vm_pop(vm, result)) { vm->error = FDS2_DSL_VM_STACK_UNDERFLOW; return vm->error; }
                if (vm->frame_count) {
                    size_t return_instruction = vm->frames[vm->frame_count - 1].return_instruction;
                    fds2_free_a(vm->allocator, vm->locals);
                    vm->locals = vm->frames[vm->frame_count - 1].locals;
                    vm->local_count = vm->frames[vm->frame_count - 1].local_count;
                    vm->frame_count--;
                    if (!fds2_dsl_vm_push(vm, *result)) { vm->error = FDS2_DSL_VM_STACK_OVERFLOW; return vm->error; }
                    instruction_index = return_instruction - 1;
                    break;
                }
                vm->error = FDS2_DSL_VM_OK;
                return vm->error;
            default: vm->error = FDS2_DSL_VM_BAD_BYTECODE; return vm->error;
        }
    }
    vm->error = FDS2_DSL_VM_BAD_BYTECODE;
    return vm->error;
}

static inline fds2_dsl_vm_error fds2_dsl_run_source(fds2_dsl_context *context,
                                                    const char *source,
                                                    const fds2_dsl_value *arguments,
                                                    size_t argument_count,
                                                    fds2_dsl_value *result,
                                                    fds2_dsl_compile_diagnostic *diagnostic) {
    fds2_dsl_bytecode bytecode;
    fds2_dsl_vm vm;
    fds2_dsl_compile_error compile_error;
    fds2_dsl_vm_error vm_error;
    if (!context || !source || !result) return FDS2_DSL_VM_BAD_ARGUMENT;
    fds2_dsl_bytecode_init(&bytecode, context->allocator, context);
    compile_error = fds2_dsl_compile_source(context, source, &bytecode, diagnostic);
    if (compile_error != FDS2_DSL_COMPILE_OK) {
        fds2_dsl_bytecode_deinit(&bytecode);
        return FDS2_DSL_VM_BAD_BYTECODE;
    }
    fds2_dsl_vm_init(&vm, context->allocator, &bytecode);
    vm_error = fds2_dsl_vm_run(&vm, arguments, argument_count, result);
    fds2_dsl_vm_deinit(&vm);
    fds2_dsl_bytecode_deinit(&bytecode);
    return vm_error;
}

struct fds2_dsl_state {
    fds2_allocator *allocator;
    fds2_dsl_context context;
    fds2_dsl_bytecode bytecode;
    fds2_dsl_vm vm;
    bool compiled;
    bool vm_ready;
};

typedef struct fds2_dsl_config {
    fds2_allocator *allocator;
    bool register_core;
    size_t gc_threshold;
} fds2_dsl_config;

static inline fds2_dsl_config fds2_dsl_config_default(void) {
    return (fds2_dsl_config){ NULL, true, 4096 };
}

static inline const fds2_dsl_type *fds2_dsl_state_type(fds2_dsl_state *state,
                                                       const char *name) {
    return state && name ? fds2_dsl_find_type(&state->context, name, strlen(name)) : NULL;
}

static inline bool fds2_dsl_state_register_native(fds2_dsl_state *state,
                                                  const fds2_dsl_native_definition *definition) {
    return state && definition && fds2_dsl_register_native(&state->context,
        definition->name, definition->return_type, definition->argument_types,
        definition->argument_count, definition->wrapper, definition->user_data) != NULL;
}

static inline bool fds2_dsl_state_register_natives(fds2_dsl_state *state,
                                                   const fds2_dsl_native_definition *definitions,
                                                   size_t count) {
    return state && fds2_dsl_register_natives(&state->context, definitions, count);
}

static inline fds2_dsl_value fds2_dsl_state_i32(const fds2_dsl_state *state, int32_t value) {
    return fds2_dsl_i32_value(state ? &state->context : NULL, value);
}

static inline fds2_dsl_value fds2_dsl_state_bool(const fds2_dsl_state *state, bool value) {
    return fds2_dsl_bool_value(state ? &state->context : NULL, value);
}

static inline bool fds2_dsl_state_init(fds2_dsl_state *state,
                                       fds2_allocator *allocator,
                                       bool register_core);

static inline bool fds2_dsl_state_init_config(fds2_dsl_state *state,
                                              const fds2_dsl_config *config) {
    fds2_dsl_config defaults;
    fds2_allocator *allocator;
    if (!state) return false;
    defaults = fds2_dsl_config_default();
    if (!config) config = &defaults;
    allocator = config->allocator ? config->allocator : fds2_allocator_current();
    memset(state, 0, sizeof(*state));
    state->allocator = allocator;
    fds2_dsl_context_init(&state->context, allocator);
    if (config->gc_threshold) state->context.heap.next_collection = config->gc_threshold;
    if (config->register_core && !fds2_dsl_register_core(&state->context)) {
        fds2_dsl_context_deinit(&state->context);
        memset(state, 0, sizeof(*state));
        return false;
    }
    return true;
}

static inline fds2_dsl_state *fds2_dsl_state_new_config(const fds2_dsl_config *config) {
    fds2_dsl_config defaults;
    fds2_allocator *allocator;
    fds2_dsl_state *state;
    if (!config) {
        defaults = fds2_dsl_config_default();
        config = &defaults;
    }
    allocator = config->allocator ? config->allocator : fds2_allocator_current();
    state = (fds2_dsl_state *)fds2_alloc_a(allocator, sizeof(*state));
    if (!state || !fds2_dsl_state_init_config(state, config)) {
        fds2_free_a(allocator, state);
        return NULL;
    }
    return state;
}

static inline fds2_dsl_state *fds2_dsl_state_new(fds2_allocator *allocator, bool register_core) {
    fds2_dsl_config config = { allocator, register_core, 4096 };
    return fds2_dsl_state_new_config(&config);
}

static inline bool fds2_dsl_state_init(fds2_dsl_state *state,
                                       fds2_allocator *allocator,
                                       bool register_core) {
    fds2_dsl_config config = { allocator, register_core, 4096 };
    return fds2_dsl_state_init_config(state, &config);
}

static inline void fds2_dsl_state_deinit(fds2_dsl_state *state) {
    if (!state) return;
    if (state->vm_ready) fds2_dsl_vm_deinit(&state->vm);
    if (state->compiled) fds2_dsl_bytecode_deinit(&state->bytecode);
    fds2_dsl_context_deinit(&state->context);
    memset(state, 0, sizeof(*state));
}

static inline void fds2_dsl_state_delete(fds2_dsl_state *state) {
    fds2_allocator *allocator;
    if (!state) return;
    allocator = state->allocator;
    fds2_dsl_state_deinit(state);
    fds2_free_a(allocator, state);
}

static inline fds2_dsl_compile_error fds2_dsl_state_compile(fds2_dsl_state *state,
                                                             const char *source,
                                                             fds2_dsl_compile_diagnostic *diagnostic) {
    fds2_dsl_compile_error error;
    if (!state || !source) return FDS2_DSL_COMPILE_BAD_PROGRAM;
    if (state->vm_ready) {
        fds2_dsl_vm_deinit(&state->vm);
        state->vm_ready = false;
    }
    if (state->compiled) {
        fds2_dsl_bytecode_deinit(&state->bytecode);
        state->compiled = false;
    }
    fds2_dsl_bytecode_init(&state->bytecode, state->allocator, &state->context);
    error = fds2_dsl_compile_source(&state->context, source, &state->bytecode, diagnostic);
    if (error != FDS2_DSL_COMPILE_OK) {
        fds2_dsl_bytecode_deinit(&state->bytecode);
        return error;
    }
    state->compiled = true;
    return FDS2_DSL_COMPILE_OK;
}

static inline fds2_dsl_compile_error fds2_dsl_state_compile_file(fds2_dsl_state *state,
                                                                  const char *path,
                                                                  fds2_dsl_compile_diagnostic *diagnostic) {
    char *source;
    fds2_dsl_compile_error error;
    if (!state || !path || !fds2_dsl_read_file(state->allocator, path, &source, NULL)) return FDS2_DSL_COMPILE_BAD_PROGRAM;
    error = fds2_dsl_state_compile(state, source, diagnostic);
    fds2_free_a(state->allocator, source);
    return error;
}

static inline fds2_dsl_vm_error fds2_dsl_state_run(fds2_dsl_state *state,
                                                   const char *entry_name,
                                                   const fds2_dsl_value *arguments,
                                                   size_t argument_count,
                                                   fds2_dsl_value *result) {
    int entry;
    if (!state || !state->compiled || !result) return FDS2_DSL_VM_BAD_ARGUMENT;
    if (!state->vm_ready) {
        fds2_dsl_vm_init(&state->vm, state->allocator, &state->bytecode);
        state->vm_ready = true;
    }
    if (entry_name) {
        entry = fds2_dsl_bytecode_find_function_name(&state->bytecode, entry_name);
        if (entry < 0 || !fds2_dsl_vm_set_entry(&state->vm, (size_t)entry)) return FDS2_DSL_VM_BAD_BYTECODE;
    }
    return fds2_dsl_vm_run(&state->vm, arguments, argument_count, result);
}

#ifdef __cplusplus
}
#endif

#endif
