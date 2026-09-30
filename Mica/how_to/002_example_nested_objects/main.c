#include "../../mica.h"
#include <stdio.h>

int main(void)
{
    MicaContext *ctx = mica_parse(
        "server = {\n"
        "  host = \"127.0.0.1\"\n"
        "  http = { port = 8080; keep_alive = true }\n"
        "}\n");

    printf("http://%s:%.0f, keep-alive=%s\n",
           mica_get_string(ctx, "server.host", "localhost"),
           mica_get_float(ctx, "server.http.port", 80),
           mica_get_bool(ctx, "server.http.keep_alive", 0) ? "yes" : "no");
    mica_destroy(ctx);
    return 0;
}
