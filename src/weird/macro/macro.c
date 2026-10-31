#define FDS_MACRO_IMPL
#include "fds_macro.h"

#include <stdio.h>

typedef struct {
    FdsMacroContext *ctx;
    bool holding_left;
} MacroState;

static void repeat_macro(void *user_data) {
    MacroState *state = (MacroState *)user_data;

    for (;;) {
        fds_macro_sleep(2000);
        fds_macro_key_press('1');
        fds_macro_mouse_click(FDS_MOUSE_RIGHT);
        fds_macro_key_press('2');

        state->holding_left = true;
        fds_macro_mouse_button(FDS_MOUSE_LEFT, true);
        fds_macro_sleep(2000);
        fds_macro_mouse_button(FDS_MOUSE_LEFT, false);
        state->holding_left = false;
    }
}

static void on_pause(void *user_data, bool paused) {
    MacroState *state = (MacroState *)user_data;

    if (paused) {
        /* The input functions themselves retain their original behavior. */
        fds_macro_key_state('1', false);
        fds_macro_mouse_button(FDS_MOUSE_RIGHT, false);
        fds_macro_key_state('2', false);
        fds_macro_mouse_button(FDS_MOUSE_LEFT, false);
    } else if (state->holding_left) {
        /* Resume a left-button hold interrupted by the pause. */
        fds_macro_mouse_button(FDS_MOUSE_LEFT, true);
    }
}

static void toggle_pause(void *user_data) {
    MacroState *state = (MacroState *)user_data;
    fds_macro_toggle_pause(state->ctx);
    puts(state->ctx->paused ? "Macro paused" : "Macro resumed");
}

static void stop_macro(void *user_data) {
    MacroState *state = (MacroState *)user_data;

    /* Pause callback releases any input that may be held. */
    fds_macro_set_paused(state->ctx, true);
    fds_macro_stop(state->ctx);
}

int main(void) {
    FdsMacroContext ctx;
    MacroState state = { .ctx = &ctx, .holding_left = false };
    FdsMacroHotkey hotkeys[] = {
        { .vk_code = 0x78, .is_pressed = false, .callback = toggle_pause }, /* F9 */
        { .vk_code = 0x79, .is_pressed = false, .callback = stop_macro }   /* F10 */
    };

    fds_macro_init(&ctx, 60, NULL, hotkeys,
                   sizeof(hotkeys) / sizeof(hotkeys[0]), &state);
    fds_macro_set_pause_callback(&ctx, on_pause);

    if (!fds_macro_coroutine_create(&ctx, repeat_macro, &state, 64 * 1024)) {
        fputs("Failed to create macro coroutine\n", stderr);
        return 1;
    }

    puts("F9: pause/resume, F10: exit");
    fds_macro_run(&ctx);
    fds_macro_destroy(&ctx);
    return 0;
}
