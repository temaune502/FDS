#include "json.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef JSON_PRETTY_INDENT
#define JSON_PRETTY_INDENT 2
#endif

static void json_value_free(JsonValue *value);

static void json_da_grow(void **items, size_t *count, size_t *capacity, size_t elem_size)
{
    if (*count < *capacity)
        return;
    size_t new_cap = *capacity ? *capacity * 2 : 4;
    void *p = FDS_REALLOC(*items, new_cap * elem_size);
    if (!p)
        fds_log(FFATAL, "Out of memory");
    *items = p;
    *capacity = new_cap;
}

static void json_array_push_raw(JsonArray *array, JsonValue item)
{
    json_da_grow((void **)&array->items, &array->count, &array->capacity, sizeof(*array->items));
    array->items[array->count++] = item;
}

static void json_object_push_raw(JsonObject *object, JsonMember member)
{
    json_da_grow((void **)&object->items, &object->count, &object->capacity, sizeof(*object->items));
    object->items[object->count++] = member;
}

static SV json_intern(Json *doc, const char *data, size_t count)
{
    if (!data || count == 0)
        return sv_new();
    char *copy = fixed_arena_strndup(&doc->arena, data, count);
    return sv_from_parts(copy, count);
}

static void json_reset_error(Json *doc)
{
    doc->ok = true;
    doc->error_line = 0;
    doc->error_col = 0;
    doc->error[0] = '\0';
}

Json json_new_cap(size_t arena_bytes)
{
    Json doc = zero;
    if (arena_bytes < 64)
        arena_bytes = 64;
    doc.arena = fixed_arena_create(arena_bytes);
    doc.root = json_null();
    json_reset_error(&doc);
    return doc;
}

Json json_new(void)
{
    return json_new_cap(JSON_ARENA_SIZE);
}

void json_free(Json *doc)
{
    if (!doc)
        return;
    json_value_free(&doc->root);
    doc->root = json_null();
    fixed_arena_free(&doc->arena);
    json_reset_error(doc);
}

JsonValue json_null(void)
{
    JsonValue v = zero;
    v.type = JSON_NULL;
    return v;
}

JsonValue json_bool(bool value)
{
    JsonValue v = zero;
    v.type = JSON_BOOL;
    v.as.boolean = value;
    return v;
}

JsonValue json_number(double value)
{
    JsonValue v = zero;
    v.type = JSON_NUMBER;
    v.as.number = value;
    return v;
}

JsonValue json_string(Json *doc, const char *cstr)
{
    FDS_ASSERT(doc != NULL, "doc != NULL");
    if (!cstr)
        return json_string_sv(doc, sv_new());
    return json_string_sv(doc, sv_from_cstr(cstr));
}

JsonValue json_string_sv(Json *doc, SV sv)
{
    FDS_ASSERT(doc != NULL, "doc != NULL");
    JsonValue v = zero;
    v.type = JSON_STRING;
    v.as.string = json_intern(doc, sv.data, sv.count);
    return v;
}

JsonValue json_array(void)
{
    JsonValue v = zero;
    v.type = JSON_ARRAY;
    return v;
}

JsonValue json_object(void)
{
    JsonValue v = zero;
    v.type = JSON_OBJECT;
    return v;
}

static void json_value_free(JsonValue *value)
{
    if (!value)
        return;
    if (value->type == JSON_ARRAY)
    {
        for (size_t i = 0; i < value->as.array.count; i++)
            json_value_free(&value->as.array.items[i]);
        FDS_FREE(value->as.array.items);
        value->as.array.items = NULL;
        value->as.array.count = 0;
        value->as.array.capacity = 0;
    }
    else if (value->type == JSON_OBJECT)
    {
        for (size_t i = 0; i < value->as.object.count; i++)
            json_value_free(&value->as.object.items[i].value);
        FDS_FREE(value->as.object.items);
        value->as.object.items = NULL;
        value->as.object.count = 0;
        value->as.object.capacity = 0;
    }
    value->type = JSON_NULL;
}

void json_array_push(JsonValue *array, JsonValue item)
{
    FDS_ASSERT(array != NULL, "array != NULL");
    FDS_ASSERT(array->type == JSON_ARRAY, "json_array_push expects JSON_ARRAY");
    json_array_push_raw(&array->as.array, item);
}

static JsonMember *json_object_find(JsonObject *object, SV key)
{
    for (size_t i = 0; i < object->count; i++)
    {
        if (sv_eq(object->items[i].key, key))
            return &object->items[i];
    }
    return NULL;
}

void json_object_put_sv(Json *doc, JsonValue *object, SV key, JsonValue value)
{
    FDS_ASSERT(doc != NULL, "doc != NULL");
    FDS_ASSERT(object != NULL, "object != NULL");
    FDS_ASSERT(object->type == JSON_OBJECT, "json_object_put expects JSON_OBJECT");

    JsonMember *existing = json_object_find(&object->as.object, key);
    if (existing)
    {
        json_value_free(&existing->value);
        existing->value = value;
        return;
    }

    JsonMember member = zero;
    member.key = json_intern(doc, key.data, key.count);
    member.value = value;
    json_object_push_raw(&object->as.object, member);
}

void json_object_put(Json *doc, JsonValue *object, const char *key, JsonValue value)
{
    FDS_ASSERT(key != NULL, "key != NULL");
    json_object_put_sv(doc, object, sv_from_cstr(key), value);
}

JsonValue *json_object_get(JsonValue *object, const char *key)
{
    return (JsonValue *)json_object_get_c(object, key);
}

const JsonValue *json_object_get_c(const JsonValue *object, const char *key)
{
    if (!object || object->type != JSON_OBJECT || !key)
        return NULL;
    SV want = sv_from_cstr(key);
    for (size_t i = 0; i < object->as.object.count; i++)
    {
        const JsonMember *it = &object->as.object.items[i];
        if (sv_eq(it->key, want))
            return &it->value;
    }
    return NULL;
}

JsonValue *json_array_at(JsonValue *array, size_t index)
{
    return (JsonValue *)json_array_at_c(array, index);
}

const JsonValue *json_array_at_c(const JsonValue *array, size_t index)
{
    if (!array || array->type != JSON_ARRAY || index >= array->as.array.count)
        return NULL;
    return &array->as.array.items[index];
}

int json_get_bool(const JsonValue *object, const char *key, int default_val)
{
    const JsonValue *v = json_object_get_c(object, key);
    if (!v || v->type != JSON_BOOL)
        return default_val;
    return v->as.boolean ? 1 : 0;
}

double json_get_number(const JsonValue *object, const char *key, double default_val)
{
    const JsonValue *v = json_object_get_c(object, key);
    if (!v || v->type != JSON_NUMBER)
        return default_val;
    return v->as.number;
}

SV json_get_string(const JsonValue *object, const char *key, const char *default_val)
{
    const JsonValue *v = json_object_get_c(object, key);
    if (!v || v->type != JSON_STRING)
        return default_val ? sv_from_cstr(default_val) : sv_new();
    return v->as.string;
}

typedef struct
{
    Json *doc;
    SV in;
    const char *start;
    int depth;
} JsonParser;

static void json_skip_ws(JsonParser *p)
{
    while (p->in.count > 0)
    {
        char c = p->in.data[0];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            break;
        sv_remove_prefix(&p->in, 1);
    }
}

static char json_peek(const JsonParser *p)
{
    return p->in.count ? p->in.data[0] : '\0';
}

static void json_set_error(JsonParser *p, const char *fmt, ...)
{
    if (!p->doc->ok)
        return;

    p->doc->ok = false;
    const char *cur = p->in.data ? p->in.data : p->start;
    size_t line = 1;
    size_t col = 1;
    if (p->start && cur >= p->start)
    {
        for (const char *c = p->start; c < cur; ++c)
        {
            if (*c == '\n')
            {
                line++;
                col = 1;
            }
            else
            {
                col++;
            }
        }
    }
    p->doc->error_line = line;
    p->doc->error_col = col;

    va_list args;
    va_start(args, fmt);
    vsnprintf(p->doc->error, sizeof(p->doc->error), fmt, args);
    va_end(args);

    fds_log(FERROR, "JSON parse error at %zu:%zu: %s", line, col, p->doc->error);
}

static int json_hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int json_parse_hex4(JsonParser *p, unsigned *out)
{
    if (p->in.count < 4)
        return 0;
    unsigned cp = 0;
    for (int i = 0; i < 4; i++)
    {
        int n = json_hex_nibble(p->in.data[i]);
        if (n < 0)
            return 0;
        cp = (cp << 4) | (unsigned)n;
    }
    sv_remove_prefix(&p->in, 4);
    *out = cp;
    return 1;
}

static void json_utf8_append(SB *sb, unsigned cp)
{
    if (cp <= 0x7F)
    {
        sb_append_char(sb, (char)cp);
    }
    else if (cp <= 0x7FF)
    {
        sb_append_char(sb, (char)(0xC0 | (cp >> 6)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF)
    {
        sb_append_char(sb, (char)(0xE0 | (cp >> 12)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
    else
    {
        sb_append_char(sb, (char)(0xF0 | (cp >> 18)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 12) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
}

static JsonValue json_parse_value(JsonParser *p);

static JsonValue json_parse_string(JsonParser *p)
{
    if (!sv_consume_char(&p->in, '"'))
    {
        json_set_error(p, "expected string");
        return json_null();
    }

    SB sb = sb_new();
    while (p->in.count > 0)
    {
        unsigned char c = (unsigned char)p->in.data[0];
        sv_remove_prefix(&p->in, 1);

        if (c == '"')
        {
            JsonValue v = json_string_sv(p->doc, sb_to_sv(&sb));
            sb_free(&sb);
            return v;
        }
        if (c == '\\')
        {
            if (p->in.count == 0)
            {
                json_set_error(p, "unterminated string escape");
                sb_free(&sb);
                return json_null();
            }
            char e = p->in.data[0];
            sv_remove_prefix(&p->in, 1);
            switch (e)
            {
            case '"':
            case '\\':
            case '/':
                sb_append_char(&sb, e);
                break;
            case 'b':
                sb_append_char(&sb, '\b');
                break;
            case 'f':
                sb_append_char(&sb, '\f');
                break;
            case 'n':
                sb_append_char(&sb, '\n');
                break;
            case 'r':
                sb_append_char(&sb, '\r');
                break;
            case 't':
                sb_append_char(&sb, '\t');
                break;
            case 'u':
            {
                unsigned cp = 0;
                if (!json_parse_hex4(p, &cp))
                {
                    json_set_error(p, "invalid \\u escape");
                    sb_free(&sb);
                    return json_null();
                }
                if (cp >= 0xD800 && cp <= 0xDBFF)
                {
                    if (p->in.count < 6 || p->in.data[0] != '\\' || p->in.data[1] != 'u')
                    {
                        json_set_error(p, "expected low surrogate after high surrogate");
                        sb_free(&sb);
                        return json_null();
                    }
                    sv_remove_prefix(&p->in, 2);
                    unsigned low = 0;
                    if (!json_parse_hex4(p, &low) || low < 0xDC00 || low > 0xDFFF)
                    {
                        json_set_error(p, "invalid low surrogate");
                        sb_free(&sb);
                        return json_null();
                    }
                    cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF)
                {
                    json_set_error(p, "lone low surrogate");
                    sb_free(&sb);
                    return json_null();
                }
                json_utf8_append(&sb, cp);
                break;
            }
            default:
                json_set_error(p, "invalid escape \\%c", e);
                sb_free(&sb);
                return json_null();
            }
            continue;
        }
        if (c < 0x20)
        {
            json_set_error(p, "unescaped control character in string");
            sb_free(&sb);
            return json_null();
        }
        sb_append_char(&sb, (char)c);
    }

    json_set_error(p, "unterminated string");
    sb_free(&sb);
    return json_null();
}

static int json_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static JsonValue json_parse_number(JsonParser *p)
{
    SV start = p->in;
    if (sv_consume_char(&p->in, '-'))
    {
        /* sign ok */
    }

    if (p->in.count == 0 || !json_is_digit(json_peek(p)))
    {
        json_set_error(p, "invalid number");
        return json_null();
    }

    if (json_peek(p) == '0')
    {
        sv_remove_prefix(&p->in, 1);
        if (p->in.count && json_is_digit(json_peek(p)))
        {
            json_set_error(p, "leading zeros are not allowed");
            return json_null();
        }
    }
    else
    {
        while (p->in.count && json_is_digit(json_peek(p)))
            sv_remove_prefix(&p->in, 1);
    }

    if (sv_consume_char(&p->in, '.'))
    {
        if (p->in.count == 0 || !json_is_digit(json_peek(p)))
        {
            json_set_error(p, "expected digit after decimal point");
            return json_null();
        }
        while (p->in.count && json_is_digit(json_peek(p)))
            sv_remove_prefix(&p->in, 1);
    }

    char exp = json_peek(p);
    if (exp == 'e' || exp == 'E')
    {
        sv_remove_prefix(&p->in, 1);
        if (json_peek(p) == '+' || json_peek(p) == '-')
            sv_remove_prefix(&p->in, 1);
        if (p->in.count == 0 || !json_is_digit(json_peek(p)))
        {
            json_set_error(p, "expected digit in exponent");
            return json_null();
        }
        while (p->in.count && json_is_digit(json_peek(p)))
            sv_remove_prefix(&p->in, 1);
    }

    size_t len = (size_t)(p->in.data - start.data);
    char *tmp = sv_to_cstr(sv_from_parts(start.data, len));
    char *end = NULL;
    double num = strtod(tmp, &end);
    int bad = (end == tmp || *end != '\0' || !isfinite(num));
    FDS_FREE(tmp);
    if (bad)
    {
        json_set_error(p, "number out of range");
        return json_null();
    }
    return json_number(num);
}

static JsonValue json_parse_array(JsonParser *p)
{
    if (!sv_consume_char(&p->in, '['))
    {
        json_set_error(p, "expected '['");
        return json_null();
    }

    JsonValue arr = json_array();
    json_skip_ws(p);
    if (sv_consume_char(&p->in, ']'))
        return arr;

    for (;;)
    {
        JsonValue item = json_parse_value(p);
        if (!p->doc->ok)
        {
            json_value_free(&item);
            json_value_free(&arr);
            return json_null();
        }
        json_array_push(&arr, item);
        json_skip_ws(p);
        if (sv_consume_char(&p->in, ']'))
            return arr;
        if (!sv_consume_char(&p->in, ','))
        {
            json_set_error(p, "expected ',' or ']' in array");
            json_value_free(&arr);
            return json_null();
        }
        json_skip_ws(p);
    }
}

static JsonValue json_parse_object(JsonParser *p)
{
    if (!sv_consume_char(&p->in, '{'))
    {
        json_set_error(p, "expected '{'");
        return json_null();
    }

    JsonValue obj = json_object();
    json_skip_ws(p);
    if (sv_consume_char(&p->in, '}'))
        return obj;

    for (;;)
    {
        json_skip_ws(p);
        if (json_peek(p) != '"')
        {
            json_set_error(p, "expected object key");
            json_value_free(&obj);
            return json_null();
        }
        JsonValue key = json_parse_string(p);
        if (!p->doc->ok)
        {
            json_value_free(&key);
            json_value_free(&obj);
            return json_null();
        }
        json_skip_ws(p);
        if (!sv_consume_char(&p->in, ':'))
        {
            json_set_error(p, "expected ':' after object key");
            json_value_free(&key);
            json_value_free(&obj);
            return json_null();
        }
        JsonValue val = json_parse_value(p);
        if (!p->doc->ok)
        {
            json_value_free(&key);
            json_value_free(&val);
            json_value_free(&obj);
            return json_null();
        }
        json_object_put_sv(p->doc, &obj, key.as.string, val);
        json_skip_ws(p);
        if (sv_consume_char(&p->in, '}'))
            return obj;
        if (!sv_consume_char(&p->in, ','))
        {
            json_set_error(p, "expected ',' or '}' in object");
            json_value_free(&obj);
            return json_null();
        }
        json_skip_ws(p);
    }
}

static JsonValue json_parse_value(JsonParser *p)
{
    json_skip_ws(p);
    if (!p->doc->ok)
        return json_null();
    if (p->depth >= JSON_MAX_DEPTH)
    {
        json_set_error(p, "maximum nesting depth exceeded");
        return json_null();
    }

    char c = json_peek(p);
    if (c == '\0')
    {
        json_set_error(p, "unexpected end of input");
        return json_null();
    }

    p->depth++;
    JsonValue result = json_null();
    if (c == 'n')
    {
        if (!sv_consume(&p->in, sv_from_cstr("null")))
            json_set_error(p, "expected 'null'");
        else
            result = json_null();
    }
    else if (c == 't')
    {
        if (!sv_consume(&p->in, sv_from_cstr("true")))
            json_set_error(p, "expected 'true'");
        else
            result = json_bool(true);
    }
    else if (c == 'f')
    {
        if (!sv_consume(&p->in, sv_from_cstr("false")))
            json_set_error(p, "expected 'false'");
        else
            result = json_bool(false);
    }
    else if (c == '"')
    {
        result = json_parse_string(p);
    }
    else if (c == '{')
    {
        result = json_parse_object(p);
    }
    else if (c == '[')
    {
        result = json_parse_array(p);
    }
    else if (c == '-' || json_is_digit(c))
    {
        result = json_parse_number(p);
    }
    else
    {
        json_set_error(p, "unexpected character '%c'", c);
    }
    p->depth--;
    return result;
}

static Json json_parse_sv_impl(SV content, size_t arena_bytes)
{
    Json doc = json_new_cap(arena_bytes);
    if (content.count >= 3 &&
        (unsigned char)content.data[0] == 0xEF &&
        (unsigned char)content.data[1] == 0xBB &&
        (unsigned char)content.data[2] == 0xBF)
    {
        sv_remove_prefix(&content, 3);
    }

    JsonParser p = zero;
    p.doc = &doc;
    p.in = content;
    p.start = content.data;
    p.depth = 0;

    doc.root = json_parse_value(&p);
    if (!doc.ok)
    {
        json_value_free(&doc.root);
        doc.root = json_null();
        return doc;
    }

    json_skip_ws(&p);
    if (p.in.count > 0)
    {
        json_set_error(&p, "trailing data after JSON value");
        json_value_free(&doc.root);
        doc.root = json_null();
    }
    return doc;
}

Json json_parse_sv(SV content)
{
    size_t cap = content.count + JSON_ARENA_SIZE;
    if (cap < JSON_ARENA_SIZE)
        cap = JSON_ARENA_SIZE;
    return json_parse_sv_impl(content, cap);
}

Json json_parse_cstr(const char *text)
{
    FDS_ASSERT(text != NULL, "text != NULL");
    return json_parse_sv(sv_from_cstr(text));
}

Json json_parse_sb(const SB *content)
{
    FDS_ASSERT(content != NULL, "content != NULL");
    return json_parse_sv(sb_to_sv(content));
}

Json json_parse(const char *filepath)
{
    FDS_ASSERT(filepath != NULL, "filepath != NULL");
    size_t file_size = fds_get_file_size(filepath);
    if (file_size == (size_t)-1)
    {
        Json doc = json_new();
        doc.ok = false;
        snprintf(doc.error, sizeof(doc.error), "cannot read file %s", filepath);
        fds_log(FERROR, "%s", doc.error);
        return doc;
    }

    Json doc = json_new_cap(file_size + JSON_ARENA_SIZE);
    SV content;
    if (fds_file_read_to_arena(filepath, &doc.arena, &content) != 0)
    {
        doc.ok = false;
        snprintf(doc.error, sizeof(doc.error), "cannot read file %s", filepath);
        fds_log(FERROR, "%s", doc.error);
        return doc;
    }

    if (content.count >= 3 &&
        (unsigned char)content.data[0] == 0xEF &&
        (unsigned char)content.data[1] == 0xBB &&
        (unsigned char)content.data[2] == 0xBF)
    {
        sv_remove_prefix(&content, 3);
    }

    JsonParser p = zero;
    p.doc = &doc;
    p.in = content;
    p.start = content.data;
    p.depth = 0;
    doc.root = json_parse_value(&p);
    if (!doc.ok)
    {
        json_value_free(&doc.root);
        doc.root = json_null();
        return doc;
    }
    json_skip_ws(&p);
    if (p.in.count > 0)
    {
        json_set_error(&p, "trailing data after JSON value");
        json_value_free(&doc.root);
        doc.root = json_null();
    }
    return doc;
}

static void json_append_indent(SB *sb, int indent, int level)
{
    int n = indent * level;
    for (int i = 0; i < n; i++)
        sb_append_char(sb, ' ');
}

static void json_append_escaped(SB *sb, SV sv)
{
    sb_append_char(sb, '"');
    for (size_t i = 0; i < sv.count; i++)
    {
        unsigned char c = (unsigned char)sv.data[i];
        switch (c)
        {
        case '"':
            sb_append(sb, "\\\"");
            break;
        case '\\':
            sb_append(sb, "\\\\");
            break;
        case '\b':
            sb_append(sb, "\\b");
            break;
        case '\f':
            sb_append(sb, "\\f");
            break;
        case '\n':
            sb_append(sb, "\\n");
            break;
        case '\r':
            sb_append(sb, "\\r");
            break;
        case '\t':
            sb_append(sb, "\\t");
            break;
        default:
            if (c < 0x20)
                sb_appendf(sb, "\\u%04x", c);
            else
                sb_append_char(sb, (char)c);
            break;
        }
    }
    sb_append_char(sb, '"');
}

static void json_append_impl(SB *sb, const JsonValue *value, int indent, int level)
{
    if (!value)
    {
        sb_append(sb, "null");
        return;
    }

    switch (value->type)
    {
    case JSON_NULL:
        sb_append(sb, "null");
        break;
    case JSON_BOOL:
        sb_append(sb, value->as.boolean ? "true" : "false");
        break;
    case JSON_NUMBER:
        if (!isfinite(value->as.number))
        {
            sb_append(sb, "null");
            break;
        }
        {
            double n = value->as.number;
            if (n == (double)(int64_t)n && n >= (double)INT64_MIN && n <= (double)INT64_MAX)
                sb_appendf(sb, "%lld", (long long)(int64_t)n);
            else
                sb_appendf(sb, "%.17g", n);
        }
        break;
    case JSON_STRING:
        json_append_escaped(sb, value->as.string);
        break;
    case JSON_ARRAY:
        sb_append_char(sb, '[');
        if (value->as.array.count == 0)
        {
            sb_append_char(sb, ']');
            break;
        }
        for (size_t i = 0; i < value->as.array.count; i++)
        {
            if (indent > 0)
            {
                sb_append_char(sb, '\n');
                json_append_indent(sb, indent, level + 1);
            }
            json_append_impl(sb, &value->as.array.items[i], indent, level + 1);
            if (i + 1 < value->as.array.count)
                sb_append_char(sb, ',');
        }
        if (indent > 0)
        {
            sb_append_char(sb, '\n');
            json_append_indent(sb, indent, level);
        }
        sb_append_char(sb, ']');
        break;
    case JSON_OBJECT:
        sb_append_char(sb, '{');
        if (value->as.object.count == 0)
        {
            sb_append_char(sb, '}');
            break;
        }
        for (size_t i = 0; i < value->as.object.count; i++)
        {
            if (indent > 0)
            {
                sb_append_char(sb, '\n');
                json_append_indent(sb, indent, level + 1);
            }
            json_append_escaped(sb, value->as.object.items[i].key);
            sb_append(sb, indent > 0 ? ": " : ":");
            json_append_impl(sb, &value->as.object.items[i].value, indent, level + 1);
            if (i + 1 < value->as.object.count)
                sb_append_char(sb, ',');
        }
        if (indent > 0)
        {
            sb_append_char(sb, '\n');
            json_append_indent(sb, indent, level);
        }
        sb_append_char(sb, '}');
        break;
    }
}

void json_append(SB *sb, const JsonValue *value)
{
    FDS_ASSERT(sb != NULL, "sb != NULL");
    json_append_impl(sb, value, 0, 0);
}

void json_append_pretty(SB *sb, const JsonValue *value, int indent)
{
    FDS_ASSERT(sb != NULL, "sb != NULL");
    if (indent <= 0)
        indent = JSON_PRETTY_INDENT;
    json_append_impl(sb, value, indent, 0);
}

SB json_stringify(const JsonValue *value)
{
    SB sb = sb_new();
    json_append(&sb, value);
    return sb;
}

SB json_stringify_pretty(const JsonValue *value, int indent)
{
    SB sb = sb_new();
    json_append_pretty(&sb, value, indent);
    return sb;
}

char *json_to_cstr(const JsonValue *value)
{
    SB sb = json_stringify(value);
    char *out = sv_to_cstr(sb_to_sv(&sb));
    sb_free(&sb);
    return out;
}

void json_print(const JsonValue *value)
{
    SB sb = json_stringify_pretty(value, JSON_PRETTY_INDENT);
    printf("%s\n", sb_to_cstr(&sb));
    sb_free(&sb);
}

int json_write_file(const char *filepath, const JsonValue *value, int pretty)
{
    SB sb = pretty ? json_stringify_pretty(value, pretty) : json_stringify(value);
    int rc = fds_file_write_sb(filepath, &sb);
    sb_free(&sb);
    return rc;
}
