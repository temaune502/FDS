#include <stdio.h>
#define DEBUG_MEM
#define FDS2_ALLOCATOR_IMPLEMENTATION
#include "fds2_dsl.h"

int main(void) {
    const char *source = "fn int main(int value) { return max(value, 42); }";
    fds2_allocator *allocator = fds2_allocator_create();
    fds2_dsl_state *state;
    fds2_dsl_config config;
    fds2_dsl_value argument;
    fds2_dsl_value result;
    fds2_dsl_compile_diagnostic diagnostic;

    if (!allocator) return 1;
    config = fds2_dsl_config_default();
    config.allocator = allocator;
    state = fds2_dsl_state_new_config(&config);
    if (!state) return 1;
    argument = fds2_dsl_state_i32(state, 27);
    if (fds2_dsl_state_compile(state, source, &diagnostic) != FDS2_DSL_COMPILE_OK) {
        fprintf(stderr, "compile failed at %zu:%zu: %s\n", diagnostic.line, diagnostic.column, diagnostic.message);
        fds2_dsl_state_delete(state);
        fds2_allocator_destroy(allocator);
        return 1;
    }
    if (fds2_dsl_state_run(state, "main", &argument, 1, &result) != FDS2_DSL_VM_OK) {
        fprintf(stderr, "runtime failed\n");
        fds2_dsl_state_delete(state);
        fds2_allocator_destroy(allocator);
        return 1;
    }
    printf("result=%lld\n", (long long)result.as.signed_integer);
    fds2_dsl_state_delete(state);
    fds2_allocator_print_stats(allocator);
    fds2_allocator_destroy(allocator);
    return result.as.signed_integer == 42 ? 0 : 1;
}
