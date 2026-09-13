#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#define FDS2_ALLOCATOR_IMPLEMENTATION
#include "fds2_dsl.h"

typedef struct test_host {
    int calls;
    fds2_dsl_context *context;
} test_host;

static int finalized_objects;

static void finalize_test_object(fds2_dsl_object *object, void *user_data) {
    (void)object;
    (*(int *)user_data)++;
}

static bool host_scale(fds2_dsl_vm *vm,
                       const fds2_dsl_value *arguments,
                       size_t argument_count,
                       fds2_dsl_value *result,
                       void *user_data) {
    test_host *host = (test_host *)user_data;
    int32_t value;
    if (vm) {
        if (!fds2_dsl_arg_i32(vm, arguments, argument_count, 0, &value)) return false;
    } else {
        fds2_dsl_value converted;
        if (!fds2_dsl_native_argument(arguments, argument_count, 0, &host->context->builtin_i32, &converted)) return false;
        value = (int32_t)converted.as.signed_integer;
    }
    host->calls++;
    if (vm) return fds2_dsl_return_i32(vm, result, value * 3);
    *result = fds2_dsl_i32_value(host->context, value * 3);
    return true;
}

static fds2_dsl_node *parse_source(fds2_allocator *allocator,
                                   const char *source,
                                   fds2_dsl_parser *parser) {
    fds2_dsl_parser_init(parser, allocator, source);
    return fds2_dsl_parse(parser);
}

static void run_function(fds2_allocator *allocator,
                         fds2_dsl_context *context,
                         fds2_dsl_node *function,
                         const int64_t *input,
                         size_t input_count,
                         int64_t expected) {
    fds2_dsl_bytecode bytecode;
    fds2_dsl_compile_diagnostic diagnostic;
    fds2_dsl_vm vm;
    fds2_dsl_value arguments[4];
    fds2_dsl_value result;

    assert(input_count <= 4);
    fds2_dsl_bytecode_init(&bytecode, allocator, context);
    assert(fds2_dsl_compile_function(context, function, &bytecode, &diagnostic) == FDS2_DSL_COMPILE_OK);
    for (size_t index = 0; index < input_count; index++) {
        arguments[index] = fds2_dsl_value_of(&context->builtin_i32);
        arguments[index].as.signed_integer = input[index];
    }
    fds2_dsl_vm_init(&vm, allocator, &bytecode);
    assert(fds2_dsl_vm_run(&vm, arguments, input_count, &result) == FDS2_DSL_VM_OK);
    assert(result.type == &context->builtin_i32);
    assert(result.as.signed_integer == expected);
    fds2_dsl_vm_deinit(&vm);
    fds2_dsl_bytecode_deinit(&bytecode);
}

int main(void) {
    const char *source =
        "fn int add(int left, int right) { return left + right; }"
        "fn int choose(int value) { if (value) { return value; } else { return 0; } }"
        "fn int loop(int value) { int total = 0; while (value > 0) { total = total + value; value = value - 1; } return total; }";
    fds2_allocator *allocator = fds2_allocator_create();
    fds2_dsl_context context;
    fds2_dsl_parser parser;
    fds2_dsl_node *program;
    test_host host = { 0, &context };
    const fds2_dsl_type *native_arguments[1];
    const fds2_dsl_native *native;
    fds2_dsl_value native_input;
    fds2_dsl_value native_result;

    assert(allocator);
    fds2_dsl_context_init(&context, allocator);
    program = parse_source(allocator, source, &parser);
    assert(program && parser.error == FDS2_DSL_PARSE_OK);
    assert(program->as.list.count == 3);

    {
        const int64_t arguments[] = { 19, 23 };
        run_function(allocator, &context, program->as.list.items[0], arguments, 2, 42);
    }
    {
        const int64_t arguments[] = { 7 };
        run_function(allocator, &context, program->as.list.items[1], arguments, 1, 7);
        {
            const int64_t zero[] = { 0 };
            run_function(allocator, &context, program->as.list.items[1], zero, 1, 0);
        }
    }
    {
        const int64_t arguments[] = { 5 };
        run_function(allocator, &context, program->as.list.items[2], arguments, 1, 15);
        {
            const int64_t zero[] = { 0 };
            run_function(allocator, &context, program->as.list.items[2], zero, 1, 0);
        }
    }

    native_arguments[0] = &context.builtin_i32;
    native = fds2_dsl_register_native(&context, "host_scale", &context.builtin_i32,
                                      native_arguments, 1, host_scale, &host);
    assert(native);
    native_input = fds2_dsl_value_of(&context.builtin_i16);
    native_input.as.signed_integer = 14;
    assert(fds2_dsl_native_call(NULL, native, &native_input, 1, &native_result) == FDS2_DSL_OK);
    assert(native_result.as.signed_integer == 42);
    assert(host.calls == 1);

    {
        const char *call_source =
            "fn int calc(int value) { return host_scale(value) + helper(value); }"
            "fn int helper(int value) { return value + 1; }";
        fds2_dsl_bytecode bytecode;
        fds2_dsl_compile_diagnostic diagnostic;
        fds2_dsl_vm vm;
        fds2_dsl_value argument = fds2_dsl_value_of(&context.builtin_i32);
        fds2_dsl_value result;
        fds2_dsl_node *call_program = parse_source(allocator, call_source, &parser);
        assert(call_program && parser.error == FDS2_DSL_PARSE_OK);
        fds2_dsl_bytecode_init(&bytecode, allocator, &context);
        assert(fds2_dsl_compile_program(&context, call_program, &bytecode, &diagnostic) == FDS2_DSL_COMPILE_OK);
        fds2_dsl_vm_init(&vm, allocator, &bytecode);
        assert(fds2_dsl_vm_set_entry(&vm, (size_t)fds2_dsl_bytecode_find_function_name(&bytecode, "calc")));
        argument.as.signed_integer = 10;
        assert(fds2_dsl_vm_run(&vm, &argument, 1, &result) == FDS2_DSL_VM_OK);
        assert(result.as.signed_integer == 41);
        assert(host.calls == 2);
        fds2_dsl_vm_deinit(&vm);
        fds2_dsl_bytecode_deinit(&bytecode);
        fds2_dsl_node_delete(allocator, call_program);
    }

    {
        fds2_dsl_type *vec2 = (fds2_dsl_type *)fds2_dsl_register_type(&context, "Vec2", sizeof(int32_t) * 2, _Alignof(int32_t));
        const fds2_dsl_type *reference;
        fds2_dsl_array *array;
        fds2_dsl_map *map;
        fds2_dsl_tuple *tuple;
        fds2_dsl_object *user_object;
        fds2_dsl_value roots[4];
        fds2_dsl_value value;
        fds2_dsl_map_entry entry;
        int32_t coordinate = 27;
        int32_t read_coordinate = 0;

        assert(vec2);
        assert(fds2_dsl_register_field(&context, vec2, "x", &context.builtin_i32, 0));
        assert(fds2_dsl_register_field(&context, vec2, "y", &context.builtin_i32, sizeof(int32_t)));
        assert(fds2_dsl_find_field(vec2, "y", 1)->offset == sizeof(int32_t));
        reference = fds2_dsl_register_reference(&context, "Vec2Ref", vec2);
        assert(reference && reference->kind == FDS2_DSL_TYPE_REFERENCE && reference->referenced_type == vec2);
        user_object = fds2_dsl_user_object_new(&context.heap, vec2, finalize_test_object, &finalized_objects);
        array = fds2_dsl_array_new(&context.heap, 1);
        map = fds2_dsl_map_new(&context.heap, 1);
        tuple = fds2_dsl_tuple_new(&context.heap, 1);
        assert(user_object && array && map && tuple);
        assert(fds2_dsl_user_field_set(user_object, vec2, fds2_dsl_find_field(vec2, "x", 1), &coordinate, sizeof(coordinate)));
        assert(fds2_dsl_user_field_get(user_object, vec2, fds2_dsl_find_field(vec2, "x", 1), &read_coordinate, sizeof(read_coordinate)));
        assert(read_coordinate == 27);
        assert(fds2_dsl_array_set(array, 0, fds2_dsl_object_value(reference, user_object)));
        assert(fds2_dsl_map_set(map, 0, fds2_dsl_i32_value(&context, 1), fds2_dsl_object_value(reference, user_object)));
        assert(fds2_dsl_tuple_set(tuple, 0, fds2_dsl_object_value(reference, user_object)));
        roots[0] = fds2_dsl_object_value(reference, &array->object);
        roots[1] = fds2_dsl_object_value(reference, &map->object);
        roots[2] = fds2_dsl_object_value(reference, &tuple->object);
        roots[3] = fds2_dsl_bool_value(&context, true);
        fds2_dsl_heap_collect(&context.heap, roots, 4);
        assert(context.heap.object_count == 4);
        assert(fds2_dsl_map_get(map, 0, &entry) && entry.key.as.signed_integer == 1);
        assert(fds2_dsl_tuple_get(tuple, 0, &value) && value.as.object == user_object);
        fds2_dsl_heap_collect(&context.heap, NULL, 0);
        assert(context.heap.object_count == 0);
        assert(finalized_objects == 1);
    }

    assert(fds2_dsl_register_core(&context));
    {
        const char *core_source = "fn int main(int value) { return max(abs(value), min(value, 10)); }";
        fds2_dsl_value argument = fds2_dsl_value_of(&context.builtin_i32);
        fds2_dsl_value result;
        fds2_dsl_compile_diagnostic diagnostic;
        argument.as.signed_integer = -14;
        assert(fds2_dsl_run_source(&context, core_source, &argument, 1, &result, &diagnostic) == FDS2_DSL_VM_OK);
        assert(result.as.signed_integer == 14);
    }

    {
        fds2_dsl_state state;
        fds2_dsl_config config = fds2_dsl_config_default();
        fds2_dsl_value argument;
        fds2_dsl_value result;
        fds2_dsl_compile_diagnostic diagnostic;
        config.allocator = allocator;
        config.gc_threshold = 128;
        assert(fds2_dsl_state_init_config(&state, &config));
        argument = fds2_dsl_state_i32(&state, 21);
        assert(fds2_dsl_state_compile(&state, "fn int main(int value) { return value * 2; }", &diagnostic) == FDS2_DSL_COMPILE_OK);
        assert(fds2_dsl_state_run(&state, "main", &argument, 1, &result) == FDS2_DSL_VM_OK);
        assert(result.as.signed_integer == 42);
        fds2_dsl_state_deinit(&state);
    }

    {
        const char *path = "build\\fds2_dsl_state_source.tmp";
        FILE *file = fopen(path, "wb");
        fds2_dsl_state state;
        fds2_dsl_value result;
        fds2_dsl_compile_diagnostic diagnostic;
        assert(file);
        assert(fwrite("fn int main() { return 42; }", 1, sizeof("fn int main() { return 42; }") - 1, file) == sizeof("fn int main() { return 42; }") - 1);
        assert(fclose(file) == 0);
        assert(fds2_dsl_state_init(&state, allocator, true));
        assert(fds2_dsl_state_compile_file(&state, path, &diagnostic) == FDS2_DSL_COMPILE_OK);
        assert(fds2_dsl_state_run(&state, "main", NULL, 0, &result) == FDS2_DSL_VM_OK);
        assert(result.as.signed_integer == 42);
        fds2_dsl_state_deinit(&state);
        assert(remove(path) == 0);
    }

    {
        const char *tuple_source = "fn tuple pair(int left, int right) { return left, right; }";
        fds2_dsl_value arguments[2];
        fds2_dsl_value result;
        fds2_dsl_value first;
        fds2_dsl_value second;
        fds2_dsl_compile_diagnostic diagnostic;
        fds2_dsl_bytecode bytecode;
        fds2_dsl_vm vm;
        fds2_dsl_node *tuple_program = parse_source(allocator, tuple_source, &parser);
        assert(tuple_program && parser.error == FDS2_DSL_PARSE_OK);
        fds2_dsl_bytecode_init(&bytecode, allocator, &context);
        assert(fds2_dsl_compile_program(&context, tuple_program, &bytecode, &diagnostic) == FDS2_DSL_COMPILE_OK);
        arguments[0] = fds2_dsl_i32_value(&context, 12);
        arguments[1] = fds2_dsl_i32_value(&context, 30);
        fds2_dsl_vm_init(&vm, allocator, &bytecode);
        assert(fds2_dsl_vm_run(&vm, arguments, 2, &result) == FDS2_DSL_VM_OK);
        assert(result.type == &context.builtin_tuple && result.as.object);
        assert(fds2_dsl_tuple_get((fds2_dsl_tuple *)result.as.object, 0, &first));
        assert(fds2_dsl_tuple_get((fds2_dsl_tuple *)result.as.object, 1, &second));
        assert(first.as.signed_integer == 12 && second.as.signed_integer == 30);
        fds2_dsl_vm_deinit(&vm);
        fds2_dsl_bytecode_deinit(&bytecode);
        fds2_dsl_node_delete(allocator, tuple_program);
    }

    {
        fds2_dsl_parser string_parser;
        fds2_dsl_node *string_program = parse_source(allocator, "fn int main() { return 1; }", &string_parser);
        fds2_dsl_bytecode string_bytecode;
        fds2_dsl_compile_diagnostic string_diagnostic;
        fds2_dsl_vm string_vm;
        fds2_dsl_value string_result;
        assert(string_program && string_parser.error == FDS2_DSL_PARSE_OK);
        fds2_dsl_bytecode_init(&string_bytecode, allocator, &context);
        assert(fds2_dsl_compile_program(&context, string_program, &string_bytecode, &string_diagnostic) == FDS2_DSL_COMPILE_OK);
        fds2_dsl_vm_init(&string_vm, allocator, &string_bytecode);
        assert(fds2_dsl_return_string(&string_vm, &string_result, "hello", 5));
        assert(string_result.type == &context.builtin_string);
        assert(string_result.as.string.length == 5 && memcmp(string_result.as.string.data, "hello", 5) == 0);
        assert(strcmp(fds2_dsl_vm_error_string(FDS2_DSL_VM_TYPE_ERROR), "type error") == 0);
        fds2_dsl_vm_deinit(&string_vm);
        fds2_dsl_bytecode_deinit(&string_bytecode);
        fds2_dsl_node_delete(allocator, string_program);
    }

    fds2_dsl_node_delete(allocator, program);
    fds2_dsl_context_deinit(&context);
    assert(fds2_allocator_live_blocks_count(allocator) == 0);
    fds2_allocator_destroy(allocator);
    return 0;
}
