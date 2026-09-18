#include <assert.h>
#include <stdint.h>

#define DEBUG_MEM

#define FDS2_ALLOCATOR_IMPLEMENTATION
#include "fds2_dsl.h"
#include "raylib.h"

typedef struct ray_host { fds2_dsl_context *context; } ray_host;

static bool ray_result(fds2_dsl_vm *vm, int value, fds2_dsl_value *result) {
    return fds2_dsl_return_i32(vm, result, value);
}

static bool native_should_close(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    ray_host *host = (ray_host *)user_data;
    (void)host; (void)arguments;
    return count == 0 && ray_result(vm, WindowShouldClose() ? 1 : 0, result);
}

static bool native_key_down(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    ray_host *host = (ray_host *)user_data;
    int32_t key;
    KeyboardKey keys[] = { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN };
    (void)vm;
    if (!fds2_dsl_arg_i32(vm, arguments, count, 0, &key)) return false;
    if (key < 0 || key >= 4) return false;
    return ray_result(vm, IsKeyDown(keys[key]) ? 1 : 0, result);
}

static bool native_begin(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    (void)arguments; (void)user_data;
    if (count != 0) return false;
    BeginDrawing();
    return ray_result(vm, 0, result);
}

static bool native_end(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    (void)arguments; (void)user_data;
    if (count != 0) return false;
    EndDrawing();
    return ray_result(vm, 0, result);
}

static bool native_clear(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    (void)arguments; (void)user_data;
    if (count != 0) return false;
    ClearBackground((Color){ 18, 22, 32, 255 });
    return ray_result(vm, 0, result);
}

static bool native_draw_scene(fds2_dsl_vm *vm, const fds2_dsl_value *arguments, size_t count, fds2_dsl_value *result, void *user_data) {
    ray_host *host = (ray_host *)user_data;
    int32_t x;
    int32_t y;
    if (!fds2_dsl_arg_i32(vm, arguments, count, 0, &x) || !fds2_dsl_arg_i32(vm, arguments, count, 1, &y)) return false;
    DrawText("FDS2 DSL + Raylib", 20, 18, 24, RAYWHITE);
    DrawText("The game loop, input and update run inside DSL", 20, 50, 18, LIGHTGRAY);
    DrawText("Arrow keys move the square", 20, 76, 18, LIGHTGRAY);
    DrawRectangle(x - 20, y - 20, 40, 40, SKYBLUE);
    DrawCircle(x, y, 7.0f, WHITE);
    return ray_result(vm, 0, result);
}

static bool register_raylib(fds2_dsl_context *context, ray_host *host) {
    const fds2_dsl_type *none[] = { NULL };
    const fds2_dsl_type *one[] = { &context->builtin_i32 };
    const fds2_dsl_type *two[] = { &context->builtin_i32, &context->builtin_i32 };
    const fds2_dsl_native_definition definitions[] = {
        { "rl_should_close", &context->builtin_i32, none, 0, native_should_close, host },
        { "rl_key_down", &context->builtin_i32, one, 1, native_key_down, host },
        { "rl_begin", &context->builtin_i32, none, 0, native_begin, host },
        { "rl_end", &context->builtin_i32, none, 0, native_end, host },
        { "rl_clear", &context->builtin_i32, none, 0, native_clear, host },
        { "rl_draw_scene", &context->builtin_i32, two, 2, native_draw_scene, host },
    };
    return fds2_dsl_register_natives(context, definitions, sizeof(definitions) / sizeof(definitions[0]));
}

int main(void) {
    const char *source =
        "fn int game_main() {"
        "    int x = 400; int y = 300;"
        "    int z = 400; int d = 300;"
        "    while (rl_should_close() == 0) {"
        "        int left = rl_key_down(0); int right = rl_key_down(1);"
        "        int up = rl_key_down(2); int down = rl_key_down(3);"
        "        if (left) { x = x - 6; } if (right) { x = x + 6; }"
        "        if (up) { y = y - 6; } if (down) { y = y + 6; }"
        "        x = max(20, min(x, 780)); y = max(110, min(y, 580));"
        "        rl_begin(); rl_clear(); rl_draw_scene(x, y); rl_end();"
        "    } return 0;"
        "}";
    fds2_allocator *allocator = fds2_allocator_create();
    fds2_dsl_state state;
    fds2_dsl_compile_diagnostic diagnostic;
    fds2_dsl_value result;
    ray_host host;
    assert(allocator);
    assert(fds2_dsl_state_init(&state, allocator, true));
    host.context = &state.context;
    assert(register_raylib(&state.context, &host));
    assert(fds2_dsl_state_compile(&state, source, &diagnostic) == FDS2_DSL_COMPILE_OK);
    InitWindow(800, 600, "FDS2 DSL + Raylib");
    SetTargetFPS(60);
    assert(fds2_dsl_state_run(&state, "game_main", NULL, 0, &result) == FDS2_DSL_VM_OK);
    assert(result.type == &state.context.builtin_i32 && result.as.signed_integer == 0);
    CloseWindow();
    fds2_dsl_state_deinit(&state);
    assert(fds2_allocator_live_blocks_count(allocator) == 0);
    fds2_allocator_print_stats(allocator);
    fds2_allocator_destroy(allocator);
    return 0;
}
