#include "../../mica.h"
#include <stdio.h>

int main(void)
{
    MicaContext *ctx = mica_parse(
        "network = { retries = 3\n"
        "  timeout = 10 / 0\n"
        "  safe_mode = false && (1 / 0 > 0) || true }\n");

    printf("retries = %.0f\n", mica_get_float(ctx, "network.retries", 1));
    printf("missing = %.0f\n", mica_get_float(ctx, "network.unknown", 99));
    printf("broken timeout = %.0f\n", mica_get_float(ctx, "network.timeout", 30));
    printf("safe mode = %s\n",
           mica_get_bool(ctx, "network.safe_mode", 0) ? "true" : "false");
    mica_destroy(ctx);
    return 0;
}
