# Mica v0.1 (C)

`mica.c` and `mica.h` provide a dependency-free C99 implementation of the described declarative configuration format. The detailed Ukrainian-language reference is in [docs/mica-v0.1.md](docs/mica-v0.1.md); a realistic configuration is in [examples/complex.mica](examples/complex.mica).

Supported: newline/`;` separators, objects, lexical and relative references, arithmetic, object merge, lazy ternaries, `clamp`, `avg`, `min`, `max`, `abs`, `${...}` interpolation, raw `'''...'''` strings, memoized AST nodes, cycle detection, and typed fallback getters. A repeated field in one object overrides its earlier declaration.

In a relative reference one dot denotes the expression's current object scope; every additional dot moves up one parent. This is the interpretation required by the supplied example: `.width` in `window` and `..debug` in `window` resolve as documented there.

## Expressions

Numeric comparisons are `==`, `!=`, `<`, `<=`, `>`, and `>=`; strings support `==` and `!=`. Boolean operators are `!`, `&&`, and `||`. Both `&&` and `||`, as well as `?:`, evaluate only the required branch, so this is safe:

```mica
safe = false && (1 / 0 > 0) || true
profile = debug && width >= 1280 ? "development" : "release"
fps = max(60, min(target_fps, 240))
```

Build the demo with `make` (or `cc -std=c99 -Wall -Wextra -Wpedantic mica.c example.c -o example`).

```c
MicaContext *ctx = mica_parse(text);
double size = mica_get_float(ctx, "engine.render.buffer_size", 2048.0);
mica_destroy(ctx);
```

## Benchmark

`benchmark.c` measures two distinct paths with the same nontrivial configuration: parsing plus its first dependency resolution (cold path), then repeated reads from one resolved AST (memoized path). It reads numeric, boolean, and interpolated-string values, and prints a checksum so the optimizer cannot remove the work.

```sh
cc -std=c99 -O2 -Wall -Wextra -pedantic mica.c benchmark.c -o benchmark
./benchmark             # 10,000 iterations by default
./benchmark 100000      # explicit iteration count
```

Use an optimized build for timing. CPU frequency, compiler version, platform and iteration count should accompany any published result; timing numbers are not portable across machines.

## Runtime edits

`mica_set_float`, `mica_set_bool`, and `mica_set_string` edit a literal or create a missing path; pass `&ctx` because a successful edit reparses and replaces the context. Existing computed expressions are protected by default. Use the explicit `*_force` variants only to intentionally replace one. See [docs/mica-v0.1.md](docs/mica-v0.1.md) for result codes and formatting rules.

## Learning by example

The ordered [how_to](how_to/README.md) directory contains seven compact examples, from basic getters to safe runtime edits. Each topic has its own `001_example_name`-style directory with a configuration, a compilable C program, and a short explanation.
