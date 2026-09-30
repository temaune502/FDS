#include "../../mica.h"
#include <stdio.h>

int main(void)
{
    MicaContext *ctx = mica_parse(
        "base_window = { width = 1280; height = 720\n"
        " title = \"Base ${.width}x${.height}\" }\n"
        "desktop_window = base_window + { width = 2560; height = 1440 }\n");

    printf("%s\n",
           mica_get_string(ctx, "desktop_window.title", "Untitled"));
    mica_destroy(ctx);
    return 0;
}
