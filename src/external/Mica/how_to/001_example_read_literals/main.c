#include "../../mica.h"
#include <stdio.h>

int main(void)
{
    MicaContext *ctx = mica_parse(
        "app_name = \"Mica Demo\"\n"
        "port = 8080\n"
        "enabled = true\n");

    if (mica_last_error(ctx)) return 1;
    printf("%s on port %.0f (%s)\n",
           mica_get_string(ctx, "app_name", "Unnamed"),
           mica_get_float(ctx, "port", 80),
           mica_get_bool(ctx, "enabled", 0) ? "enabled" : "disabled");
    mica_destroy(ctx);
    return 0;
}
