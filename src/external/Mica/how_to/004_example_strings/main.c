#include "../../mica.h"
#include <stdio.h>

int main(void)
{
    MicaContext *ctx = mica_parse(
        "window = { name = \"Mica Studio\"; width = 1280; height = 720\n"
        "  title = \"${.name} — ${.width}x${.height}\"\n"
        "  license = '''Copyright 2026.\n${this_is_literal}''' }\n");

    printf("title: %s\nlicense:\n%s\n",
           mica_get_string(ctx, "window.title", "Untitled"),
           mica_get_string(ctx, "window.license", ""));
    mica_destroy(ctx);
    return 0;
}
