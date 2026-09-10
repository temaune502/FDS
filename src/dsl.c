#include "fds_dsl.h"

#include <stdio.h>
#include <stdlib.h>

static bool native_c_add(struct fds_dsl_vm *vm,
                         const fds_dsl_value *args,
                         size_t argument_count,
                         fds_dsl_value *result,
                         void *user_data) {
    (void)vm;
    (void)user_data;
    if (argument_count != 2 || args[0].type != FDS_DSL_TYPE_INT || args[1].type != FDS_DSL_TYPE_INT) return false;
    *result = fds_dsl_int(args[0].as.integer + args[1].as.integer);
    return true;
}

static bool native_c_log(struct fds_dsl_vm *vm,
                         const fds_dsl_value *args,
                         size_t argument_count,
                         fds_dsl_value *result,
                         void *user_data) {
    (void)vm;
    (void)user_data;
    if (argument_count != 1 || args[0].type != FDS_DSL_TYPE_INT) return false;
    printf("C <- DSL: %lld\n", (long long)args[0].as.integer);
    *result = fds_dsl_nil();
    return true;
}

static bool native_heap_text(struct fds_dsl_vm *vm,
                             const fds_dsl_value *args,
                             size_t argument_count,
                             fds_dsl_value *result,
                             void *user_data) {
    (void)args;
    (void)user_data;
    if (argument_count != 0) return false;
    *result = fds_dsl_vm_string(vm, "owned by GC", 11);
    return result->type == FDS_DSL_TYPE_STRING;
}

static int finalized_objects;

static void finalize_test_object(fds_dsl_object *object) {
    (void)object;
    finalized_objects++;
}

static bool native_make_object(struct fds_dsl_vm *vm,
                               const fds_dsl_value *args,
                               size_t argument_count,
                               fds_dsl_value *result,
                               void *user_data) {
    fds_dsl_object *object;
    (void)args;
    (void)user_data;
    if (argument_count != 0 || !vm || !vm->heap) return false;
    object = fds_dsl_heap_alloc(vm->heap, FDS_DSL_OBJECT_USER, sizeof(int), NULL, finalize_test_object);
    if (!object) return false;
    *(int *)(object + 1) = 42;
    *result = fds_dsl_object_value(object);
    return true;
}

static void stdout_output(const char *text, size_t length, void *user_data) {
    (void)user_data;
    fwrite(text, 1, length, stdout);
    fputc('\n', stdout);
}

static const char *compile_error_name(fds_dsl_compile_error error) {
    switch (error) {
        case FDS_DSL_COMPILE_OK: return "ok";
        case FDS_DSL_COMPILE_LEXICAL: return "lexical";
        case FDS_DSL_COMPILE_SYNTAX: return "syntax";
        case FDS_DSL_COMPILE_TYPE: return "type";
        case FDS_DSL_COMPILE_NAME: return "name";
        case FDS_DSL_COMPILE_LIMIT: return "limit";
        case FDS_DSL_COMPILE_NATIVE: return "native";
    }
    return "unknown";
}

static int compile_or_report(const char *source, fds_dsl_state *state) {
    fds_dsl_compile_diagnostic diagnostic;
    fds_dsl_compile_error error = fds_dsl_state_compile(state, source, &diagnostic);
    if (error != FDS_DSL_COMPILE_OK) {
        fprintf(stderr, "DSL %s error at %zu:%zu: %s\n",
                compile_error_name(error), diagnostic.line, diagnostic.column,
                diagnostic.message ? diagnostic.message : "no diagnostic");
        return 1;
    }
    return 0;
}

int main(void) {
    const char *source =
        "import \"std\";\n"
        "import \"std\";\n"
        "int seed = 1;\n"
        "int sum_to(int limit) {\n"
        "    global int i = 0;\n"
        "    int sum = seed;\n"
        "    for (i = 0; i < limit; i = i + 1) {\n"
        "        if (i == 2) { continue; }\n"
        "        sum = c_add(sum, i);\n"
        "        if (i == 4) { break; }\n"
        "    }\n"
        "    if (sum >= 9 && !(sum != 9) || sum <= 0) {\n"
        "        c_log(sum);\n"
        "        print(\"sum reached\");\n"
        "        print(length(\"abc\"));\n"
        "        print(to_int(\"40\"));\n"
        "        print(heap_text());\n"
        "        make_object();\n"
        "        return sum;\n"
        "    } else {\n"
        "        return 0;\n"
        "    }\n"
        "}\n"
        "return sum_to(5);\n";
    fds_dsl_state *state;
    fds_dsl_value result;
    uint16_t add_index;
    uint16_t log_index;
    uint16_t heap_text_index;
    uint16_t object_index;
    state = fds_dsl_state_new(stdout_output, NULL);
    if (!state) return 1;
    add_index = fds_dsl_state_add_native(state, "c_add", FDS_DSL_TYPE_INT, native_c_add, NULL);
    log_index = fds_dsl_state_add_native(state, "c_log", FDS_DSL_TYPE_NIL, native_c_log, NULL);
    heap_text_index = fds_dsl_state_add_native(state, "heap_text", FDS_DSL_TYPE_STRING, native_heap_text, NULL);
    object_index = fds_dsl_state_add_native(state, "make_object", FDS_DSL_TYPE_OBJECT, native_make_object, NULL);
    if (add_index == FDS_DSL_INVALID_INDEX || log_index == FDS_DSL_INVALID_INDEX || heap_text_index == FDS_DSL_INVALID_INDEX || object_index == FDS_DSL_INVALID_INDEX) { fds_dsl_state_delete(state); return 1; }
    if (compile_or_report(source, state)) { fds_dsl_state_delete(state); return 1; }

    if (fds_dsl_state_run(state, &result) != FDS_DSL_OK) {
        fprintf(stderr, "VM error %d at instruction %zu\n", state->vm.error, state->vm.error_instruction);
        fds_dsl_state_delete(state);
        return 1;
    }
    if (result.type != FDS_DSL_TYPE_INT || result.as.integer != 9) {
        fprintf(stderr, "unexpected DSL result: type=%d value=%lld\n",
                result.type, (long long)(result.type == FDS_DSL_TYPE_INT ? result.as.integer : 0));
        fds_dsl_state_delete(state);
        return 1;
    }
    if (state->heap.object_count == 0) {
        fprintf(stderr, "GC test did not allocate an owned string\n");
        fds_dsl_state_delete(state);
        return 1;
    }
    fds_dsl_heap_collect(&state->heap, &state->vm);
    if (state->heap.object_count != 0) {
        fprintf(stderr, "GC test retained an unreachable string\n");
        fds_dsl_state_delete(state);
        return 1;
    }
    if (finalized_objects != 1) {
        fprintf(stderr, "GC test finalized %d objects instead of 1\n", finalized_objects);
        fds_dsl_state_delete(state);
        return 1;
    }
    fds_dsl_value child = fds_dsl_vm_string(&state->vm, "array child", 11);
    uint16_t child_handle = fds_dsl_heap_push_handle(&state->heap, child);
    fds_dsl_array *array = fds_dsl_array_new(&state->heap, 1);
    if (!array || child.type != FDS_DSL_TYPE_STRING || !fds_dsl_array_set(array, 0, child) ||
        child_handle == FDS_DSL_INVALID_INDEX || !fds_dsl_push(&state->vm, fds_dsl_array_value(array))) {
        fprintf(stderr, "array GC setup failed\n");
        fds_dsl_state_delete(state);
        return 1;
    }
	if (!fds_dsl_heap_release_handle(&state->heap, child_handle)) {
		fprintf(stderr, "GC handle release failed\n");
		fds_dsl_state_delete(state);
		return 1;
	}
    fds_dsl_heap_collect(&state->heap, &state->vm);
    if (state->heap.object_count != 2) {
        fprintf(stderr, "GC lost a live array graph: %zu objects remain\n", state->heap.object_count);
        fds_dsl_state_delete(state);
        return 1;
    }
    fds_dsl_value discarded;
    fds_dsl_pop(&state->vm, &discarded);
    fds_dsl_heap_collect(&state->heap, &state->vm);
    if (state->heap.object_count != 0) {
        fprintf(stderr, "GC retained an unreachable array graph\n");
        fds_dsl_state_delete(state);
        return 1;
    }
    printf("DSL -> VM -> C test passed: result=%lld\n", (long long)result.as.integer);
    fds_dsl_state_delete(state);
    return 0;
}
