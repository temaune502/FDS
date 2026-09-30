#include "mica.h"
#include <stdio.h>

int main(void) {
    const char *config =
        "engine = { debug = true\n"
        " window = { width = 1280; height = 720; aspect = .width / .height\n"
        " title = \"Mica ${.width}x${.height}, debug=${..debug}\" }\n"
        " render = { is_debug_build = ..debug && ..window.width >= 1280\n"
        " max_fps = .is_debug_build ? 60 : 144\n"
        " scale = .max_fps / 60.0\n"
        " buffer = clamp(..window.width * .scale, 1024, 4096)\n"
        " tuned_fps = max(.max_fps, 90) - min(10, 4) + abs(-2)\n"
        " safe_logic = false && (1 / 0 > 0) || true } }\n"
        "dev_window = engine.window + { width = 1920\n height = 1080 }\n";
    MicaContext *ctx = mica_parse(config);
    if (mica_last_error(ctx)) { fprintf(stderr, "parse error: %s\n", mica_last_error(ctx)); return 1; }
    printf("buffer = %.0f\n", mica_get_float(ctx, "engine.render.buffer", 2048));
    printf("title = %s\n", mica_get_string(ctx, "engine.window.title", "<missing>"));
    printf("dev width = %.0f\n", mica_get_float(ctx, "dev_window.width", 0));
    printf("tuned fps = %.0f\n", mica_get_float(ctx, "engine.render.tuned_fps", 0));
    printf("safe logic = %s\n", mica_get_bool(ctx, "engine.render.safe_logic", 0) ? "true" : "false");
    mica_destroy(ctx);
    return 0;
}
