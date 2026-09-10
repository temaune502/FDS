#ifndef FDS_DSL_H
#define FDS_DSL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef FDS_DSL_STACK_CAPACITY
#define FDS_DSL_STACK_CAPACITY 4096
#endif
#ifndef FDS_DSL_CODE_CAPACITY
#define FDS_DSL_CODE_CAPACITY 16384
#endif
#ifndef FDS_DSL_CONSTANT_CAPACITY
#define FDS_DSL_CONSTANT_CAPACITY 4096
#endif
#ifndef FDS_DSL_LOCAL_CAPACITY
#define FDS_DSL_LOCAL_CAPACITY 256
#endif
#ifndef FDS_DSL_NATIVE_CAPACITY
#define FDS_DSL_NATIVE_CAPACITY 256
#endif
#ifndef FDS_DSL_FUNCTION_CAPACITY
#define FDS_DSL_FUNCTION_CAPACITY 256
#endif
#ifndef FDS_DSL_RETURN_CAPACITY
#define FDS_DSL_RETURN_CAPACITY 32
#endif
#ifndef FDS_DSL_IMPORT_CAPACITY
#define FDS_DSL_IMPORT_CAPACITY 128
#endif
#ifndef FDS_DSL_HEAP_INITIAL_LIMIT
#define FDS_DSL_HEAP_INITIAL_LIMIT 4096
#endif
#ifndef FDS_DSL_HANDLE_CAPACITY
#define FDS_DSL_HANDLE_CAPACITY 256
#endif

#define FDS_DSL_INVALID_INDEX UINT16_MAX

typedef enum fds_dsl_type {
	FDS_DSL_TYPE_NIL = 0,
	FDS_DSL_TYPE_BOOL,
	FDS_DSL_TYPE_INT,
	FDS_DSL_TYPE_FLOAT,
	FDS_DSL_TYPE_STRING,
	FDS_DSL_TYPE_OBJECT,
} fds_dsl_type;

typedef enum fds_dsl_object_type {
	FDS_DSL_OBJECT_STRING = 1,
	FDS_DSL_OBJECT_USER,
	FDS_DSL_OBJECT_ARRAY,
} fds_dsl_object_type;

typedef struct fds_dsl_object fds_dsl_object;
typedef struct fds_dsl_heap fds_dsl_heap;
typedef void (*fds_dsl_object_trace_fn)(fds_dsl_heap *heap, fds_dsl_object *object);
typedef void (*fds_dsl_object_finalize_fn)(fds_dsl_object *object);

struct fds_dsl_object {
	fds_dsl_object_type type;
	bool marked;
	size_t bytes;
	fds_dsl_object *next;
	fds_dsl_object_trace_fn trace;
	fds_dsl_object_finalize_fn finalize;
};

typedef struct fds_dsl_string_view {
	const char *data;
	size_t length;
	fds_dsl_object *object;
} fds_dsl_string_view;

typedef struct fds_dsl_value {
	fds_dsl_type type;
	union {
		bool boolean;
		int64_t integer;
		double floating;
		fds_dsl_string_view string;
		fds_dsl_object *object;
	} as;
} fds_dsl_value;

typedef struct fds_dsl_array {
	fds_dsl_object object;
	size_t count;
	fds_dsl_value values[];
} fds_dsl_array;

struct fds_dsl_vm;
typedef struct fds_dsl_program fds_dsl_program;
typedef bool (*fds_dsl_native_fn)(struct fds_dsl_vm *vm,
								  const fds_dsl_value *args,
								  size_t argument_count,
								  fds_dsl_value *result,
								  void *user_data);

typedef bool (*fds_dsl_import_fn)(struct fds_dsl_program *program,
								  const char *module_name,
								  size_t module_length,
								  void *user_data);

typedef void (*fds_dsl_output_fn)(const char *text, size_t length, void *user_data);

typedef enum fds_dsl_opcode {
	FDS_DSL_OP_CONST = 0,
	FDS_DSL_OP_NIL,
	FDS_DSL_OP_TRUE,
	FDS_DSL_OP_FALSE,
	FDS_DSL_OP_LOAD_LOCAL,
	FDS_DSL_OP_STORE_LOCAL,
	FDS_DSL_OP_LOAD_GLOBAL,
	FDS_DSL_OP_STORE_GLOBAL,
	FDS_DSL_OP_POP,
	FDS_DSL_OP_DUP,
	FDS_DSL_OP_ADD,
	FDS_DSL_OP_SUB,
	FDS_DSL_OP_MUL,
	FDS_DSL_OP_DIV,
	FDS_DSL_OP_NEGATE,
	FDS_DSL_OP_EQUAL,
	FDS_DSL_OP_LESS,
	FDS_DSL_OP_GREATER,
	FDS_DSL_OP_NOT,
	FDS_DSL_OP_AND,
	FDS_DSL_OP_OR,
	FDS_DSL_OP_CALL,
	FDS_DSL_OP_CALL_FUNCTION,
	FDS_DSL_OP_JUMP,
	FDS_DSL_OP_JUMP_IF_FALSE,
	FDS_DSL_OP_RETURN,
	FDS_DSL_OP_RETURN_VALUES,
	FDS_DSL_OP_HALT,
} fds_dsl_opcode;

typedef struct fds_dsl_instruction {
	uint8_t opcode;
	uint16_t operand;
} fds_dsl_instruction;

typedef struct fds_dsl_native {
	const char *name;
	fds_dsl_native_fn function;
	void *user_data;
	fds_dsl_type result_type;
} fds_dsl_native;

typedef struct fds_dsl_function {
	const char *name;
	uint16_t name_length;
	size_t code_start;
	size_t code_end;
	fds_dsl_type parameter_types[FDS_DSL_LOCAL_CAPACITY];
	fds_dsl_type return_types[FDS_DSL_RETURN_CAPACITY];
	uint16_t parameter_count;
	uint16_t return_count;
	uint16_t local_count;
} fds_dsl_function;

typedef struct fds_dsl_global {
	const char *name;
	uint16_t name_length;
	fds_dsl_type type;
	uint16_t index;
} fds_dsl_global;

typedef struct fds_dsl_import {
	const char *name;
	uint16_t name_length;
} fds_dsl_import;

typedef enum fds_dsl_error {
	FDS_DSL_OK = 0,
	FDS_DSL_ERROR_OVERFLOW,
	FDS_DSL_ERROR_BAD_ARGUMENT,
	FDS_DSL_ERROR_BAD_BYTECODE,
	FDS_DSL_ERROR_STACK_UNDERFLOW,
	FDS_DSL_ERROR_TYPE,
	FDS_DSL_ERROR_DIVISION_BY_ZERO,
	FDS_DSL_ERROR_UNKNOWN_NATIVE,
	FDS_DSL_ERROR_NATIVE,
} fds_dsl_error;

struct fds_dsl_program {
	fds_dsl_instruction code[FDS_DSL_CODE_CAPACITY];
	size_t code_count;
	fds_dsl_value constants[FDS_DSL_CONSTANT_CAPACITY];
	size_t constant_count;
	fds_dsl_native natives[FDS_DSL_NATIVE_CAPACITY];
	size_t native_count;
	fds_dsl_function functions[FDS_DSL_FUNCTION_CAPACITY];
	size_t function_count;
	fds_dsl_global globals[FDS_DSL_LOCAL_CAPACITY];
	size_t global_count;
	fds_dsl_import imports[FDS_DSL_IMPORT_CAPACITY];
	size_t import_count;
	size_t local_count;
};

typedef struct fds_dsl_frame {
	fds_dsl_value locals[FDS_DSL_LOCAL_CAPACITY];
	size_t instruction_pointer;
	size_t stack_base;
	uint16_t function_index;
} fds_dsl_frame;

typedef struct fds_dsl_vm {
	const fds_dsl_program *program;
	fds_dsl_heap *heap;
	fds_dsl_value stack[FDS_DSL_STACK_CAPACITY];
	size_t stack_count;
	fds_dsl_value locals[FDS_DSL_LOCAL_CAPACITY];
	fds_dsl_frame frames[FDS_DSL_FUNCTION_CAPACITY];
	size_t frame_count;
	size_t instruction_pointer;
	fds_dsl_error error;
	size_t error_instruction;
} fds_dsl_vm;

struct fds_dsl_heap {
	fds_dsl_object *objects;
	fds_dsl_value handles[FDS_DSL_HANDLE_CAPACITY];
	size_t bytes_allocated;
	size_t next_collection;
	size_t object_count;
	size_t handle_count;
};

static inline fds_dsl_value fds_dsl_nil(void) {
	return (fds_dsl_value){ FDS_DSL_TYPE_NIL, { 0 } };
}

static inline fds_dsl_value fds_dsl_bool(bool value) {
	fds_dsl_value result = { FDS_DSL_TYPE_BOOL, { 0 } };
	result.as.boolean = value;
	return result;
}

static inline fds_dsl_value fds_dsl_int(int64_t value) {
	fds_dsl_value result = { FDS_DSL_TYPE_INT, { 0 } };
	result.as.integer = value;
	return result;
}

static inline fds_dsl_value fds_dsl_float(double value) {
	fds_dsl_value result = { FDS_DSL_TYPE_FLOAT, { 0 } };
	result.as.floating = value;
	return result;
}

static inline fds_dsl_value fds_dsl_string(const char *data, size_t length) {
	fds_dsl_value result = { FDS_DSL_TYPE_STRING, { 0 } };
	result.as.string = (fds_dsl_string_view){ data, length, NULL };
	return result;
}

static inline fds_dsl_value fds_dsl_object_value(fds_dsl_object *object) {
	fds_dsl_value result = { FDS_DSL_TYPE_OBJECT, { 0 } };
	result.as.object = object;
	return result;
}

static inline void fds_dsl_heap_init(fds_dsl_heap *heap) {
	if (!heap) return;
	memset(heap, 0, sizeof(*heap));
	heap->next_collection = FDS_DSL_HEAP_INITIAL_LIMIT;
}

static inline void fds_dsl_heap_deinit(fds_dsl_heap *heap) {
	fds_dsl_object *object;
	if (!heap) return;
	object = heap->objects;
	while (object) {
		fds_dsl_object *next = object->next;
		free(object);
		object = next;
	}
	memset(heap, 0, sizeof(*heap));
}

static inline fds_dsl_object *fds_dsl_heap_alloc(fds_dsl_heap *heap,
									 fds_dsl_object_type type,
									 size_t payload_size,
									 fds_dsl_object_trace_fn trace,
									 fds_dsl_object_finalize_fn finalize) {
	fds_dsl_object *object;
	if (!heap || payload_size > SIZE_MAX - sizeof(*object)) return NULL;
	object = (fds_dsl_object *)malloc(sizeof(*object) + payload_size);
	if (!object) return NULL;
	*object = (fds_dsl_object){ type, false, sizeof(*object) + payload_size, heap->objects, trace, finalize };
	heap->objects = object;
	heap->bytes_allocated += object->bytes;
	heap->object_count++;
	return object;
}

static inline void fds_dsl_heap_mark_object(fds_dsl_heap *heap, fds_dsl_object *object) {
	(void)heap;
	if (object) object->marked = true;
}

static inline uint16_t fds_dsl_heap_push_handle(fds_dsl_heap *heap, fds_dsl_value value) {
	if (!heap) return FDS_DSL_INVALID_INDEX;
	for (size_t index = 0; index < heap->handle_count; index++) {
		if (heap->handles[index].type == FDS_DSL_TYPE_NIL) {
			heap->handles[index] = value;
			return (uint16_t)index;
		}
	}
	if (heap->handle_count >= FDS_DSL_HANDLE_CAPACITY) return FDS_DSL_INVALID_INDEX;
	heap->handles[heap->handle_count] = value;
	return (uint16_t)heap->handle_count++;
}

static inline bool fds_dsl_heap_set_handle(fds_dsl_heap *heap, uint16_t handle, fds_dsl_value value) {
	if (!heap || handle >= heap->handle_count) return false;
	heap->handles[handle] = value;
	return true;
}

static inline bool fds_dsl_heap_release_handle(fds_dsl_heap *heap, uint16_t handle) {
	if (!heap || handle >= heap->handle_count) return false;
	heap->handles[handle] = fds_dsl_nil();
	while (heap->handle_count && heap->handles[heap->handle_count - 1].type == FDS_DSL_TYPE_NIL) heap->handle_count--;
	return true;
}

static inline void fds_dsl_program_init(fds_dsl_program *program) {
	if (program) memset(program, 0, sizeof(*program));
}

static inline bool fds_dsl_program_has_import(const fds_dsl_program *program,
										 const char *name,
										 size_t length) {
	if (!program || !name) return false;
	for (size_t index = 0; index < program->import_count; index++) {
		if (program->imports[index].name_length == length &&
			memcmp(program->imports[index].name, name, length) == 0) return true;
	}
	return false;
}

static inline bool fds_dsl_program_add_import(fds_dsl_program *program,
										 const char *name,
										 size_t length) {
	if (!program || !name || length > UINT16_MAX || program->import_count >= FDS_DSL_IMPORT_CAPACITY) return false;
	if (fds_dsl_program_has_import(program, name, length)) return true;
	program->imports[program->import_count++] = (fds_dsl_import){ name, (uint16_t)length };
	return true;
}

static inline bool fds_dsl_emit(fds_dsl_program *program, fds_dsl_opcode opcode, uint16_t operand) {
	if (!program || program->code_count >= FDS_DSL_CODE_CAPACITY) return false;
	program->code[program->code_count++] = (fds_dsl_instruction){ (uint8_t)opcode, operand };
	return true;
}

static inline uint16_t fds_dsl_add_constant(fds_dsl_program *program, fds_dsl_value value) {
	if (!program || program->constant_count >= FDS_DSL_CONSTANT_CAPACITY) return FDS_DSL_INVALID_INDEX;
	program->constants[program->constant_count] = value;
	return (uint16_t)program->constant_count++;
}

static inline uint16_t fds_dsl_add_native(fds_dsl_program *program, const char *name,
										  fds_dsl_native_fn function, void *user_data) {
	if (!program || !function || program->native_count >= FDS_DSL_NATIVE_CAPACITY) return FDS_DSL_INVALID_INDEX;
	program->natives[program->native_count] = (fds_dsl_native){ name, function, user_data, FDS_DSL_TYPE_NIL };
	return (uint16_t)program->native_count++;
}

static inline uint16_t fds_dsl_add_native_typed(fds_dsl_program *program, const char *name,
										 fds_dsl_type result_type,
										 fds_dsl_native_fn function, void *user_data) {
	if (!program || !function || program->native_count >= FDS_DSL_NATIVE_CAPACITY) return FDS_DSL_INVALID_INDEX;
	program->natives[program->native_count] = (fds_dsl_native){ name, function, user_data, result_type };
	return (uint16_t)program->native_count++;
}

static inline bool fds_dsl_set_local_count(fds_dsl_program *program, size_t count) {
	if (!program || count > FDS_DSL_LOCAL_CAPACITY) return false;
	program->local_count = count;
	return true;
}

static inline void fds_dsl_vm_init(fds_dsl_vm *vm, const fds_dsl_program *program) {
	if (!vm) return;
	memset(vm, 0, sizeof(*vm));
	vm->program = program;
}

static inline void fds_dsl_vm_init_with_heap(fds_dsl_vm *vm,
										 const fds_dsl_program *program,
										 fds_dsl_heap *heap) {
	if (!vm) return;
	memset(vm, 0, sizeof(*vm));
	vm->program = program;
	vm->heap = heap;
}

static inline bool fds_dsl_push(fds_dsl_vm *vm, fds_dsl_value value) {
	if (!vm || vm->stack_count >= FDS_DSL_STACK_CAPACITY) return false;
	vm->stack[vm->stack_count++] = value;
	return true;
}

static inline bool fds_dsl_pop(fds_dsl_vm *vm, fds_dsl_value *value) {
	if (!vm || !value || !vm->stack_count) return false;
	*value = vm->stack[--vm->stack_count];
	return true;
}

static inline char *fds_dsl_object_string_data(fds_dsl_object *object) {
	return (char *)(object + 1);
}

static inline void fds_dsl_heap_mark_value(fds_dsl_value value) {
	if (value.type == FDS_DSL_TYPE_STRING && value.as.string.object) value.as.string.object->marked = true;
	if (value.type == FDS_DSL_TYPE_OBJECT) fds_dsl_heap_mark_object(NULL, value.as.object);
}

static inline void fds_dsl_array_trace(fds_dsl_heap *heap, fds_dsl_object *object) {
	fds_dsl_array *array = (fds_dsl_array *)object;
	(void)heap;
	for (size_t index = 0; index < array->count; index++) fds_dsl_heap_mark_value(array->values[index]);
}

static inline fds_dsl_array *fds_dsl_array_new(fds_dsl_heap *heap, size_t count) {
	fds_dsl_array *array;
	if (!heap || count > (SIZE_MAX - sizeof(*array)) / sizeof(array->values[0])) return NULL;
	array = (fds_dsl_array *)fds_dsl_heap_alloc(heap, FDS_DSL_OBJECT_ARRAY,
										 count * sizeof(array->values[0]), fds_dsl_array_trace, NULL);
	if (!array) return NULL;
	array->count = count;
	for (size_t index = 0; index < count; index++) array->values[index] = fds_dsl_nil();
	return array;
}

static inline fds_dsl_value fds_dsl_array_value(fds_dsl_array *array) {
	return array ? fds_dsl_object_value(&array->object) : fds_dsl_nil();
}

static inline bool fds_dsl_array_set(fds_dsl_array *array, size_t index, fds_dsl_value value) {
	if (!array || index >= array->count) return false;
	array->values[index] = value;
	return true;
}

static inline bool fds_dsl_array_get(const fds_dsl_array *array, size_t index, fds_dsl_value *value) {
	if (!array || !value || index >= array->count) return false;
	*value = array->values[index];
	return true;
}

static inline void fds_dsl_heap_mark_roots(fds_dsl_heap *heap, const fds_dsl_vm *vm) {
	if (!heap) return;
	for (size_t index = 0; index < heap->handle_count; index++) fds_dsl_heap_mark_value(heap->handles[index]);
	if (!vm) return;
	for (size_t index = 0; index < vm->stack_count; index++) fds_dsl_heap_mark_value(vm->stack[index]);
	for (size_t index = 0; index < FDS_DSL_LOCAL_CAPACITY; index++) fds_dsl_heap_mark_value(vm->locals[index]);
	for (size_t frame = 0; frame < vm->frame_count; frame++) {
		for (size_t index = 0; index < FDS_DSL_LOCAL_CAPACITY; index++) fds_dsl_heap_mark_value(vm->frames[frame].locals[index]);
	}
	if (vm->program) {
		for (size_t index = 0; index < vm->program->constant_count; index++) fds_dsl_heap_mark_value(vm->program->constants[index]);
	}
}

static inline void fds_dsl_heap_collect(fds_dsl_heap *heap, const fds_dsl_vm *vm) {
	fds_dsl_object **cursor;
	if (!heap) return;
	fds_dsl_heap_mark_roots(heap, vm);
	for (size_t pass = 0; pass < heap->object_count; pass++) {
		for (fds_dsl_object *object = heap->objects; object; object = object->next) {
			if (object->marked && object->trace) object->trace(heap, object);
		}
	}
	cursor = &heap->objects;
	while (*cursor) {
		fds_dsl_object *object = *cursor;
		if (object->marked) {
			object->marked = false;
			cursor = &object->next;
		} else {
			*cursor = object->next;
			heap->bytes_allocated -= object->bytes;
			heap->object_count--;
			if (object->finalize) object->finalize(object);
			free(object);
		}
	}
	heap->next_collection = heap->bytes_allocated + (heap->bytes_allocated / 2) + 1;
}

static inline fds_dsl_value fds_dsl_vm_string(fds_dsl_vm *vm, const char *data, size_t length) {
	fds_dsl_object *object;
	fds_dsl_heap *heap;
	if (!vm || !data) return fds_dsl_nil();
	if (!vm->heap) return fds_dsl_string(data, length);
	heap = vm->heap;
	if (heap->bytes_allocated + sizeof(*object) + length + 1 > heap->next_collection) fds_dsl_heap_collect(heap, vm);
	object = fds_dsl_heap_alloc(heap, FDS_DSL_OBJECT_STRING, length + 1, NULL, NULL);
	if (!object) return fds_dsl_nil();
	memcpy(fds_dsl_object_string_data(object), data, length);
	fds_dsl_object_string_data(object)[length] = '\0';
	return (fds_dsl_value){ FDS_DSL_TYPE_STRING, { .string = { fds_dsl_object_string_data(object), length, object } } };
}

static inline bool fds_dsl_numeric(fds_dsl_type type) {
	return type == FDS_DSL_TYPE_INT || type == FDS_DSL_TYPE_FLOAT;
}

static inline bool fds_dsl_values_equal(fds_dsl_value left, fds_dsl_value right) {
	if (left.type != right.type) return false;
	switch (left.type) {
		case FDS_DSL_TYPE_NIL: return true;
		case FDS_DSL_TYPE_BOOL: return left.as.boolean == right.as.boolean;
		case FDS_DSL_TYPE_INT: return left.as.integer == right.as.integer;
		case FDS_DSL_TYPE_FLOAT: return left.as.floating == right.as.floating;
		case FDS_DSL_TYPE_OBJECT: return left.as.object == right.as.object;
		case FDS_DSL_TYPE_STRING:
			return left.as.string.length == right.as.string.length &&
				   (left.as.string.length == 0 || (left.as.string.data && right.as.string.data &&
					memcmp(left.as.string.data, right.as.string.data, left.as.string.length) == 0));
	}
	return false;
}

static inline fds_dsl_value *fds_dsl_current_locals(fds_dsl_vm *vm) {
	return vm->frame_count ? vm->frames[vm->frame_count - 1].locals : vm->locals;
}

static inline fds_dsl_error fds_dsl_vm_run(fds_dsl_vm *vm, fds_dsl_value *result) {
	const fds_dsl_program *program;
	if (!vm || !vm->program || !result) return FDS_DSL_ERROR_BAD_ARGUMENT;
	program = vm->program;
	while (vm->instruction_pointer < program->code_count) {
		fds_dsl_instruction instruction = program->code[vm->instruction_pointer++];
		fds_dsl_value left, right, value;
		switch ((fds_dsl_opcode)instruction.opcode) {
			case FDS_DSL_OP_CONST:
				if (instruction.operand >= program->constant_count || !fds_dsl_push(vm, program->constants[instruction.operand])) goto bad_bytecode;
				break;
			case FDS_DSL_OP_NIL: if (!fds_dsl_push(vm, fds_dsl_nil())) goto overflow; break;
			case FDS_DSL_OP_TRUE: if (!fds_dsl_push(vm, fds_dsl_bool(true))) goto overflow; break;
			case FDS_DSL_OP_FALSE: if (!fds_dsl_push(vm, fds_dsl_bool(false))) goto overflow; break;
			case FDS_DSL_OP_LOAD_LOCAL:
				if (instruction.operand >= (vm->frame_count ? program->functions[vm->frames[vm->frame_count - 1].function_index].local_count : program->local_count) || !fds_dsl_push(vm, fds_dsl_current_locals(vm)[instruction.operand])) goto bad_bytecode;
				break;
			case FDS_DSL_OP_STORE_LOCAL:
				if (instruction.operand >= (vm->frame_count ? program->functions[vm->frames[vm->frame_count - 1].function_index].local_count : program->local_count) || !fds_dsl_pop(vm, &value)) goto stack_underflow;
				fds_dsl_current_locals(vm)[instruction.operand] = value;
				break;
			case FDS_DSL_OP_LOAD_GLOBAL:
				if (instruction.operand >= program->global_count || !fds_dsl_push(vm, vm->locals[instruction.operand])) goto bad_bytecode;
				break;
			case FDS_DSL_OP_STORE_GLOBAL:
				if (instruction.operand >= program->global_count || !fds_dsl_pop(vm, &value)) goto stack_underflow;
				vm->locals[instruction.operand] = value;
				break;
			case FDS_DSL_OP_POP: if (!fds_dsl_pop(vm, &value)) goto stack_underflow; break;
			case FDS_DSL_OP_DUP:
				if (!vm->stack_count || !fds_dsl_push(vm, vm->stack[vm->stack_count - 1])) goto stack_underflow;
				break;
			case FDS_DSL_OP_NEGATE:
				if (!fds_dsl_pop(vm, &value) || !fds_dsl_numeric(value.type)) goto type_error;
				if (value.type == FDS_DSL_TYPE_INT) value.as.integer = -value.as.integer;
				else value.as.floating = -value.as.floating;
				if (!fds_dsl_push(vm, value)) goto overflow;
				break;
			case FDS_DSL_OP_ADD: case FDS_DSL_OP_SUB: case FDS_DSL_OP_MUL: case FDS_DSL_OP_DIV:
				if (!fds_dsl_pop(vm, &right) || !fds_dsl_pop(vm, &left) || left.type != right.type || !fds_dsl_numeric(left.type)) goto type_error;
				if (right.type == FDS_DSL_TYPE_INT && instruction.opcode == FDS_DSL_OP_DIV && right.as.integer == 0) goto division_by_zero;
				if (right.type == FDS_DSL_TYPE_FLOAT && instruction.opcode == FDS_DSL_OP_DIV && right.as.floating == 0.0) goto division_by_zero;
				if (left.type == FDS_DSL_TYPE_INT) {
					value.type = FDS_DSL_TYPE_INT;
					if (instruction.opcode == FDS_DSL_OP_ADD) value.as.integer = left.as.integer + right.as.integer;
					if (instruction.opcode == FDS_DSL_OP_SUB) value.as.integer = left.as.integer - right.as.integer;
					if (instruction.opcode == FDS_DSL_OP_MUL) value.as.integer = left.as.integer * right.as.integer;
					if (instruction.opcode == FDS_DSL_OP_DIV) value.as.integer = left.as.integer / right.as.integer;
				} else {
					value.type = FDS_DSL_TYPE_FLOAT;
					if (instruction.opcode == FDS_DSL_OP_ADD) value.as.floating = left.as.floating + right.as.floating;
					if (instruction.opcode == FDS_DSL_OP_SUB) value.as.floating = left.as.floating - right.as.floating;
					if (instruction.opcode == FDS_DSL_OP_MUL) value.as.floating = left.as.floating * right.as.floating;
					if (instruction.opcode == FDS_DSL_OP_DIV) value.as.floating = left.as.floating / right.as.floating;
				}
				if (!fds_dsl_push(vm, value)) goto overflow;
				break;
			case FDS_DSL_OP_EQUAL:
				if (!fds_dsl_pop(vm, &right) || !fds_dsl_pop(vm, &left) || !fds_dsl_push(vm, fds_dsl_bool(fds_dsl_values_equal(left, right)))) goto stack_underflow;
				break;
			case FDS_DSL_OP_LESS: case FDS_DSL_OP_GREATER:
				if (!fds_dsl_pop(vm, &right) || !fds_dsl_pop(vm, &left) || left.type != right.type || !fds_dsl_numeric(left.type)) goto type_error;
				if (!fds_dsl_push(vm, fds_dsl_bool(instruction.opcode == FDS_DSL_OP_LESS ? (left.type == FDS_DSL_TYPE_INT ? left.as.integer < right.as.integer : left.as.floating < right.as.floating) : (left.type == FDS_DSL_TYPE_INT ? left.as.integer > right.as.integer : left.as.floating > right.as.floating)))) goto overflow;
				break;
			case FDS_DSL_OP_NOT:
				if (!fds_dsl_pop(vm, &value) || value.type != FDS_DSL_TYPE_BOOL || !fds_dsl_push(vm, fds_dsl_bool(!value.as.boolean))) goto type_error;
				break;
			case FDS_DSL_OP_AND: case FDS_DSL_OP_OR:
				if (!fds_dsl_pop(vm, &right) || !fds_dsl_pop(vm, &left) || left.type != FDS_DSL_TYPE_BOOL || right.type != FDS_DSL_TYPE_BOOL) goto type_error;
				if (!fds_dsl_push(vm, fds_dsl_bool(instruction.opcode == FDS_DSL_OP_AND ? left.as.boolean && right.as.boolean : left.as.boolean || right.as.boolean))) goto overflow;
				break;
			case FDS_DSL_OP_CALL: {
				size_t argument_count = instruction.operand >> 8;
				uint16_t native_index = instruction.operand & 0xff;
				if (native_index >= program->native_count) goto unknown_native;
				if (vm->stack_count < argument_count) goto stack_underflow;
				value = fds_dsl_nil();
				if (!program->natives[native_index].function(vm, vm->stack + vm->stack_count - argument_count, argument_count, &value, program->natives[native_index].user_data)) goto native_error;
				vm->stack_count -= argument_count;
				if (!fds_dsl_push(vm, value)) goto overflow;
				break;
			}
			case FDS_DSL_OP_CALL_FUNCTION: {
				const fds_dsl_function *function;
				fds_dsl_frame *frame;
				if (instruction.operand >= program->function_count || vm->frame_count >= FDS_DSL_FUNCTION_CAPACITY) goto bad_bytecode;
				function = &program->functions[instruction.operand];
				if (vm->stack_count < function->parameter_count) goto stack_underflow;
				frame = &vm->frames[vm->frame_count++];
				*frame = (fds_dsl_frame){ .instruction_pointer = vm->instruction_pointer,
					.stack_base = vm->stack_count - function->parameter_count,
					.function_index = instruction.operand };
				for (size_t index = 0; index < function->parameter_count; index++) {
					value = vm->stack[frame->stack_base + index];
					if (function->parameter_types[index] != FDS_DSL_TYPE_NIL && value.type != function->parameter_types[index]) goto type_error;
					frame->locals[index] = value;
				}
				vm->stack_count = frame->stack_base;
				vm->instruction_pointer = function->code_start;
				break;
			}
			case FDS_DSL_OP_JUMP:
				if (instruction.operand >= program->code_count) goto bad_bytecode;
				vm->instruction_pointer = instruction.operand;
				break;
			case FDS_DSL_OP_JUMP_IF_FALSE:
				if (!fds_dsl_pop(vm, &value) || value.type != FDS_DSL_TYPE_BOOL) goto type_error;
				if (!value.as.boolean) {
					if (instruction.operand >= program->code_count) goto bad_bytecode;
					vm->instruction_pointer = instruction.operand;
				}
				break;
			case FDS_DSL_OP_RETURN: case FDS_DSL_OP_HALT:
				if (instruction.opcode == FDS_DSL_OP_RETURN && vm->frame_count) {
					if (!fds_dsl_pop(vm, &value)) goto stack_underflow;
					if (vm->stack_count < vm->frames[vm->frame_count - 1].stack_base) goto bad_bytecode;
					vm->instruction_pointer = vm->frames[--vm->frame_count].instruction_pointer;
					if (!fds_dsl_push(vm, value)) goto overflow;
					break;
				}
				if (!fds_dsl_pop(vm, result)) goto stack_underflow;
				return vm->error = FDS_DSL_OK;
			case FDS_DSL_OP_RETURN_VALUES:
				if (instruction.operand > FDS_DSL_RETURN_CAPACITY) goto bad_bytecode;
				if (!vm->frame_count) {
					if (!instruction.operand) { *result = fds_dsl_nil(); return vm->error = FDS_DSL_OK; }
					if (vm->stack_count < instruction.operand) goto stack_underflow;
					*result = vm->stack[--vm->stack_count];
					return vm->error = FDS_DSL_OK;
				}
				if (vm->stack_count < vm->frames[vm->frame_count - 1].stack_base + instruction.operand) goto stack_underflow;
				vm->instruction_pointer = vm->frames[--vm->frame_count].instruction_pointer;
				break;
			default: goto bad_bytecode;
		}
	}
	if (vm->frame_count) goto bad_bytecode;
	if (!fds_dsl_pop(vm, result)) goto stack_underflow;
	return vm->error = FDS_DSL_OK;
bad_bytecode: vm->error = FDS_DSL_ERROR_BAD_BYTECODE; goto fail;
overflow: vm->error = FDS_DSL_ERROR_OVERFLOW; goto fail;
stack_underflow: vm->error = FDS_DSL_ERROR_STACK_UNDERFLOW; goto fail;
type_error: vm->error = FDS_DSL_ERROR_TYPE; goto fail;
division_by_zero: vm->error = FDS_DSL_ERROR_DIVISION_BY_ZERO; goto fail;
unknown_native: vm->error = FDS_DSL_ERROR_UNKNOWN_NATIVE; goto fail;
native_error: vm->error = FDS_DSL_ERROR_NATIVE;
fail: vm->error_instruction = vm->instruction_pointer - 1; return vm->error;
}

typedef enum fds_dsl_token_kind {
	FDS_DSL_TOKEN_EOF = 0,
	FDS_DSL_TOKEN_ERROR,
	FDS_DSL_TOKEN_INT,
	FDS_DSL_TOKEN_FLOAT,
	FDS_DSL_TOKEN_STRING,
	FDS_DSL_TOKEN_IDENTIFIER,
	FDS_DSL_TOKEN_TRUE,
	FDS_DSL_TOKEN_FALSE,
	FDS_DSL_TOKEN_LET,
	FDS_DSL_TOKEN_RETURN,
	FDS_DSL_TOKEN_FN,
	FDS_DSL_TOKEN_GLOBAL,
	FDS_DSL_TOKEN_IF,
	FDS_DSL_TOKEN_ELSE,
	FDS_DSL_TOKEN_WHILE,
	FDS_DSL_TOKEN_FOR,
	FDS_DSL_TOKEN_IMPORT,
	FDS_DSL_TOKEN_BREAK,
	FDS_DSL_TOKEN_CONTINUE,
	FDS_DSL_TOKEN_PLUS,
	FDS_DSL_TOKEN_MINUS,
	FDS_DSL_TOKEN_STAR,
	FDS_DSL_TOKEN_SLASH,
	FDS_DSL_TOKEN_EQUAL,
	FDS_DSL_TOKEN_EQUAL_EQUAL,
	FDS_DSL_TOKEN_BANG,
	FDS_DSL_TOKEN_BANG_EQUAL,
	FDS_DSL_TOKEN_LESS,
	FDS_DSL_TOKEN_LESS_EQUAL,
	FDS_DSL_TOKEN_GREATER,
	FDS_DSL_TOKEN_GREATER_EQUAL,
	FDS_DSL_TOKEN_AND_AND,
	FDS_DSL_TOKEN_OR_OR,
	FDS_DSL_TOKEN_LEFT_PAREN,
	FDS_DSL_TOKEN_RIGHT_PAREN,
	FDS_DSL_TOKEN_COLON,
	FDS_DSL_TOKEN_COMMA,
	FDS_DSL_TOKEN_SEMICOLON,
	FDS_DSL_TOKEN_LEFT_BRACE,
	FDS_DSL_TOKEN_RIGHT_BRACE,
} fds_dsl_token_kind;

typedef struct fds_dsl_token {
	fds_dsl_token_kind kind;
	const char *start;
	size_t length;
	size_t line;
	size_t column;
	int64_t integer;
	double floating;
} fds_dsl_token;

typedef struct fds_dsl_lexer {
	const char *source;
	const char *current;
	size_t line;
	size_t column;
} fds_dsl_lexer;

typedef enum fds_dsl_compile_error {
	FDS_DSL_COMPILE_OK = 0,
	FDS_DSL_COMPILE_LEXICAL,
	FDS_DSL_COMPILE_SYNTAX,
	FDS_DSL_COMPILE_TYPE,
	FDS_DSL_COMPILE_NAME,
	FDS_DSL_COMPILE_LIMIT,
	FDS_DSL_COMPILE_NATIVE,
} fds_dsl_compile_error;

typedef struct fds_dsl_compile_diagnostic {
	fds_dsl_compile_error error;
	const char *message;
	size_t line;
	size_t column;
} fds_dsl_compile_diagnostic;

typedef struct fds_dsl_compiler_local {
	const char *name;
	size_t length;
	fds_dsl_type type;
	uint16_t index;
} fds_dsl_compiler_local;

typedef struct fds_dsl_compiler {
	fds_dsl_program *program;
	fds_dsl_lexer lexer;
	fds_dsl_token current;
	fds_dsl_token previous;
	fds_dsl_compiler_local locals[FDS_DSL_LOCAL_CAPACITY];
	size_t local_count;
	bool has_return;
	fds_dsl_compile_diagnostic diagnostic;
} fds_dsl_compiler;

static inline bool fds_dsl_ascii_digit(char character) {
	return character >= '0' && character <= '9';
}

static inline bool fds_dsl_ascii_alpha(char character) {
	return (character >= 'a' && character <= 'z') ||
		   (character >= 'A' && character <= 'Z') || character == '_';
}

static inline bool fds_dsl_token_is(fds_dsl_token token, const char *text) {
	size_t length = strlen(text);
	return token.length == length && memcmp(token.start, text, length) == 0;
}

static inline void fds_dsl_lexer_init(fds_dsl_lexer *lexer, const char *source) {
	*lexer = (fds_dsl_lexer){ source, source, 1, 1 };
}

static inline char fds_dsl_lexer_advance(fds_dsl_lexer *lexer) {
	char character = *lexer->current++;
	if (character == '\n') {
		lexer->line++;
		lexer->column = 1;
	} else {
		lexer->column++;
	}
	return character;
}

static inline char fds_dsl_lexer_peek(const fds_dsl_lexer *lexer) {
	return *lexer->current;
}

static inline char fds_dsl_lexer_peek_next(const fds_dsl_lexer *lexer) {
	return lexer->current[0] ? lexer->current[1] : '\0';
}

static inline fds_dsl_token fds_dsl_make_token(fds_dsl_lexer *lexer,
								   fds_dsl_token_kind kind,
								   const char *start,
								   size_t line,
								   size_t column) {
	fds_dsl_token token = { kind, start, (size_t)(lexer->current - start), line, column, 0, 0 };
	return token;
}

static inline fds_dsl_token fds_dsl_lexer_next(fds_dsl_lexer *lexer) {
	const char *start;
	size_t line, column;
	while (fds_dsl_lexer_peek(lexer) == ' ' || fds_dsl_lexer_peek(lexer) == '\t' ||
		   fds_dsl_lexer_peek(lexer) == '\r' || fds_dsl_lexer_peek(lexer) == '\n') {
		fds_dsl_lexer_advance(lexer);
	}
	start = lexer->current;
	line = lexer->line;
	column = lexer->column;
	if (!fds_dsl_lexer_peek(lexer)) return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_EOF, start, line, column);
	if (fds_dsl_ascii_alpha(fds_dsl_lexer_peek(lexer))) {
		while (fds_dsl_ascii_alpha(fds_dsl_lexer_peek(lexer)) || fds_dsl_ascii_digit(fds_dsl_lexer_peek(lexer))) fds_dsl_lexer_advance(lexer);
		fds_dsl_token token = fds_dsl_make_token(lexer, FDS_DSL_TOKEN_IDENTIFIER, start, line, column);
		if (fds_dsl_token_is(token, "true")) token.kind = FDS_DSL_TOKEN_TRUE;
		else if (fds_dsl_token_is(token, "false")) token.kind = FDS_DSL_TOKEN_FALSE;
		else if (fds_dsl_token_is(token, "let")) token.kind = FDS_DSL_TOKEN_LET;
		else if (fds_dsl_token_is(token, "return")) token.kind = FDS_DSL_TOKEN_RETURN;
		else if (fds_dsl_token_is(token, "fn")) token.kind = FDS_DSL_TOKEN_FN;
		else if (fds_dsl_token_is(token, "global")) token.kind = FDS_DSL_TOKEN_GLOBAL;
		else if (fds_dsl_token_is(token, "if")) token.kind = FDS_DSL_TOKEN_IF;
		else if (fds_dsl_token_is(token, "else")) token.kind = FDS_DSL_TOKEN_ELSE;
		else if (fds_dsl_token_is(token, "while")) token.kind = FDS_DSL_TOKEN_WHILE;
		else if (fds_dsl_token_is(token, "for")) token.kind = FDS_DSL_TOKEN_FOR;
		else if (fds_dsl_token_is(token, "import")) token.kind = FDS_DSL_TOKEN_IMPORT;
		else if (fds_dsl_token_is(token, "break")) token.kind = FDS_DSL_TOKEN_BREAK;
		else if (fds_dsl_token_is(token, "continue")) token.kind = FDS_DSL_TOKEN_CONTINUE;
		return token;
	}
	if (fds_dsl_ascii_digit(fds_dsl_lexer_peek(lexer))) {
		int64_t integer = 0;
		while (fds_dsl_ascii_digit(fds_dsl_lexer_peek(lexer))) {
			integer = integer * 10 + (fds_dsl_lexer_advance(lexer) - '0');
		}
		if (fds_dsl_lexer_peek(lexer) == '.' && fds_dsl_ascii_digit(fds_dsl_lexer_peek_next(lexer))) {
			double fraction = 0.0;
			double divisor = 1.0;
			fds_dsl_lexer_advance(lexer);
			while (fds_dsl_ascii_digit(fds_dsl_lexer_peek(lexer))) {
				fraction = fraction * 10.0 + (fds_dsl_lexer_advance(lexer) - '0');
				divisor *= 10.0;
			}
			fds_dsl_token token = fds_dsl_make_token(lexer, FDS_DSL_TOKEN_FLOAT, start, line, column);
			token.floating = (double)integer + fraction / divisor;
			return token;
		}
		fds_dsl_token token = fds_dsl_make_token(lexer, FDS_DSL_TOKEN_INT, start, line, column);
		token.integer = integer;
		return token;
	}
	if (fds_dsl_lexer_peek(lexer) == '"') {
		fds_dsl_lexer_advance(lexer);
		start = lexer->current;
		while (fds_dsl_lexer_peek(lexer) && fds_dsl_lexer_peek(lexer) != '"' && fds_dsl_lexer_peek(lexer) != '\n') fds_dsl_lexer_advance(lexer);
		if (fds_dsl_lexer_peek(lexer) != '"') return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_ERROR, start, line, column);
		fds_dsl_token token = fds_dsl_make_token(lexer, FDS_DSL_TOKEN_STRING, start, line, column);
		fds_dsl_lexer_advance(lexer);
		return token;
	}
	char character = fds_dsl_lexer_advance(lexer);
	if (character == '=' && fds_dsl_lexer_peek(lexer) == '=') {
		fds_dsl_lexer_advance(lexer);
		return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_EQUAL_EQUAL, start, line, column);
	}
	if (character == '!' && fds_dsl_lexer_peek(lexer) == '=') {
		fds_dsl_lexer_advance(lexer);
		return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_BANG_EQUAL, start, line, column);
	}
	if (character == '<' && fds_dsl_lexer_peek(lexer) == '=') {
		fds_dsl_lexer_advance(lexer);
		return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_LESS_EQUAL, start, line, column);
	}
	if (character == '>' && fds_dsl_lexer_peek(lexer) == '=') {
		fds_dsl_lexer_advance(lexer);
		return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_GREATER_EQUAL, start, line, column);
	}
	if (character == '&' && fds_dsl_lexer_peek(lexer) == '&') {
		fds_dsl_lexer_advance(lexer);
		return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_AND_AND, start, line, column);
	}
	if (character == '|' && fds_dsl_lexer_peek(lexer) == '|') {
		fds_dsl_lexer_advance(lexer);
		return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_OR_OR, start, line, column);
	}
	switch (character) {
		case '+': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_PLUS, start, line, column);
		case '-': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_MINUS, start, line, column);
		case '*': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_STAR, start, line, column);
		case '/': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_SLASH, start, line, column);
		case '=': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_EQUAL, start, line, column);
		case '!': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_BANG, start, line, column);
		case '<': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_LESS, start, line, column);
		case '>': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_GREATER, start, line, column);
		case '(': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_LEFT_PAREN, start, line, column);
		case ')': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_RIGHT_PAREN, start, line, column);
		case ':': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_COLON, start, line, column);
		case ',': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_COMMA, start, line, column);
		case ';': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_SEMICOLON, start, line, column);
		case '{': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_LEFT_BRACE, start, line, column);
		case '}': return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_RIGHT_BRACE, start, line, column);
		default: return fds_dsl_make_token(lexer, FDS_DSL_TOKEN_ERROR, start, line, column);
	}
}

static inline void fds_dsl_compiler_error(fds_dsl_compiler *compiler,
									 fds_dsl_compile_error error,
									 const char *message) {
	if (compiler->diagnostic.error == FDS_DSL_COMPILE_OK) {
		compiler->diagnostic = (fds_dsl_compile_diagnostic){ error, message,
			compiler->current.line, compiler->current.column };
	}
}

static inline void fds_dsl_compiler_advance(fds_dsl_compiler *compiler) {
	compiler->previous = compiler->current;
	compiler->current = fds_dsl_lexer_next(&compiler->lexer);
	if (compiler->current.kind == FDS_DSL_TOKEN_ERROR) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LEXICAL, "invalid token");
}

static inline bool fds_dsl_compiler_match(fds_dsl_compiler *compiler, fds_dsl_token_kind kind) {
	if (compiler->current.kind != kind) return false;
	fds_dsl_compiler_advance(compiler);
	return true;
}

static inline bool fds_dsl_compiler_expect(fds_dsl_compiler *compiler, fds_dsl_token_kind kind, const char *message) {
	if (fds_dsl_compiler_match(compiler, kind)) return true;
	fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_SYNTAX, message);
	return false;
}

static inline bool fds_dsl_name_equal(const char *left, size_t left_length, const char *right, size_t right_length) {
	return left_length == right_length && memcmp(left, right, left_length) == 0;
}

static inline int fds_dsl_find_local(const fds_dsl_compiler *compiler, fds_dsl_token token) {
	for (size_t index = compiler->local_count; index > 0; index--) {
		const fds_dsl_compiler_local *local = &compiler->locals[index - 1];
		if (fds_dsl_name_equal(local->name, local->length, token.start, token.length)) return (int)(index - 1);
	}
	return -1;
}

static inline int fds_dsl_find_native(const fds_dsl_program *program, fds_dsl_token token) {
	for (size_t index = 0; index < program->native_count; index++) {
		const char *name = program->natives[index].name;
		if (name && fds_dsl_name_equal(name, strlen(name), token.start, token.length)) return (int)index;
	}
	return -1;
}

static inline bool fds_dsl_emit_call(fds_dsl_program *program, uint16_t native_index, size_t argument_count) {
	if (native_index > UINT8_MAX || argument_count > UINT8_MAX) return false;
	return fds_dsl_emit(program, FDS_DSL_OP_CALL, (uint16_t)((argument_count << 8) | native_index));
}

static inline fds_dsl_type fds_dsl_parse_expression(fds_dsl_compiler *compiler);

static inline fds_dsl_type fds_dsl_parse_primary(fds_dsl_compiler *compiler) {
	fds_dsl_token token = compiler->current;
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_INT)) {
		uint16_t constant = fds_dsl_add_constant(compiler->program, fds_dsl_int(token.integer));
		if (constant == FDS_DSL_INVALID_INDEX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_CONST, constant)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LIMIT, "constant or bytecode limit reached");
		return FDS_DSL_TYPE_INT;
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_FLOAT)) {
		uint16_t constant = fds_dsl_add_constant(compiler->program, fds_dsl_float(token.floating));
		if (constant == FDS_DSL_INVALID_INDEX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_CONST, constant)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LIMIT, "constant or bytecode limit reached");
		return FDS_DSL_TYPE_FLOAT;
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_STRING)) {
		uint16_t constant = fds_dsl_add_constant(compiler->program, fds_dsl_string(token.start, token.length));
		if (constant == FDS_DSL_INVALID_INDEX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_CONST, constant)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LIMIT, "constant or bytecode limit reached");
		return FDS_DSL_TYPE_STRING;
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_TRUE)) {
		fds_dsl_emit(compiler->program, FDS_DSL_OP_TRUE, 0);
		return FDS_DSL_TYPE_BOOL;
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_FALSE)) {
		fds_dsl_emit(compiler->program, FDS_DSL_OP_FALSE, 0);
		return FDS_DSL_TYPE_BOOL;
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_LEFT_PAREN)) {
		fds_dsl_type type = fds_dsl_parse_expression(compiler);
		fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after expression");
		return type;
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_IDENTIFIER)) {
		int local = fds_dsl_find_local(compiler, token);
		if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_LEFT_PAREN)) {
			int native = fds_dsl_find_native(compiler->program, token);
			size_t argument_count = 0;
			if (compiler->current.kind != FDS_DSL_TOKEN_RIGHT_PAREN) {
				do {
					if (argument_count >= UINT8_MAX) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many native arguments");
					fds_dsl_parse_expression(compiler);
					argument_count++;
				} while (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_COMMA));
			}
			fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after arguments");
			if (native < 0) {
				fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_NATIVE, "unknown native function");
				return FDS_DSL_TYPE_NIL;
			}
			if (!fds_dsl_emit_call(compiler->program, (uint16_t)native, argument_count)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LIMIT, "native call encoding limit reached");
			return compiler->program->natives[native].result_type;
		}
		if (local < 0) {
			fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_NAME, "unknown variable");
			return FDS_DSL_TYPE_NIL;
		}
		fds_dsl_emit(compiler->program, FDS_DSL_OP_LOAD_LOCAL, compiler->locals[local].index);
		return compiler->locals[local].type;
	}
	fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_SYNTAX, "expected expression");
	return FDS_DSL_TYPE_NIL;
}

static inline fds_dsl_type fds_dsl_parse_unary(fds_dsl_compiler *compiler) {
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_MINUS)) {
		fds_dsl_type type = fds_dsl_parse_unary(compiler);
		if (!fds_dsl_numeric(type)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_TYPE, "unary '-' expects a number");
		fds_dsl_emit(compiler->program, FDS_DSL_OP_NEGATE, 0);
		return type;
	}
	return fds_dsl_parse_primary(compiler);
}

static inline fds_dsl_type fds_dsl_parse_factor(fds_dsl_compiler *compiler) {
	fds_dsl_type left = fds_dsl_parse_unary(compiler);
	while (compiler->current.kind == FDS_DSL_TOKEN_STAR || compiler->current.kind == FDS_DSL_TOKEN_SLASH) {
		fds_dsl_token_kind token_operator = compiler->current.kind;
		fds_dsl_compiler_advance(compiler);
		fds_dsl_type right = fds_dsl_parse_unary(compiler);
		if (left != right || !fds_dsl_numeric(left)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_TYPE, "arithmetic operands must have the same numeric type");
		fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_STAR ? FDS_DSL_OP_MUL : FDS_DSL_OP_DIV, 0);
	}
	return left;
}

static inline fds_dsl_type fds_dsl_parse_expression(fds_dsl_compiler *compiler) {
	fds_dsl_type left = fds_dsl_parse_factor(compiler);
	while (compiler->current.kind == FDS_DSL_TOKEN_PLUS || compiler->current.kind == FDS_DSL_TOKEN_MINUS) {
		fds_dsl_token_kind token_operator = compiler->current.kind;
		fds_dsl_compiler_advance(compiler);
		fds_dsl_type right = fds_dsl_parse_factor(compiler);
		if (left != right || !fds_dsl_numeric(left)) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_TYPE, "arithmetic operands must have the same numeric type");
		fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_PLUS ? FDS_DSL_OP_ADD : FDS_DSL_OP_SUB, 0);
	}
	if (compiler->current.kind == FDS_DSL_TOKEN_EQUAL_EQUAL || compiler->current.kind == FDS_DSL_TOKEN_LESS || compiler->current.kind == FDS_DSL_TOKEN_GREATER) {
		fds_dsl_token_kind token_operator = compiler->current.kind;
		fds_dsl_compiler_advance(compiler);
		fds_dsl_type right = fds_dsl_parse_factor(compiler);
		if (left != right || (token_operator != FDS_DSL_TOKEN_EQUAL_EQUAL && !fds_dsl_numeric(left))) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_TYPE, "comparison operands have incompatible types");
		fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_EQUAL_EQUAL ? FDS_DSL_OP_EQUAL : token_operator == FDS_DSL_TOKEN_LESS ? FDS_DSL_OP_LESS : FDS_DSL_OP_GREATER, 0);
		return FDS_DSL_TYPE_BOOL;
	}
	return left;
}

static inline bool fds_dsl_parse_statement(fds_dsl_compiler *compiler) {
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_LET)) {
		fds_dsl_token name = compiler->current;
		fds_dsl_type declared = FDS_DSL_TYPE_NIL;
		if (!fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_IDENTIFIER, "expected variable name")) return false;
		if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_COLON)) {
			fds_dsl_token type = compiler->current;
			if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_IDENTIFIER)) {
				if (fds_dsl_token_is(type, "int")) declared = FDS_DSL_TYPE_INT;
				else if (fds_dsl_token_is(type, "float")) declared = FDS_DSL_TYPE_FLOAT;
				else if (fds_dsl_token_is(type, "bool")) declared = FDS_DSL_TYPE_BOOL;
				else if (fds_dsl_token_is(type, "string")) declared = FDS_DSL_TYPE_STRING;
				else fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_TYPE, "unknown variable type");
			} else fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_SYNTAX, "expected type name");
		}
		if (!fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_EQUAL, "expected '=' after variable name")) return false;
		fds_dsl_type inferred = fds_dsl_parse_expression(compiler);
		if (declared != FDS_DSL_TYPE_NIL && declared != inferred) fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_TYPE, "initializer type does not match declaration");
		if (compiler->local_count >= FDS_DSL_LOCAL_CAPACITY || fds_dsl_find_local(compiler, name) >= 0) {
			fds_dsl_compiler_error(compiler, FDS_DSL_COMPILE_LIMIT, "duplicate or too many local variables");
		} else {
			uint16_t index = (uint16_t)compiler->local_count;
			compiler->locals[compiler->local_count++] = (fds_dsl_compiler_local){ name.start, name.length, declared ? declared : inferred, index };
			fds_dsl_set_local_count(compiler->program, compiler->local_count);
			fds_dsl_emit(compiler->program, FDS_DSL_OP_STORE_LOCAL, index);
		}
		return fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after declaration");
	}
	if (fds_dsl_compiler_match(compiler, FDS_DSL_TOKEN_RETURN)) {
		fds_dsl_parse_expression(compiler);
		fds_dsl_emit(compiler->program, FDS_DSL_OP_RETURN, 0);
		compiler->has_return = true;
		return fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after return");
	}
	fds_dsl_parse_expression(compiler);
	fds_dsl_emit(compiler->program, FDS_DSL_OP_POP, 0);
	return fds_dsl_compiler_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after expression");
}

static inline fds_dsl_compile_error fds_dsl_compile_legacy(const char *source,
									 fds_dsl_program *program,
									 fds_dsl_compile_diagnostic *diagnostic) {
	fds_dsl_compiler compiler;
	if (!source || !program) return FDS_DSL_COMPILE_LEXICAL;
	fds_dsl_program_init(program);
	compiler = (fds_dsl_compiler){ 0 };
	compiler.program = program;
	fds_dsl_lexer_init(&compiler.lexer, source);
	compiler.current = fds_dsl_lexer_next(&compiler.lexer);
	while (compiler.current.kind != FDS_DSL_TOKEN_EOF && compiler.diagnostic.error == FDS_DSL_COMPILE_OK) fds_dsl_parse_statement(&compiler);
	if (compiler.diagnostic.error == FDS_DSL_COMPILE_OK && !compiler.has_return) {
		fds_dsl_emit(program, FDS_DSL_OP_NIL, 0);
		fds_dsl_emit(program, FDS_DSL_OP_RETURN, 0);
	}
	if (diagnostic) *diagnostic = compiler.diagnostic;
	return compiler.diagnostic.error;
}

typedef struct fds_dsl2_variable {
	const char *name;
	size_t length;
	fds_dsl_type type;
	uint16_t index;
	bool global;
} fds_dsl2_variable;

typedef struct fds_dsl2_compiler {
	fds_dsl_program *program;
	fds_dsl_lexer lexer;
	fds_dsl_token current;
	fds_dsl_compile_diagnostic *diagnostic;
	fds_dsl2_variable variables[FDS_DSL_LOCAL_CAPACITY];
	size_t variable_count;
	size_t maximum_local_count;
	size_t scopes[FDS_DSL_LOCAL_CAPACITY];
	size_t scope_count;
	uint16_t function_index;
	uint16_t expected_return_count;
	bool root;
	bool has_return;
	fds_dsl_import_fn import_function;
	void *import_user_data;
	size_t loop_depth;
	size_t loop_break_count;
	size_t loop_breaks[FDS_DSL_LOCAL_CAPACITY];
	size_t loop_break_starts[FDS_DSL_LOCAL_CAPACITY];
	size_t loop_continue_targets[FDS_DSL_LOCAL_CAPACITY];
} fds_dsl2_compiler;

static inline void fds_dsl2_error(fds_dsl2_compiler *compiler,
								  fds_dsl_compile_error error,
								  const char *message) {
	if (compiler->diagnostic->error == FDS_DSL_COMPILE_OK) {
		*compiler->diagnostic = (fds_dsl_compile_diagnostic){ error, message,
			compiler->current.line, compiler->current.column };
	}
}

static inline void fds_dsl2_advance(fds_dsl2_compiler *compiler) {
	compiler->current = fds_dsl_lexer_next(&compiler->lexer);
	if (compiler->current.kind == FDS_DSL_TOKEN_ERROR) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LEXICAL, "invalid token");
}

static inline bool fds_dsl2_match(fds_dsl2_compiler *compiler, fds_dsl_token_kind kind) {
	if (compiler->current.kind != kind) return false;
	fds_dsl2_advance(compiler);
	return true;
}

static inline bool fds_dsl2_expect(fds_dsl2_compiler *compiler, fds_dsl_token_kind kind, const char *message) {
	if (fds_dsl2_match(compiler, kind)) return true;
	fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, message);
	return false;
}

static inline bool fds_dsl2_is_type(fds_dsl_token token) {
	return token.kind == FDS_DSL_TOKEN_IDENTIFIER &&
		(fds_dsl_token_is(token, "int") || fds_dsl_token_is(token, "float") ||
		 fds_dsl_token_is(token, "bool") || fds_dsl_token_is(token, "string") ||
		 fds_dsl_token_is(token, "nil"));
}

static inline fds_dsl_type fds_dsl2_type(fds_dsl_token token) {
	if (fds_dsl_token_is(token, "int")) return FDS_DSL_TYPE_INT;
	if (fds_dsl_token_is(token, "float")) return FDS_DSL_TYPE_FLOAT;
	if (fds_dsl_token_is(token, "bool")) return FDS_DSL_TYPE_BOOL;
	if (fds_dsl_token_is(token, "string")) return FDS_DSL_TYPE_STRING;
	return FDS_DSL_TYPE_NIL;
}

static inline int fds_dsl2_find_variable(const fds_dsl2_compiler *compiler, fds_dsl_token token) {
	for (size_t index = compiler->variable_count; index > 0; index--) {
		const fds_dsl2_variable *variable = &compiler->variables[index - 1];
		if (fds_dsl_name_equal(variable->name, variable->length, token.start, token.length)) return (int)(index - 1);
	}
	return -1;
}

static inline int fds_dsl2_find_global(const fds_dsl_program *program, fds_dsl_token token) {
	for (size_t index = 0; index < program->global_count; index++) {
		if (fds_dsl_name_equal(program->globals[index].name, program->globals[index].name_length, token.start, token.length)) return (int)index;
	}
	return -1;
}

static inline void fds_dsl2_begin_scope(fds_dsl2_compiler *compiler) {
	if (compiler->scope_count < FDS_DSL_LOCAL_CAPACITY) compiler->scopes[compiler->scope_count++] = compiler->variable_count;
}

static inline void fds_dsl2_end_scope(fds_dsl2_compiler *compiler) {
	if (compiler->scope_count) compiler->variable_count = compiler->scopes[--compiler->scope_count];
}

static inline fds_dsl_token fds_dsl2_peek_token(const fds_dsl2_compiler *compiler) {
	fds_dsl_lexer lexer = compiler->lexer;
	return fds_dsl_lexer_next(&lexer);
}

static inline fds_dsl_type fds_dsl2_expression(fds_dsl2_compiler *compiler);

static inline fds_dsl_type fds_dsl2_primary(fds_dsl2_compiler *compiler) {
	fds_dsl_token token = compiler->current;
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_INT)) {
		uint16_t constant = fds_dsl_add_constant(compiler->program, fds_dsl_int(token.integer));
		if (constant == FDS_DSL_INVALID_INDEX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_CONST, constant)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "bytecode limit reached");
		return FDS_DSL_TYPE_INT;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_FLOAT)) {
		uint16_t constant = fds_dsl_add_constant(compiler->program, fds_dsl_float(token.floating));
		if (constant == FDS_DSL_INVALID_INDEX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_CONST, constant)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "bytecode limit reached");
		return FDS_DSL_TYPE_FLOAT;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_STRING)) {
		uint16_t constant = fds_dsl_add_constant(compiler->program, fds_dsl_string(token.start, token.length));
		if (constant == FDS_DSL_INVALID_INDEX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_CONST, constant)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "bytecode limit reached");
		return FDS_DSL_TYPE_STRING;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_TRUE)) { fds_dsl_emit(compiler->program, FDS_DSL_OP_TRUE, 0); return FDS_DSL_TYPE_BOOL; }
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_FALSE)) { fds_dsl_emit(compiler->program, FDS_DSL_OP_FALSE, 0); return FDS_DSL_TYPE_BOOL; }
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_LEFT_PAREN)) {
		fds_dsl_type type = fds_dsl2_expression(compiler);
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after expression");
		return type;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_IDENTIFIER)) {
		int variable = fds_dsl2_find_variable(compiler, token);
		if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_LEFT_PAREN)) {
			int native = fds_dsl_find_native(compiler->program, token);
			int function = -1;
			for (size_t index = 0; index < compiler->program->function_count; index++) {
				fds_dsl_function *candidate = &compiler->program->functions[index];
				if (fds_dsl_name_equal(candidate->name, candidate->name_length, token.start, token.length)) { function = (int)index; break; }
			}
			size_t argument_count = 0;
			if (compiler->current.kind != FDS_DSL_TOKEN_RIGHT_PAREN) {
				do { fds_dsl2_expression(compiler); argument_count++; } while (fds_dsl2_match(compiler, FDS_DSL_TOKEN_COMMA));
			}
			fds_dsl2_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after arguments");
			if (function >= 0) {
				if (argument_count != compiler->program->functions[function].parameter_count) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "wrong number of function arguments");
				if (!fds_dsl_emit(compiler->program, FDS_DSL_OP_CALL_FUNCTION, (uint16_t)function)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "function limit reached");
				return compiler->program->functions[function].return_count ? compiler->program->functions[function].return_types[0] : FDS_DSL_TYPE_NIL;
			}
			if (native >= 0) {
				if (!fds_dsl_emit_call(compiler->program, (uint16_t)native, argument_count)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "native call limit reached");
				return compiler->program->natives[native].result_type;
			}
			fds_dsl2_error(compiler, FDS_DSL_COMPILE_NAME, "unknown function");
			return FDS_DSL_TYPE_NIL;
		}
		if (variable >= 0) {
			fds_dsl_emit(compiler->program, compiler->variables[variable].global ? FDS_DSL_OP_LOAD_GLOBAL : FDS_DSL_OP_LOAD_LOCAL, compiler->variables[variable].index);
			return compiler->variables[variable].type;
		}
		int global = fds_dsl2_find_global(compiler->program, token);
		if (global < 0) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_NAME, "unknown variable"); return FDS_DSL_TYPE_NIL; }
		fds_dsl_emit(compiler->program, FDS_DSL_OP_LOAD_GLOBAL, (uint16_t)global);
		return compiler->program->globals[global].type;
	}
	fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, "expected expression");
	return FDS_DSL_TYPE_NIL;
}

static inline fds_dsl_type fds_dsl2_unary(fds_dsl2_compiler *compiler) {
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_BANG)) {
		fds_dsl_type type = fds_dsl2_unary(compiler);
		if (type != FDS_DSL_TYPE_BOOL) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "unary '!' expects bool");
		fds_dsl_emit(compiler->program, FDS_DSL_OP_NOT, 0);
		return FDS_DSL_TYPE_BOOL;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_MINUS)) {
		fds_dsl_type type = fds_dsl2_unary(compiler);
		if (!fds_dsl_numeric(type)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "unary '-' expects a number");
		fds_dsl_emit(compiler->program, FDS_DSL_OP_NEGATE, 0);
		return type;
	}
	return fds_dsl2_primary(compiler);
}

static inline fds_dsl_type fds_dsl2_factor(fds_dsl2_compiler *compiler) {
	fds_dsl_type left = fds_dsl2_unary(compiler);
	while (compiler->current.kind == FDS_DSL_TOKEN_STAR || compiler->current.kind == FDS_DSL_TOKEN_SLASH) {
		fds_dsl_token_kind token_operator = compiler->current.kind;
		fds_dsl2_advance(compiler);
		fds_dsl_type right = fds_dsl2_unary(compiler);
		if (left != right || !fds_dsl_numeric(left)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "numeric operands must have the same type");
		fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_STAR ? FDS_DSL_OP_MUL : FDS_DSL_OP_DIV, 0);
	}
	return left;
}

static inline fds_dsl_type fds_dsl2_comparison(fds_dsl2_compiler *compiler) {
	fds_dsl_type left = fds_dsl2_factor(compiler);
	while (compiler->current.kind == FDS_DSL_TOKEN_PLUS || compiler->current.kind == FDS_DSL_TOKEN_MINUS) {
		fds_dsl_token_kind token_operator = compiler->current.kind;
		fds_dsl2_advance(compiler);
		fds_dsl_type right = fds_dsl2_factor(compiler);
		if (left != right || !fds_dsl_numeric(left)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "numeric operands must have the same type");
		fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_PLUS ? FDS_DSL_OP_ADD : FDS_DSL_OP_SUB, 0);
	}
	if (compiler->current.kind == FDS_DSL_TOKEN_EQUAL_EQUAL || compiler->current.kind == FDS_DSL_TOKEN_BANG_EQUAL ||
		compiler->current.kind == FDS_DSL_TOKEN_LESS || compiler->current.kind == FDS_DSL_TOKEN_LESS_EQUAL ||
		compiler->current.kind == FDS_DSL_TOKEN_GREATER || compiler->current.kind == FDS_DSL_TOKEN_GREATER_EQUAL) {
		fds_dsl_token_kind token_operator = compiler->current.kind;
		fds_dsl2_advance(compiler);
		fds_dsl_type right = fds_dsl2_factor(compiler);
		if (left != right || (token_operator != FDS_DSL_TOKEN_EQUAL_EQUAL && token_operator != FDS_DSL_TOKEN_BANG_EQUAL && !fds_dsl_numeric(left))) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "incompatible comparison operands");
		if (token_operator == FDS_DSL_TOKEN_EQUAL_EQUAL || token_operator == FDS_DSL_TOKEN_BANG_EQUAL) {
			fds_dsl_emit(compiler->program, FDS_DSL_OP_EQUAL, 0);
			if (token_operator == FDS_DSL_TOKEN_BANG_EQUAL) fds_dsl_emit(compiler->program, FDS_DSL_OP_NOT, 0);
		} else if (token_operator == FDS_DSL_TOKEN_LESS || token_operator == FDS_DSL_TOKEN_GREATER) {
			fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_LESS ? FDS_DSL_OP_LESS : FDS_DSL_OP_GREATER, 0);
		} else {
			fds_dsl_emit(compiler->program, token_operator == FDS_DSL_TOKEN_LESS_EQUAL ? FDS_DSL_OP_GREATER : FDS_DSL_OP_LESS, 0);
			fds_dsl_emit(compiler->program, FDS_DSL_OP_NOT, 0);
		}
		return FDS_DSL_TYPE_BOOL;
	}
	return left;
}

static inline fds_dsl_type fds_dsl2_logical_and(fds_dsl2_compiler *compiler) {
	fds_dsl_type left = fds_dsl2_comparison(compiler);
	while (fds_dsl2_match(compiler, FDS_DSL_TOKEN_AND_AND)) {
		fds_dsl_type right = fds_dsl2_comparison(compiler);
		if (left != FDS_DSL_TYPE_BOOL || right != FDS_DSL_TYPE_BOOL) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "'&&' expects bool operands");
		fds_dsl_emit(compiler->program, FDS_DSL_OP_AND, 0);
		left = FDS_DSL_TYPE_BOOL;
	}
	return left;
}

static inline fds_dsl_type fds_dsl2_expression(fds_dsl2_compiler *compiler) {
	fds_dsl_type left = fds_dsl2_logical_and(compiler);
	while (fds_dsl2_match(compiler, FDS_DSL_TOKEN_OR_OR)) {
		fds_dsl_type right = fds_dsl2_logical_and(compiler);
		if (left != FDS_DSL_TYPE_BOOL || right != FDS_DSL_TYPE_BOOL) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "'||' expects bool operands");
		fds_dsl_emit(compiler->program, FDS_DSL_OP_OR, 0);
		left = FDS_DSL_TYPE_BOOL;
	}
	return left;
}

static inline bool fds_dsl2_declaration(fds_dsl2_compiler *compiler, fds_dsl_type type, fds_dsl_token name, bool global) {
	if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_EQUAL, "expected '=' in declaration")) return false;
	fds_dsl_type value_type = fds_dsl2_expression(compiler);
	if (value_type != type) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "initializer type does not match declaration");
	if (global) {
		if (compiler->program->global_count >= FDS_DSL_LOCAL_CAPACITY) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many global variables"); return false; }
		if (fds_dsl2_find_global(compiler->program, name) >= 0) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_NAME, "duplicate global variable"); return false; }
		compiler->program->globals[compiler->program->global_count] = (fds_dsl_global){ name.start, (uint16_t)name.length, type, (uint16_t)compiler->program->global_count };
		compiler->program->global_count++;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_STORE_GLOBAL, (uint16_t)(compiler->program->global_count - 1));
	} else {
		if (compiler->variable_count >= FDS_DSL_LOCAL_CAPACITY) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many local variables"); return false; }
		uint16_t index = (uint16_t)compiler->variable_count;
		compiler->variables[compiler->variable_count++] = (fds_dsl2_variable){ name.start, name.length, type, index, false };
		if (compiler->variable_count > compiler->maximum_local_count) compiler->maximum_local_count = compiler->variable_count;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_STORE_LOCAL, index);
	}
	return fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after declaration");
}

static inline bool fds_dsl2_inferred_assignment_end(fds_dsl2_compiler *compiler, fds_dsl_token name, bool global, bool require_semicolon) {
	int variable = fds_dsl2_find_variable(compiler, name);
	int global_index = fds_dsl2_find_global(compiler->program, name);
	if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_EQUAL, "expected '=' after variable name")) return false;
	fds_dsl_type type = fds_dsl2_expression(compiler);
	if (global || (variable < 0 && (compiler->root || global_index >= 0))) {
		if (global_index >= 0) {
			if (compiler->program->globals[global_index].type != type) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "assigned value has the wrong type");
			fds_dsl_emit(compiler->program, FDS_DSL_OP_STORE_GLOBAL, (uint16_t)global_index);
		} else {
			if (compiler->program->global_count >= FDS_DSL_LOCAL_CAPACITY) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many global variables"); return false; }
			uint16_t index = (uint16_t)compiler->program->global_count;
			compiler->program->globals[index] = (fds_dsl_global){ name.start, (uint16_t)name.length, type, index };
			compiler->program->global_count++;
			fds_dsl_emit(compiler->program, FDS_DSL_OP_STORE_GLOBAL, index);
		}
	} else if (variable >= 0) {
		if (compiler->variables[variable].type != type) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "assigned value has the wrong type");
		fds_dsl_emit(compiler->program, compiler->variables[variable].global ? FDS_DSL_OP_STORE_GLOBAL : FDS_DSL_OP_STORE_LOCAL, compiler->variables[variable].index);
	} else {
		if (compiler->variable_count >= FDS_DSL_LOCAL_CAPACITY) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many local variables"); return false; }
		uint16_t index = (uint16_t)compiler->variable_count;
		compiler->variables[compiler->variable_count++] = (fds_dsl2_variable){ name.start, name.length, type, index, false };
		if (compiler->variable_count > compiler->maximum_local_count) compiler->maximum_local_count = compiler->variable_count;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_STORE_LOCAL, index);
	}
	return !require_semicolon || fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after assignment");
}

static inline bool fds_dsl2_inferred_assignment(fds_dsl2_compiler *compiler, fds_dsl_token name, bool global) {
	return fds_dsl2_inferred_assignment_end(compiler, name, global, true);
}

static inline bool fds_dsl2_block(fds_dsl2_compiler *compiler);

static inline bool fds_dsl2_patch_jump(fds_dsl2_compiler *compiler, size_t instruction_index) {
	if (compiler->program->code_count > UINT16_MAX) {
		fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "bytecode jump target is too large");
		return false;
	}
	compiler->program->code[instruction_index].operand = (uint16_t)compiler->program->code_count;
	return true;
}

static inline bool fds_dsl2_begin_loop(fds_dsl2_compiler *compiler, size_t continue_target) {
	if (compiler->loop_depth >= FDS_DSL_LOCAL_CAPACITY) {
		fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many nested loops");
		return false;
	}
	compiler->loop_break_starts[compiler->loop_depth] = compiler->loop_break_count;
	compiler->loop_continue_targets[compiler->loop_depth++] = continue_target;
	return true;
}

static inline bool fds_dsl2_end_loop(fds_dsl2_compiler *compiler) {
	size_t start;
	if (!compiler->loop_depth) return false;
	start = compiler->loop_break_starts[--compiler->loop_depth];
	for (size_t index = start; index < compiler->loop_break_count; index++) fds_dsl2_patch_jump(compiler, compiler->loop_breaks[index]);
	compiler->loop_break_count = start;
	return true;
}

static inline bool fds_dsl2_control_statement(fds_dsl2_compiler *compiler) {
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_IF)) {
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_PAREN, "expected '(' after if");
		fds_dsl_type condition_type = fds_dsl2_expression(compiler);
		if (condition_type != FDS_DSL_TYPE_BOOL) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "if condition must be bool");
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after if condition");
		if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_BRACE, "expected '{' after if condition")) return false;
		size_t false_jump = compiler->program->code_count;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP_IF_FALSE, 0);
		fds_dsl2_block(compiler);
		if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_ELSE)) {
			size_t end_jump = compiler->program->code_count;
			fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, 0);
			fds_dsl2_patch_jump(compiler, false_jump);
			if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_BRACE, "expected '{' after else")) return false;
			fds_dsl2_block(compiler);
			fds_dsl2_patch_jump(compiler, end_jump);
		} else {
			fds_dsl2_patch_jump(compiler, false_jump);
		}
		return true;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_WHILE)) {
		size_t loop_start = compiler->program->code_count;
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_PAREN, "expected '(' after while");
		fds_dsl_type condition_type = fds_dsl2_expression(compiler);
		if (condition_type != FDS_DSL_TYPE_BOOL) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "while condition must be bool");
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after while condition");
		if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_BRACE, "expected '{' after while condition")) return false;
		size_t exit_jump = compiler->program->code_count;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP_IF_FALSE, 0);
		fds_dsl2_begin_loop(compiler, loop_start);
		fds_dsl2_block(compiler);
		if (loop_start > UINT16_MAX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, (uint16_t)loop_start)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "loop target is too large");
		fds_dsl2_patch_jump(compiler, exit_jump);
		fds_dsl2_end_loop(compiler);
		return true;
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_FOR)) {
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_PAREN, "expected '(' after for");
		if (compiler->current.kind != FDS_DSL_TOKEN_SEMICOLON) {
			bool global = fds_dsl2_match(compiler, FDS_DSL_TOKEN_GLOBAL);
			if (compiler->current.kind != FDS_DSL_TOKEN_IDENTIFIER) {
				fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, "for initializer must be an assignment");
			} else {
				fds_dsl_token name = compiler->current;
				fds_dsl2_advance(compiler);
				fds_dsl2_inferred_assignment_end(compiler, name, global, false);
			}
		}
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after for initializer");
		size_t loop_start = compiler->program->code_count;
		if (compiler->current.kind == FDS_DSL_TOKEN_SEMICOLON) {
			fds_dsl_emit(compiler->program, FDS_DSL_OP_TRUE, 0);
		} else {
			fds_dsl_type condition_type = fds_dsl2_expression(compiler);
			if (condition_type != FDS_DSL_TYPE_BOOL) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "for condition must be bool");
		}
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after for condition");
		size_t exit_jump = compiler->program->code_count;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP_IF_FALSE, 0);
		size_t body_jump = compiler->program->code_count;
		fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, 0);
		size_t increment_start = compiler->program->code_count;
		if (compiler->current.kind != FDS_DSL_TOKEN_RIGHT_PAREN) {
			bool global = fds_dsl2_match(compiler, FDS_DSL_TOKEN_GLOBAL);
			if (compiler->current.kind != FDS_DSL_TOKEN_IDENTIFIER) {
				fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, "for increment must be an assignment");
			} else {
				fds_dsl_token name = compiler->current;
				fds_dsl2_advance(compiler);
				fds_dsl2_inferred_assignment_end(compiler, name, global, false);
			}
		}
		fds_dsl2_expect(compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after for clauses");
		if (loop_start > UINT16_MAX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, (uint16_t)loop_start)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "for loop target is too large");
		fds_dsl2_patch_jump(compiler, body_jump);
		fds_dsl2_begin_loop(compiler, increment_start);
		if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_LEFT_BRACE, "expected '{' after for clauses")) return false;
		fds_dsl2_block(compiler);
		if (increment_start > UINT16_MAX || !fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, (uint16_t)increment_start)) fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "for loop target is too large");
		fds_dsl2_patch_jump(compiler, exit_jump);
		fds_dsl2_end_loop(compiler);
		return true;
	}
	return false;
}

static inline bool fds_dsl2_statement(fds_dsl2_compiler *compiler) {
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_LEFT_BRACE)) return fds_dsl2_block(compiler);
	if (fds_dsl2_control_statement(compiler)) return true;
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_BREAK)) {
		if (!compiler->loop_depth || compiler->loop_break_count >= FDS_DSL_LOCAL_CAPACITY) {
			fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, "break used outside loop");
		} else {
			compiler->loop_breaks[compiler->loop_break_count++] = compiler->program->code_count;
			fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, 0);
		}
		return fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after break");
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_CONTINUE)) {
		if (!compiler->loop_depth) {
			fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, "continue used outside loop");
		} else if (!fds_dsl_emit(compiler->program, FDS_DSL_OP_JUMP, (uint16_t)compiler->loop_continue_targets[compiler->loop_depth - 1])) {
			fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "continue target is too large");
		}
		return fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after continue");
	}
	if (fds_dsl2_match(compiler, FDS_DSL_TOKEN_RETURN)) {
		size_t count = 0;
		do { if (count < FDS_DSL_RETURN_CAPACITY) fds_dsl2_expression(compiler); else fds_dsl2_error(compiler, FDS_DSL_COMPILE_LIMIT, "too many return values"); count++; } while (fds_dsl2_match(compiler, FDS_DSL_TOKEN_COMMA));
		fds_dsl_emit(compiler->program, FDS_DSL_OP_RETURN_VALUES, (uint16_t)count);
		compiler->has_return = true;
		if (!compiler->root && count != compiler->expected_return_count) fds_dsl2_error(compiler, FDS_DSL_COMPILE_TYPE, "wrong number of return values");
		return fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after return");
	}
	bool global = fds_dsl2_match(compiler, FDS_DSL_TOKEN_GLOBAL);
	if (!fds_dsl2_is_type(compiler->current) && compiler->current.kind == FDS_DSL_TOKEN_IDENTIFIER &&
		(global || fds_dsl2_peek_token(compiler).kind == FDS_DSL_TOKEN_EQUAL)) {
		fds_dsl_token name = compiler->current;
		fds_dsl2_advance(compiler);
		return fds_dsl2_inferred_assignment(compiler, name, global);
	}
	if (fds_dsl2_is_type(compiler->current)) {
		fds_dsl_type type = fds_dsl2_type(compiler->current);
		fds_dsl2_advance(compiler);
		fds_dsl_token name = compiler->current;
		if (!fds_dsl2_expect(compiler, FDS_DSL_TOKEN_IDENTIFIER, "expected variable name")) return false;
		return fds_dsl2_declaration(compiler, type, name, compiler->root || global);
	}
	if (global) { fds_dsl2_error(compiler, FDS_DSL_COMPILE_SYNTAX, "global requires a type and name"); return false; }
	fds_dsl2_expression(compiler);
	fds_dsl_emit(compiler->program, FDS_DSL_OP_POP, 0);
	return fds_dsl2_expect(compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after expression");
}

static inline bool fds_dsl2_block(fds_dsl2_compiler *compiler) {
	fds_dsl2_begin_scope(compiler);
	while (compiler->current.kind != FDS_DSL_TOKEN_RIGHT_BRACE && compiler->current.kind != FDS_DSL_TOKEN_EOF && compiler->diagnostic->error == FDS_DSL_COMPILE_OK) fds_dsl2_statement(compiler);
	bool result = fds_dsl2_expect(compiler, FDS_DSL_TOKEN_RIGHT_BRACE, "expected '}' after block");
	fds_dsl2_end_scope(compiler);
	return result;
}

static inline bool fds_dsl2_function(fds_dsl2_compiler *root, fds_dsl_type *return_types, size_t return_count, fds_dsl_token name) {
	if (root->program->function_count >= FDS_DSL_FUNCTION_CAPACITY || return_count > FDS_DSL_RETURN_CAPACITY) { fds_dsl2_error(root, FDS_DSL_COMPILE_LIMIT, "too many functions or return values"); return false; }
	size_t jump_index = root->program->code_count;
	if (!fds_dsl_emit(root->program, FDS_DSL_OP_JUMP, 0)) { fds_dsl2_error(root, FDS_DSL_COMPILE_LIMIT, "bytecode limit reached"); return false; }
	uint16_t function_index = (uint16_t)root->program->function_count++;
	fds_dsl_function *function = &root->program->functions[function_index];
	memset(function, 0, sizeof(*function));
	function->name = name.start;
	function->name_length = (uint16_t)name.length;
	function->code_start = root->program->code_count;
	function->return_count = (uint16_t)return_count;
	for (size_t index = 0; index < return_count; index++) function->return_types[index] = return_types[index];
	if (!fds_dsl2_expect(root, FDS_DSL_TOKEN_LEFT_PAREN, "expected '(' after function name")) return false;
	fds_dsl2_compiler compiler;
	memset(&compiler, 0, sizeof(compiler));
	compiler.program = root->program;
	compiler.lexer = root->lexer;
	compiler.current = root->current;
	compiler.diagnostic = root->diagnostic;
	compiler.function_index = function_index;
	compiler.expected_return_count = (uint16_t)return_count;
	if (compiler.current.kind != FDS_DSL_TOKEN_RIGHT_PAREN) {
		do {
			fds_dsl_token type_token = compiler.current;
			if (!fds_dsl2_is_type(type_token)) { fds_dsl2_error(&compiler, FDS_DSL_COMPILE_SYNTAX, "expected parameter type"); break; }
			fds_dsl_type type = fds_dsl2_type(type_token);
			fds_dsl2_advance(&compiler);
			fds_dsl_token parameter = compiler.current;
			fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_IDENTIFIER, "expected parameter name");
			if (compiler.variable_count >= FDS_DSL_LOCAL_CAPACITY) { fds_dsl2_error(&compiler, FDS_DSL_COMPILE_LIMIT, "too many parameters"); break; }
			compiler.variables[compiler.variable_count] = (fds_dsl2_variable){ parameter.start, parameter.length, type, (uint16_t)compiler.variable_count, false };
			function->parameter_types[function->parameter_count++] = type;
			compiler.variable_count++;
			if (compiler.variable_count > compiler.maximum_local_count) compiler.maximum_local_count = compiler.variable_count;
		} while (fds_dsl2_match(&compiler, FDS_DSL_TOKEN_COMMA));
	}
	fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
	if (!fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_LEFT_BRACE, "expected '{' before function body")) return false;
	fds_dsl2_block(&compiler);
	if (!compiler.has_return) {
		for (size_t index = 0; index < function->return_count; index++) fds_dsl_emit(root->program, FDS_DSL_OP_NIL, 0);
		fds_dsl_emit(root->program, FDS_DSL_OP_RETURN_VALUES, function->return_count);
	}
	function->local_count = (uint16_t)compiler.maximum_local_count;
	function->code_end = root->program->code_count;
	root->program->code[jump_index].operand = (uint16_t)function->code_end;
	root->lexer = compiler.lexer;
	root->current = compiler.current;
	return true;
}

static inline fds_dsl_compile_error fds_dsl_compile_ex(const char *source,
									 fds_dsl_program *program,
									 fds_dsl_compile_diagnostic *diagnostic,
									 fds_dsl_import_fn import_function,
									 void *import_user_data) {
	fds_dsl_compile_diagnostic local_diagnostic = { FDS_DSL_COMPILE_OK, NULL, 0, 0 };
	fds_dsl2_compiler compiler = { 0 };
	fds_dsl_native natives[FDS_DSL_NATIVE_CAPACITY];
	fds_dsl_import imports[FDS_DSL_IMPORT_CAPACITY];
	size_t native_count;
	size_t import_count;
	if (!source || !program) return FDS_DSL_COMPILE_LEXICAL;
	native_count = program->native_count;
	if (native_count > FDS_DSL_NATIVE_CAPACITY) native_count = FDS_DSL_NATIVE_CAPACITY;
	memcpy(natives, program->natives, native_count * sizeof(natives[0]));
	import_count = program->import_count;
	if (import_count > FDS_DSL_IMPORT_CAPACITY) import_count = FDS_DSL_IMPORT_CAPACITY;
	memcpy(imports, program->imports, import_count * sizeof(imports[0]));
	fds_dsl_program_init(program);
	memcpy(program->natives, natives, native_count * sizeof(natives[0]));
	program->native_count = native_count;
	memcpy(program->imports, imports, import_count * sizeof(imports[0]));
	program->import_count = import_count;
	compiler.program = program;
	compiler.diagnostic = diagnostic ? diagnostic : &local_diagnostic;
	*compiler.diagnostic = local_diagnostic;
	compiler.root = true;
	compiler.function_index = FDS_DSL_INVALID_INDEX;
	compiler.import_function = import_function;
	compiler.import_user_data = import_user_data;
	fds_dsl_lexer_init(&compiler.lexer, source);
	compiler.current = fds_dsl_lexer_next(&compiler.lexer);
	while (compiler.current.kind != FDS_DSL_TOKEN_EOF && compiler.diagnostic->error == FDS_DSL_COMPILE_OK) {
		if (fds_dsl2_match(&compiler, FDS_DSL_TOKEN_IMPORT)) {
			fds_dsl_token module = compiler.current;
			if (!fds_dsl2_match(&compiler, FDS_DSL_TOKEN_STRING)) {
				fds_dsl2_error(&compiler, FDS_DSL_COMPILE_SYNTAX, "expected module string after import");
				break;
			}
			if (fds_dsl_program_has_import(program, module.start, module.length)) {
				if (!fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after import")) break;
				continue;
			}
			bool imported = module.length == 3 && memcmp(module.start, "std", 3) == 0;
			if (compiler.import_function) imported = compiler.import_function(program, module.start, module.length, compiler.import_user_data);
			if (!imported || !fds_dsl_program_add_import(program, module.start, module.length)) {
				fds_dsl2_error(&compiler, FDS_DSL_COMPILE_NAME, "module import failed");
				break;
			}
			if (!fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_SEMICOLON, "expected ';' after import")) break;
			continue;
		}
		if (fds_dsl2_match(&compiler, FDS_DSL_TOKEN_FN)) {
			fds_dsl_type return_types[FDS_DSL_RETURN_CAPACITY]; size_t return_count = 0;
			do {
				if (!fds_dsl2_is_type(compiler.current) || return_count >= FDS_DSL_RETURN_CAPACITY) { fds_dsl2_error(&compiler, FDS_DSL_COMPILE_SYNTAX, "expected function return type"); break; }
				return_types[return_count++] = fds_dsl2_type(compiler.current); fds_dsl2_advance(&compiler);
			} while (fds_dsl2_match(&compiler, FDS_DSL_TOKEN_COMMA));
			fds_dsl_token name = compiler.current; fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_IDENTIFIER, "expected function name");
			fds_dsl2_function(&compiler, return_types, return_count, name);
			continue;
		}
		if (fds_dsl2_is_type(compiler.current)) {
			fds_dsl_token type_token = compiler.current;
			fds_dsl2_advance(&compiler);
			fds_dsl_type first_type = fds_dsl2_type(type_token);
			if (compiler.current.kind == FDS_DSL_TOKEN_COMMA || (compiler.current.kind == FDS_DSL_TOKEN_IDENTIFIER && fds_dsl2_peek_token(&compiler).kind == FDS_DSL_TOKEN_LEFT_PAREN)) {
				fds_dsl_type returns[FDS_DSL_RETURN_CAPACITY]; size_t return_count = 1; returns[0] = first_type;
				while (fds_dsl2_match(&compiler, FDS_DSL_TOKEN_COMMA)) { if (!fds_dsl2_is_type(compiler.current) || return_count >= FDS_DSL_RETURN_CAPACITY) { fds_dsl2_error(&compiler, FDS_DSL_COMPILE_SYNTAX, "expected return type"); break; } returns[return_count++] = fds_dsl2_type(compiler.current); fds_dsl2_advance(&compiler); }
				fds_dsl_token name = compiler.current; fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_IDENTIFIER, "expected function name");
				fds_dsl2_function(&compiler, returns, return_count, name);
			} else {
				fds_dsl_token name = compiler.current; fds_dsl2_expect(&compiler, FDS_DSL_TOKEN_IDENTIFIER, "expected variable name");
				fds_dsl2_declaration(&compiler, first_type, name, true);
			}
			continue;
		}
		fds_dsl2_statement(&compiler);
	}
	if (compiler.diagnostic->error == FDS_DSL_COMPILE_OK) {
		fds_dsl_emit(program, FDS_DSL_OP_NIL, 0);
		fds_dsl_emit(program, FDS_DSL_OP_HALT, 0);
	}
	return compiler.diagnostic->error;
}

static inline fds_dsl_compile_error fds_dsl_compile(const char *source,
									 fds_dsl_program *program,
									 fds_dsl_compile_diagnostic *diagnostic) {
	return fds_dsl_compile_ex(source, program, diagnostic, NULL, NULL);
}

static inline fds_dsl_error fds_dsl_vm_run_values(fds_dsl_vm *vm,
									 fds_dsl_value *values,
									 size_t capacity,
									 size_t *count) {
	fds_dsl_value last;
	fds_dsl_error error;
	if (!vm || !values || !count) return FDS_DSL_ERROR_BAD_ARGUMENT;
	error = fds_dsl_vm_run(vm, &last);
	if (error != FDS_DSL_OK) return error;
	if (vm->stack_count + 1 > capacity) return FDS_DSL_ERROR_OVERFLOW;
	for (size_t index = 0; index < vm->stack_count; index++) values[index] = vm->stack[index];
	values[vm->stack_count] = last;
	*count = vm->stack_count + 1;
	return FDS_DSL_OK;
}

typedef struct fds_dsl_stdlib {
	fds_dsl_output_fn output;
	void *user_data;
} fds_dsl_stdlib;

static inline bool fds_dsl_stdlib_print(struct fds_dsl_vm *vm,
									const fds_dsl_value *args,
									size_t argument_count,
									fds_dsl_value *result,
									void *user_data) {
	fds_dsl_stdlib *stdlib = (fds_dsl_stdlib *)user_data;
	char buffer[64];
	int length;
	(void)vm;
	if (argument_count != 1 || !stdlib || !stdlib->output) return false;
	if (args[0].type == FDS_DSL_TYPE_STRING) {
		stdlib->output(args[0].as.string.data, args[0].as.string.length, stdlib->user_data);
	} else if (args[0].type == FDS_DSL_TYPE_INT) {
		length = snprintf(buffer, sizeof(buffer), "%lld", (long long)args[0].as.integer);
		stdlib->output(buffer, (size_t)length, stdlib->user_data);
	} else if (args[0].type == FDS_DSL_TYPE_FLOAT) {
		length = snprintf(buffer, sizeof(buffer), "%g", args[0].as.floating);
		stdlib->output(buffer, (size_t)length, stdlib->user_data);
	} else if (args[0].type == FDS_DSL_TYPE_BOOL) {
		const char *text = args[0].as.boolean ? "true" : "false";
		stdlib->output(text, strlen(text), stdlib->user_data);
	} else {
		const char *text = "nil";
		stdlib->output(text, strlen(text), stdlib->user_data);
	}
	*result = fds_dsl_nil();
	return true;
}

static inline bool fds_dsl_stdlib_length(struct fds_dsl_vm *vm,
									 const fds_dsl_value *args,
									 size_t argument_count,
									 fds_dsl_value *result,
									 void *user_data) {
	(void)vm;
	(void)user_data;
	if (argument_count != 1 || args[0].type != FDS_DSL_TYPE_STRING) return false;
	*result = fds_dsl_int((int64_t)args[0].as.string.length);
	return true;
}

static inline bool fds_dsl_stdlib_to_int(struct fds_dsl_vm *vm,
									 const fds_dsl_value *args,
									 size_t argument_count,
									 fds_dsl_value *result,
									 void *user_data) {
	int64_t value = 0;
	size_t index = 0;
	bool negative = false;
	(void)vm;
	(void)user_data;
	if (argument_count != 1 || args[0].type != FDS_DSL_TYPE_STRING || !args[0].as.string.data) return false;
	if (args[0].as.string.length && args[0].as.string.data[0] == '-') { negative = true; index = 1; }
	if (index == args[0].as.string.length) return false;
	for (; index < args[0].as.string.length; index++) {
		char character = args[0].as.string.data[index];
		if (!fds_dsl_ascii_digit(character)) return false;
		value = value * 10 + (character - '0');
	}
	*result = fds_dsl_int(negative ? -value : value);
	return true;
}

static inline bool fds_dsl_stdlib_register(fds_dsl_program *program, fds_dsl_stdlib *stdlib) {
	uint16_t print_index;
	uint16_t length_index;
	uint16_t to_int_index;
	if (!program || !stdlib || !stdlib->output) return false;
	print_index = fds_dsl_add_native_typed(program, "print", FDS_DSL_TYPE_NIL, fds_dsl_stdlib_print, stdlib);
	length_index = fds_dsl_add_native_typed(program, "length", FDS_DSL_TYPE_INT, fds_dsl_stdlib_length, stdlib);
	to_int_index = fds_dsl_add_native_typed(program, "to_int", FDS_DSL_TYPE_INT, fds_dsl_stdlib_to_int, stdlib);
	return print_index != FDS_DSL_INVALID_INDEX && length_index != FDS_DSL_INVALID_INDEX && to_int_index != FDS_DSL_INVALID_INDEX;
}

typedef struct fds_dsl_state {
	fds_dsl_program program;
	fds_dsl_vm vm;
	fds_dsl_heap heap;
	fds_dsl_stdlib stdlib;
	fds_dsl_import_fn import_function;
	void *import_user_data;
	bool compiled;
} fds_dsl_state;

static inline bool fds_dsl_state_init(fds_dsl_state *state,
								 fds_dsl_output_fn output,
								 void *output_user_data);

static inline fds_dsl_state *fds_dsl_state_new(fds_dsl_output_fn output,
										 void *output_user_data) {
	fds_dsl_state *state = (fds_dsl_state *)malloc(sizeof(*state));
	if (!state || !fds_dsl_state_init(state, output, output_user_data)) {
		free(state);
		return NULL;
	}
	return state;
}

static inline void fds_dsl_state_delete(fds_dsl_state *state) {
	if (!state) return;
	fds_dsl_heap_deinit(&state->heap);
	free(state);
}

static inline void fds_dsl_no_output(const char *text, size_t length, void *user_data) {
	(void)text;
	(void)length;
	(void)user_data;
}

static inline bool fds_dsl_state_import(fds_dsl_program *program,
									const char *module_name,
									size_t module_length,
									void *user_data) {
	fds_dsl_state *state = (fds_dsl_state *)user_data;
	if (module_length == 3 && memcmp(module_name, "std", 3) == 0) return true;
	return state && state->import_function && state->import_function(program, module_name, module_length, state->import_user_data);
}

static inline bool fds_dsl_state_init(fds_dsl_state *state,
								 fds_dsl_output_fn output,
								 void *output_user_data) {
	if (!state) return false;
	memset(state, 0, sizeof(*state));
	fds_dsl_program_init(&state->program);
	fds_dsl_heap_init(&state->heap);
	state->stdlib.output = output ? output : fds_dsl_no_output;
	state->stdlib.user_data = output_user_data;
	return fds_dsl_stdlib_register(&state->program, &state->stdlib);
}

static inline void fds_dsl_state_set_importer(fds_dsl_state *state,
									 fds_dsl_import_fn import_function,
									 void *user_data) {
	if (!state) return;
	state->import_function = import_function;
	state->import_user_data = user_data;
}

static inline uint16_t fds_dsl_state_add_native(fds_dsl_state *state,
									 const char *name,
									 fds_dsl_type result_type,
									 fds_dsl_native_fn function,
									 void *user_data) {
	if (!state) return FDS_DSL_INVALID_INDEX;
	return fds_dsl_add_native_typed(&state->program, name, result_type, function, user_data);
}

static inline fds_dsl_compile_error fds_dsl_state_compile(fds_dsl_state *state,
										 const char *source,
										 fds_dsl_compile_diagnostic *diagnostic) {
	fds_dsl_compile_error error;
	if (!state) return FDS_DSL_COMPILE_LEXICAL;
	error = fds_dsl_compile_ex(source, &state->program, diagnostic, fds_dsl_state_import, state);
	state->compiled = error == FDS_DSL_COMPILE_OK;
	return error;
}

static inline fds_dsl_error fds_dsl_state_run(fds_dsl_state *state, fds_dsl_value *result) {
	if (!state || !state->compiled) return FDS_DSL_ERROR_BAD_ARGUMENT;
	fds_dsl_vm_init_with_heap(&state->vm, &state->program, &state->heap);
	return fds_dsl_vm_run(&state->vm, result);
}

#endif
