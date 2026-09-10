#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FDS_EXT_CFG_IMPLEMENTATION
#include "fds_Onyx.h"

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static Fds_Cfg_Doc *parse_text(const char *source, Fds_Cfg_Error *error) {
    return fds_cfg_parse((fds_string_view){source, strlen(source)}, error);
}

static void test_values_and_expressions(void) {
    const char *source =
        "int_value = 7;\n"
        "float_value = 2.5;\n"
        "bool_value = true;\n"
        "text_value = \"ok\";\n"
        "sum = 2 + 3 * 4;\n"
        "logic = true && !false;\n"
        "comparison = 3 >= 3;\n"
        "chosen = true ? \"yes\" : \"no\";\n";
    Fds_Cfg_Error error = {0};
    Fds_Cfg_Doc *doc = parse_text(source, &error);

    CHECK(doc != NULL);
    CHECK(error.code == FDS_CFG_OK);
    CHECK(fds_cfg_get_type(doc, "int_value") == FDS_CFG_INT);
    CHECK(fds_cfg_get_int(doc, "int_value", -1) == 7);
    CHECK(fabs(fds_cfg_get_float(doc, "float_value", 0.0) - 2.5) < 0.000001);
    CHECK(fds_cfg_get_bool(doc, "bool_value", false));
    CHECK(strcmp(fds_cfg_get_string(doc, "text_value", ""), "ok") == 0);
    CHECK(fds_cfg_get_int(doc, "sum", -1) == 14);
    CHECK(fds_cfg_get_bool(doc, "logic", false));
    CHECK(fds_cfg_get_bool(doc, "comparison", false));
    CHECK(strcmp(fds_cfg_get_string(doc, "chosen", ""), "yes") == 0);
    CHECK(fds_cfg_get_int(doc, "missing", 123) == 123);
    fds_cfg_free(doc);
}

static void test_functions_and_conditions(void) {
    const char *source =
        "def add(a, b) = a + b;\n"
        "value = add(2, 5);\n"
        "enabled = false;\n"
        "@if enabled {\n"
        "    selected = 1;\n"
        "} @else {\n"
        "    selected = value;\n"
        "}\n"
        "@assert selected == 7, \"wrong selected value\";\n";
    Fds_Cfg_Error error = {0};
    Fds_Cfg_Doc *doc = parse_text(source, &error);

    CHECK(doc != NULL);
    CHECK(fds_cfg_get_int(doc, "value", -1) == 7);
    CHECK(fds_cfg_get_int(doc, "selected", -1) == 7);
    CHECK(!fds_cfg_has_key(doc, "unused"));
    fds_cfg_free(doc);

    error = (Fds_Cfg_Error){0};
    doc = parse_text("@assert false, \"must fail\";\n", &error);
    CHECK(doc == NULL);
    CHECK(error.code == FDS_CFG_ERR_ASSERT_FAILED);
    CHECK(error.line == 1);
    CHECK(error.column == 1);
    CHECK(error.msg != NULL);
}

static void test_runtime_errors(void) {
    Fds_Cfg_Error error = {0};
    Fds_Cfg_Doc *doc = parse_text("x = 1 / 0;\n", &error);
    CHECK(doc == NULL);
    CHECK(error.code == FDS_CFG_ERR_DIV_BY_ZERO);
    CHECK(error.line == 1);
    CHECK(error.column == 1);
    CHECK(error.msg != NULL);

    error = (Fds_Cfg_Error){0};
    doc = parse_text("y = unknown + 5;\n", &error);
    CHECK(doc == NULL);
    CHECK(error.code == FDS_CFG_ERR_UNDEFINED_VAR);
    CHECK(error.line == 1);
    CHECK(error.column == 1);
    CHECK(error.msg != NULL);

    error = (Fds_Cfg_Error){0};
    doc = parse_text("broken = true + 1;\n", &error);
    CHECK(doc == NULL);
    CHECK(error.code == FDS_CFG_ERR_TYPE_MISMATCH);
    CHECK(error.line == 1);
    CHECK(error.column == 1);
}

static void test_file_loading(void) {
    const char *filename = "test_onyx_format.cfg";
    const char *source = "loaded = 123;\n";
    FILE *file = fopen(filename, "wb");
    CHECK(file != NULL);
    if (!file) return;
    CHECK(fwrite(source, 1, strlen(source), file) == strlen(source));
    fclose(file);

    Fds_Cfg_Error error = {0};
    Fds_Cfg_Doc *doc = fds_cfg_load(filename, &error);
    CHECK(doc != NULL);
    CHECK(fds_cfg_get_int(doc, "loaded", -1) == 123);
    fds_cfg_free(doc);
    remove(filename);
}

static void test_binary_loading(void) {
    const size_t string_size = sizeof("answer");
    const size_t blob_size = sizeof(Fds_Blob_Header) + sizeof(Fds_Blob_Entry) + string_size;
    unsigned char *data = calloc(1, blob_size);
    CHECK(data != NULL);
    if (!data) return;

    Fds_Blob_Header *header = (Fds_Blob_Header *)data;
    Fds_Blob_Entry *entry = (Fds_Blob_Entry *)(data + sizeof(Fds_Blob_Header));
    char *strings = (char *)(data + sizeof(Fds_Blob_Header) + sizeof(Fds_Blob_Entry));
    header->magic = FDS_CFG_MAGIC;
    header->version = FDS_CFG_VERSION;
    header->total_size = (uint32_t)blob_size;
    header->entry_count = 1;
    header->entries_offset = (uint32_t)sizeof(Fds_Blob_Header);
    header->strings_offset = (uint32_t)(sizeof(Fds_Blob_Header) + sizeof(Fds_Blob_Entry));
    entry->name_offset = 0;
    entry->type = FDS_CFG_INT;
    entry->as.i_val = 42;
    memcpy(strings, "answer", string_size);

    Fds_Cfg_Error error = {0};
    Fds_Cfg_Doc *doc = fds_cfg_load_binary(data, blob_size, &error);
    CHECK(doc != NULL);
    CHECK(fds_cfg_get_int(doc, "answer", -1) == 42);
    fds_cfg_free(doc);

    header->total_size = (uint32_t)(blob_size + 1);
    error = (Fds_Cfg_Error){0};
    doc = fds_cfg_load_binary(data, blob_size, &error);
    CHECK(doc == NULL);
    CHECK(error.code == FDS_CFG_ERR_SYNTAX);
    free(data);
}

int main(void) {
    test_values_and_expressions();
    test_functions_and_conditions();
    test_runtime_errors();
    test_file_loading();
    test_binary_loading();

    if (failures != 0) {
        fprintf(stderr, "Onyx format tests failed: %d\n", failures);
        return 1;
    }
    puts("Onyx format tests passed");
    return 0;
}
