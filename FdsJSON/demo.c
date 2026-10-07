#define FDS_IMPL
#include "json.h"

static int g_failed = 0;

static void expect(int cond, const char *msg)
{
    if (!cond)
    {
        fds_log(FERROR, "FAIL: %s", msg);
        g_failed++;
    }
    else
    {
        fds_log(FINFO, "ok: %s", msg);
    }
}

int main(void)
{
    Json doc = json_new();
    doc.root = json_object();

    json_object_put(&doc, &doc.root, "name", json_string(&doc, "FdsJSON"));
    json_object_put(&doc, &doc.root, "version", json_number(1));
    json_object_put(&doc, &doc.root, "ok", json_bool(true));
    json_object_put(&doc, &doc.root, "empty", json_null());

    JsonValue tags = json_array();
    json_array_push(&tags, json_string(&doc, "parse"));
    json_array_push(&tags, json_string(&doc, "stringify"));
    json_object_put(&doc, &doc.root, "tags", tags);

    JsonValue nested = json_object();
    json_object_put(&doc, &nested, "x", json_number(3.5));
    json_object_put(&doc, &doc.root, "point", nested);

    printf("--- generated ---\n");
    json_print(&doc.root);

    SB text = json_stringify_pretty(&doc.root, 2);
    json_write_file("sample.json", &doc.root, 2);

    Json parsed = json_parse_sb(&text);
    expect(parsed.ok, "parse generated JSON");
    expect(sv_eq_cstr(json_get_string(&parsed.root, "name", ""), "FdsJSON"), "name");
    expect(json_get_bool(&parsed.root, "ok", 0) == 1, "ok bool");
    expect(json_get_number(&parsed.root, "version", 0) == 1.0, "version");
    JsonValue *arr = json_object_get(&parsed.root, "tags");
    expect(arr && arr->type == JSON_ARRAY && arr->as.array.count == 2, "tags array");
    JsonValue *point = json_object_get(&parsed.root, "point");
    expect(json_get_number(point, "x", 0) == 3.5, "nested number");

    Json file_doc = json_parse("sample.json");
    expect(file_doc.ok, "parse sample.json");

    Json uni = json_parse_cstr("{\"smile\":\"\\uD83D\\uDE00\",\"q\":\"a\\nb\"}");
    expect(uni.ok, "unicode surrogate pair");
    SV smile = json_get_string(&uni.root, "smile", "");
    expect(smile.count == 4, "utf-8 emoji length");

    Json num = json_parse_cstr("[-12.5e+2,0,true,false,null,[]]");
    expect(num.ok, "mixed array");
    expect(json_array_at(&num.root, 0)->as.number == -1250.0, "scientific number");

    Json bad = json_parse_cstr("{oops}");
    expect(!bad.ok, "reject invalid JSON");

    Json trail = json_parse_cstr("true false");
    expect(!trail.ok, "reject trailing tokens");

    json_free(&doc);
    json_free(&parsed);
    json_free(&file_doc);
    json_free(&uni);
    json_free(&num);
    json_free(&bad);
    json_free(&trail);
    sb_free(&text);

    if (g_failed)
    {
        fds_log(FERROR, "%d test(s) failed", g_failed);
        return 1;
    }
    fds_log(FINFO, "all tests passed");
    return 0;
}
