#ifndef MICA_H
#define MICA_H

/* Mica v0.1 -- small declarative configuration runtime (C99). */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MicaContext MicaContext;

typedef enum MicaSetResult {
    MICA_SET_OK = 0,
    MICA_SET_INVALID_ARGUMENT,
    MICA_SET_INVALID_PATH,
    MICA_SET_INTERMEDIATE_NOT_OBJECT,
    MICA_SET_IS_EXPRESSION,
    MICA_SET_TYPE_MISMATCH,
    MICA_SET_REPARSE_ERROR
} MicaSetResult;

/* Parses UTF-8 text.  The returned context owns the parsed AST and caches. */
MicaContext *mica_parse(const char *source);
void mica_destroy(MicaContext *ctx);

/* Read values by a root-relative dotted path.  On failure, fallback is returned. */
double mica_get_float(MicaContext *ctx, const char *path, double fallback);
int mica_get_bool(MicaContext *ctx, const char *path, int fallback);
const char *mica_get_string(MicaContext *ctx, const char *path,
                            const char *fallback);

/* The exact, current UTF-8 configuration text. It is owned by ctx. */
const char *mica_source(const MicaContext *ctx);

/*
 * Changes a literal or creates a missing field and any missing objects.
 * The context may be replaced after a successful change; pass its address.
 * Existing computed expressions are protected and return MICA_SET_IS_EXPRESSION.
 */
MicaSetResult mica_set_float(MicaContext **ctx, const char *path, double value);
MicaSetResult mica_set_bool(MicaContext **ctx, const char *path, int value);
MicaSetResult mica_set_string(MicaContext **ctx, const char *path, const char *value);

/* Explicit opt-in to replacing an existing expression with a literal. */
MicaSetResult mica_set_float_force(MicaContext **ctx, const char *path, double value);
MicaSetResult mica_set_bool_force(MicaContext **ctx, const char *path, int value);
MicaSetResult mica_set_string_force(MicaContext **ctx, const char *path, const char *value);

/* NULL means the parse itself succeeded. Runtime warnings go to stderr. */
const char *mica_last_error(const MicaContext *ctx);

#ifdef __cplusplus
}
#endif
#endif
