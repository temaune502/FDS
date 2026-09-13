#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#define FDS2_ALLOCATOR_IMPLEMENTATION
#include "fds2_dsl.h"

static bool native_add_i32(fds2_dsl_vm *vm,
                           const fds2_dsl_value *arguments,
                           size_t argument_count,
                           fds2_dsl_value *result,
                           void *user_data) {
    fds2_dsl_context *context = (fds2_dsl_context *)user_data;
    fds2_dsl_value left;
    fds2_dsl_value right;
    (void)vm;
    if (!fds2_dsl_native_argument(arguments, argument_count, 0, &context->builtin_i32, &left)) return false;
    if (!fds2_dsl_native_argument(arguments, argument_count, 1, &context->builtin_i32, &right)) return false;
    *result = fds2_dsl_value_of(&context->builtin_i32);
    result->as.signed_integer = left.as.signed_integer + right.as.signed_integer;
    return true;
}

int main(void) {
    fds2_dsl_lexer lexer;
    fds2_dsl_token token;
    fds2_dsl_parser parser;
    fds2_dsl_node *program;
    fds2_dsl_bytecode bytecode;
    fds2_dsl_compile_diagnostic diagnostic;
    fds2_dsl_vm vm;
    fds2_dsl_value vm_arguments[2];
    fds2_allocator *allocator = fds2_allocator_create();
    fds2_dsl_context context;
    const fds2_dsl_type *argument_types[2];
    const fds2_dsl_native *native;
    fds2_dsl_value arguments[2];
    fds2_dsl_value result;
    const fds2_dsl_type *vec2_type;
    const char *source_path = "build\\fds2_dsl_source.tmp";
    const char *file_source = "fn int file_add(int a, int b) { return a + b; }\n";
    char *loaded_source;
    size_t loaded_length;
    FILE *source_file;

    assert(allocator);
    fds2_dsl_context_init(&context, allocator);

    fds2_dsl_lexer_init(&lexer, "fn int add(int a, int b) { return a + b; }");
    token = fds2_dsl_lexer_next(&lexer);
    assert(token.kind == FDS2_DSL_TOKEN_FN);
    token = fds2_dsl_lexer_next(&lexer);
    assert(token.kind == FDS2_DSL_TOKEN_IDENTIFIER && token.length == 3);
    token = fds2_dsl_lexer_next(&lexer);
    assert(token.kind == FDS2_DSL_TOKEN_IDENTIFIER && token.length == 3);
    token = fds2_dsl_lexer_next(&lexer);
    assert(token.kind == FDS2_DSL_TOKEN_LEFT_PAREN);

    fds2_dsl_parser_init(&parser, allocator,
        "fn int add(int a, int b) { int sum = a + b; if (sum) { return sum; } else { return 0; } }");
    program = fds2_dsl_parse(&parser);
    assert(program && parser.error == FDS2_DSL_PARSE_OK);
    assert(program->kind == FDS2_DSL_NODE_PROGRAM && program->as.list.count == 1);
    assert(program->as.list.items[0]->kind == FDS2_DSL_NODE_FUNCTION);
    assert(program->as.list.items[0]->as.function.parameter_count == 2);
    assert(program->as.list.items[0]->as.function.body->as.list.count == 2);
    fds2_dsl_bytecode_init(&bytecode, allocator, &context);
    assert(fds2_dsl_compile_function(&context, program->as.list.items[0], &bytecode, &diagnostic) == FDS2_DSL_COMPILE_OK);
    vm_arguments[0] = fds2_dsl_value_of(&context.builtin_i32);
    vm_arguments[0].as.signed_integer = 20;
    vm_arguments[1] = fds2_dsl_value_of(&context.builtin_i32);
    vm_arguments[1].as.signed_integer = 22;
    fds2_dsl_vm_init(&vm, allocator, &bytecode);
    assert(fds2_dsl_vm_run(&vm, vm_arguments, 2, &result) == FDS2_DSL_VM_OK);
    assert(result.type == &context.builtin_i32 && result.as.signed_integer == 42);
    vm_arguments[0].as.signed_integer = -22;
    assert(fds2_dsl_vm_run(&vm, vm_arguments, 2, &result) == FDS2_DSL_VM_OK);
    assert(result.type == &context.builtin_i32 && result.as.signed_integer == 0);
    fds2_dsl_vm_deinit(&vm);
    fds2_dsl_bytecode_deinit(&bytecode);
    fds2_dsl_node_delete(allocator, program);

    source_file = fopen(source_path, "wb");
    assert(source_file);
    assert(fwrite(file_source, 1, strlen(file_source), source_file) == strlen(file_source));
    assert(fclose(source_file) == 0);
    assert(fds2_dsl_read_file(allocator, source_path, &loaded_source, &loaded_length));
    assert(loaded_length == strlen(file_source));
    fds2_dsl_parser_init(&parser, allocator, loaded_source);
    program = fds2_dsl_parse(&parser);
    assert(program && parser.error == FDS2_DSL_PARSE_OK);
    fds2_free_a(allocator, loaded_source);
    fds2_dsl_bytecode_init(&bytecode, allocator, &context);
    assert(fds2_dsl_compile_function(&context, program->as.list.items[0], &bytecode, &diagnostic) == FDS2_DSL_COMPILE_OK);
    fds2_dsl_vm_init(&vm, allocator, &bytecode);
    vm_arguments[0].as.signed_integer = 30;
    vm_arguments[1].as.signed_integer = 12;
    assert(fds2_dsl_vm_run(&vm, vm_arguments, 2, &result) == FDS2_DSL_VM_OK);
    assert(result.as.signed_integer == 42);
    fds2_dsl_vm_deinit(&vm);
    fds2_dsl_bytecode_deinit(&bytecode);
    fds2_dsl_node_delete(allocator, program);
    assert(remove(source_path) == 0);

    fds2_dsl_parser_init(&parser, allocator, "fn int loop(int a) { while (a) { return a; } return 0; }");
    program = fds2_dsl_parse(&parser);
    assert(program && parser.error == FDS2_DSL_PARSE_OK);
    fds2_dsl_bytecode_init(&bytecode, allocator, &context);
    assert(fds2_dsl_compile_function(&context, program->as.list.items[0], &bytecode, &diagnostic) == FDS2_DSL_COMPILE_OK);
    vm_arguments[0].as.signed_integer = 7;
    fds2_dsl_vm_init(&vm, allocator, &bytecode);
    assert(fds2_dsl_vm_run(&vm, vm_arguments, 1, &result) == FDS2_DSL_VM_OK);
    assert(result.as.signed_integer == 7);
    vm_arguments[0].as.signed_integer = 0;
    assert(fds2_dsl_vm_run(&vm, vm_arguments, 1, &result) == FDS2_DSL_VM_OK);
    assert(result.as.signed_integer == 0);
    fds2_dsl_vm_deinit(&vm);
    fds2_dsl_bytecode_deinit(&bytecode);
    fds2_dsl_node_delete(allocator, program);

    assert(fds2_dsl_find_type(&context, "int16", 5) == &context.builtin_i16);
    assert(fds2_dsl_find_type(&context, "uint32", 6) == &context.builtin_u32);
    vec2_type = fds2_dsl_register_type(&context, "Vec2", sizeof(float) * 2, _Alignof(float));
    assert(vec2_type && vec2_type->kind == FDS2_DSL_TYPE_USER);
    assert(fds2_dsl_find_type(&context, "Vec2", 4) == vec2_type);

    arguments[0] = fds2_dsl_value_of(&context.builtin_i16);
    arguments[0].as.signed_integer = 20;
    arguments[1] = fds2_dsl_value_of(&context.builtin_i32);
    arguments[1].as.signed_integer = 22;
    assert(fds2_dsl_cast_value(&arguments[0], &context.builtin_i32, &result));
    assert(result.as.signed_integer == 20);

    argument_types[0] = &context.builtin_i32;
    argument_types[1] = &context.builtin_i32;
    native = fds2_dsl_register_native(&context, "add", &context.builtin_i32,
                                      argument_types, 2, native_add_i32, &context);
    assert(native);
    assert(fds2_dsl_native_call(NULL, native, arguments, 2, &result) == FDS2_DSL_OK);
    assert(result.type == &context.builtin_i32);
    assert(result.as.signed_integer == 42);
    assert(fds2_dsl_native_call(NULL, native, arguments, 1, &result) == FDS2_DSL_ERROR_BAD_ARITY);

    fds2_dsl_context_deinit(&context);
    assert(fds2_allocator_live_blocks_count(allocator) == 0);
    fds2_allocator_destroy(allocator);
    return 0;
}
