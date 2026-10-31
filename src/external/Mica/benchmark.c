#include "mica.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Keep this source self-contained: the benchmark does not depend on a cwd. */
static const char *benchmark_config =
    "app = {\n"
    " name = \"Mica Benchmark\"\n"
    " debug = true\n"
    " metrics = { enabled = true; sample_rate = 0.25 }\n"
    " server = { workers = 8; max_connections = 1200 }\n"
    " ui = { window = { width = 1600; height = 900\n"
    "   title = \"${...name} ${.width}x${.height}\" } }\n"
    " renderer = { diagnostics = ..debug && ..metrics.enabled\n"
    "   target_fps = .diagnostics ? 60 : 144\n"
    "   scale = .target_fps >= 120 ? 1.0 : 1.25\n"
    "   width = ..ui.window.width * .scale\n"
    "   height = ..ui.window.height * .scale\n"
    "   buffer_size = clamp(.width * .height / 2, 1024, 4194304)\n"
    "   quality = avg(.target_fps, ..server.workers * 10, 95)\n"
    "   safe = false && (1 / 0 > 0) || true }\n"
    "}\n"
    "desktop = app.ui.window + { width = 2560; height = 1440 }\n";

static double elapsed_ms(clock_t start, clock_t finish) {
    return 1000.0 * (double)(finish - start) / (double)CLOCKS_PER_SEC;
}

static unsigned parse_iterations(int argc, char **argv) {
    char *end;
    unsigned long value;
    if (argc < 2) return 10000;
    value = strtoul(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || value == 0 || value > 100000000UL) {
        fprintf(stderr, "usage: %s [positive iteration count up to 100000000]\n", argv[0]);
        exit(2);
    }
    return (unsigned)value;
}

static void ensure_valid(MicaContext *ctx) {
    const char *error = mica_last_error(ctx);
    if (error) {
        fprintf(stderr, "benchmark config parse error: %s\n", error);
        mica_destroy(ctx);
        exit(1);
    }
}

int main(int argc, char **argv) {
    
    unsigned iterations = parse_iterations(argc, argv);
    unsigned i;
    double checksum = 0.0;
    clock_t start, finish;
    MicaContext *ctx;

    /* Parse and force first evaluation: measures the complete cold path. */
    start = clock();
    for (i = 0; i < iterations; i++) {
        ctx = mica_parse(benchmark_config);
        ensure_valid(ctx);
        checksum += mica_get_float(ctx, "app.renderer.buffer_size", 0.0);
        checksum += mica_get_float(ctx, "app.renderer.quality", 0.0);
        checksum += mica_get_float(ctx, "desktop.width", 0.0);
        checksum += mica_get_bool(ctx, "app.renderer.safe", 0);
        checksum += strlen(mica_get_string(ctx, "app.ui.window.title", ""));
        mica_destroy(ctx);
    }
    finish = clock();
    printf("cold parse + first resolution: %10.3f ms total, %8.3f us/iteration\n",
           elapsed_ms(start, finish), 1000.0 * elapsed_ms(start, finish) / iterations);

    /* Read the same resolved AST repeatedly: measures memoized lookup cost. */
    ctx = mica_parse(benchmark_config);
    ensure_valid(ctx);
    (void)mica_get_float(ctx, "app.renderer.buffer_size", 0.0);
    (void)mica_get_float(ctx, "app.renderer.quality", 0.0);
    (void)mica_get_float(ctx, "desktop.width", 0.0);
    (void)mica_get_bool(ctx, "app.renderer.safe", 0);
    (void)mica_get_string(ctx, "app.ui.window.title", "");

    start = clock();
    for (i = 0; i < iterations; i++) {
        checksum += mica_get_float(ctx, "app.renderer.buffer_size", 0.0);
        checksum += mica_get_float(ctx, "app.renderer.quality", 0.0);
        checksum += mica_get_float(ctx, "desktop.width", 0.0);
        checksum += mica_get_bool(ctx, "app.renderer.safe", 0);
        checksum += strlen(mica_get_string(ctx, "app.ui.window.title", ""));
    }
    finish = clock();
    printf("memoized reads (5 getters): %10.3f ms total, %8.3f us/iteration\n",
           elapsed_ms(start, finish), 1000.0 * elapsed_ms(start, finish) / iterations);
    printf("iterations: %u; checksum: %.0f\n", iterations, checksum);
    mica_destroy(ctx);
    return 0;
}
