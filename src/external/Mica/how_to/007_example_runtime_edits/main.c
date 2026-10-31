#include "../../mica.h"
#include <stdio.h>

static void report(MicaSetResult result, const char *action)
{
    printf("%s: %s\n", action, result == MICA_SET_OK ? "ok" : "not changed");
}

int main(void)
{
    MicaContext *ctx = mica_parse(
        "config = {\n"
        "    theme = \"dark\"\n"
        "    out_path = \"./build\"\n"
        "    width = 1280\n"
        "    aspect_ratio = .width / 720\n"
        "}\n");

    /* Adds a missing leaf to an existing object. */
    report(mica_set_string(&ctx, "config.input_path", "./src"), "input_path");

    /* Creates every absent intermediate object. */
    report(mica_set_string(&ctx, "runtime.cache.path", "./cache"), "cache path");

    /* Computed values are protected by default. */
    report(mica_set_float(&ctx, "config.aspect_ratio", 1.777), "aspect ratio");

    /* Use force only when replacing the expression is deliberate. */
    report(mica_set_float_force(&ctx, "config.aspect_ratio", 1.777), "forced aspect ratio");

    puts("\nUpdated configuration:");
    puts(mica_source(ctx));
    mica_destroy(ctx);
    return 0;
}
