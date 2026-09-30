#include <stdio.h>
#include "mica.h"

#define FDS_IMPL
#include "fds.h"

int main()
{
    SB config = sb_new();
    fds_file_read_to_sb("examples/complex.mica", &config);
    MicaContext *ctx = mica_parse(config.items);
    if (mica_last_error(ctx)) { fprintf(stderr, "parse error: %s\n", mica_last_error(ctx)); return 1; }
    // char *str = mica_get_string(ctx,"app.ui.window.legal_notice", "cringe");
    char *str = (char*)mica_get_string(ctx,"app.ui.window.title", "cringe");
    char* estr = (char*)mica_get_string(ctx,"app.ui.window.legal_notice", "ds");
    int width = (int)mica_get_float(ctx, "desktop_window.tokens", 13222);
    printf("%s\n%s\n", str, estr);
    printf("%d\n", width);


    mica_destroy(ctx);
    sb_free(&config);
}