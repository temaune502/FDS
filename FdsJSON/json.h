#ifndef FDS_JSON_H
#define FDS_JSON_H

#include "fds.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH 128
#endif

#ifndef JSON_ARENA_SIZE
#define JSON_ARENA_SIZE (256 * KB)
#endif

    typedef enum
    {
        JSON_NULL = 0,
        JSON_BOOL,
        JSON_NUMBER,
        JSON_STRING,
        JSON_ARRAY,
        JSON_OBJECT
    } JsonType;

    typedef struct JsonValue JsonValue;
    typedef struct JsonMember JsonMember;

    typedef struct
    {
        JsonValue *items;
        size_t count;
        size_t capacity;
    } JsonArray;

    typedef struct
    {
        JsonMember *items;
        size_t count;
        size_t capacity;
    } JsonObject;

    struct JsonValue
    {
        JsonType type;
        union
        {
            bool boolean;
            double number;
            SV string;
            JsonArray array;
            JsonObject object;
        } as;
    };

    struct JsonMember
    {
        SV key;
        JsonValue value;
    };

    typedef struct
    {
        JsonValue root;
        FixedArena arena;
        bool ok;
        size_t error_line;
        size_t error_col;
        char error[256];
    } Json;

    Json json_new(void);
    Json json_new_cap(size_t arena_bytes);
    void json_free(Json *doc);

    Json json_parse(const char *filepath);
    Json json_parse_cstr(const char *text);
    Json json_parse_sv(SV content);
    Json json_parse_sb(const SB *content);

    JsonValue json_null(void);
    JsonValue json_bool(bool value);
    JsonValue json_number(double value);
    JsonValue json_string(Json *doc, const char *cstr);
    JsonValue json_string_sv(Json *doc, SV sv);
    JsonValue json_array(void);
    JsonValue json_object(void);

    void json_array_push(JsonValue *array, JsonValue item);
    void json_object_put(Json *doc, JsonValue *object, const char *key, JsonValue value);
    void json_object_put_sv(Json *doc, JsonValue *object, SV key, JsonValue value);

    JsonValue *json_object_get(JsonValue *object, const char *key);
    const JsonValue *json_object_get_c(const JsonValue *object, const char *key);
    JsonValue *json_array_at(JsonValue *array, size_t index);
    const JsonValue *json_array_at_c(const JsonValue *array, size_t index);

    int json_get_bool(const JsonValue *object, const char *key, int default_val);
    double json_get_number(const JsonValue *object, const char *key, double default_val);
    SV json_get_string(const JsonValue *object, const char *key, const char *default_val);

    void json_append(SB *sb, const JsonValue *value);
    void json_append_pretty(SB *sb, const JsonValue *value, int indent);
    SB json_stringify(const JsonValue *value);
    SB json_stringify_pretty(const JsonValue *value, int indent);
    char *json_to_cstr(const JsonValue *value);
    void json_print(const JsonValue *value);
    int json_write_file(const char *filepath, const JsonValue *value, int pretty);

    static inline int json_is(const JsonValue *v, JsonType type)
    {
        return v && v->type == type;
    }

#ifdef __cplusplus
}
#endif

#endif
