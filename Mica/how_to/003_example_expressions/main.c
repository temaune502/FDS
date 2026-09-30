#include "../../mica.h"
#include <stdio.h>

int main(void)
{
    MicaContext *ctx = mica_parse(
        "build = { debug = true; width = 1920; height = 1080\n"
        "  pixels = .width * .height\n"
        "  high_resolution = .width >= 1920 && .height >= 1080\n"
        "  target_fps = .debug ? 60 : 144\n"
        "  buffer_size = clamp(.pixels / 2, 1024, 4194304) }\n");

    printf("%.0f px, %.0f fps, high-res=%s, buffer=%.0f\n",
           mica_get_float(ctx, "build.pixels", 0),
           mica_get_float(ctx, "build.target_fps", 0),
           mica_get_bool(ctx, "build.high_resolution", 0) ? "yes" : "no",
           mica_get_float(ctx, "build.buffer_size", 0));
    mica_destroy(ctx);
    return 0;
}
