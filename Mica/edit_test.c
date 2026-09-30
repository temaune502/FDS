#include "mica.h"

#include <stdio.h>
#include <string.h>

static int check(MicaSetResult got, MicaSetResult expected, const char *label)
{
    if (got == expected)
        return 1;
    fprintf(stderr, "%s: expected %d, got %d\n", label, (int)expected, (int)got);
    return 0;
}

int main(void)
{
    MicaContext *ctx = mica_parse("config = { theme = \"dark\"; out_path = \"./build\"; note = '''old''' }\n"
                                  "computed = { width = 800; height = .width / 2 }\n");
    const char *source;

    if (mica_last_error(ctx) ||
        !check(mica_set_string(&ctx, "config.input_path", "./src"), MICA_SET_OK, "new field") ||
        !check(mica_set_string(&ctx, "runtime.cache.path", "./cache"), MICA_SET_OK, "new objects") ||
        !check(mica_set_string(&ctx, "config.note", "new\nvalue"), MICA_SET_OK, "raw string") ||
        !check(mica_set_float(&ctx, "computed.height", 600), MICA_SET_IS_EXPRESSION, "protect expression") ||
        !check(mica_set_float_force(&ctx, "computed.height", 600), MICA_SET_OK, "force expression") ||
        strcmp(mica_get_string(ctx, "config.input_path", ""), "./src") ||
        strcmp(mica_get_string(ctx, "runtime.cache.path", ""), "./cache") ||
        mica_get_float(ctx, "computed.height", 0) != 600)
    {
        mica_destroy(ctx);
        return 1;
    }

    source = mica_source(ctx);
    if (!strstr(source, "out_path = \"./build\"") ||
        !strstr(source, "input_path = \"./src\"") ||
        !strstr(source, "note = '''new\nvalue'''") ||
        !strstr(source, "runtime = {") ||
        !strstr(source, "height = 600"))
    {
        fputs("source formatting/content check failed\n", stderr);
        mica_destroy(ctx);
        return 1;
    }
    puts(source);
    mica_destroy(ctx);
    return 0;
}
