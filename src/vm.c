#include <stdlib.h>
#include <string.h>
#include <vm.h>

#define PUSH(vm, v) (vm)->stack[(vm)->sp++] = (v) /* push v onto data stack */
#define POP(vm) (vm)->stack[--(vm)->sp] /* pop from data stack */
#define GETCODE(vm) (vm)->code[(vm)->pc++] /* get next opcode */

#define PUSH_16(vm, v) \
	PUSH((vm), ((v) >> 8) & 0xFF); \
	PUSH((vm), (v) & 0xFF)

#define PUSH_32(vm, v) \
	PUSH((vm), ((v) >> 24) & 0xFF); \
	PUSH((vm), ((v) >> 16) & 0xFF); \
	PUSH((vm), ((v) >> 8) & 0xFF); \
	PUSH((vm), (v) & 0xFF)

#define PUSH_64(vm, v) \
	PUSH((vm), ((v) >> 56) & 0xFF); \
	PUSH((vm), ((v) >> 48) & 0xFF); \
	PUSH((vm), ((v) >> 40) & 0xFF); \
	PUSH((vm), ((v) >> 32) & 0xFF); \
	PUSH((vm), ((v) >> 24) & 0xFF); \
	PUSH((vm), ((v) >> 16) & 0xFF); \
	PUSH((vm), ((v) >> 8) & 0xFF); \
	PUSH((vm), (v) & 0xFF)

#define POP_16(vm, v, buf) \
	(buf) = POP((vm)); \
	(v) = (buf); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 8)

#define POP_32(vm, v, buf) \
	(buf) = POP((vm)); \
	(v) = (buf); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 8); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 16); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 24)

#define POP_64(vm, v, buf) \
	(buf) = POP((vm)); \
	(v) = (buf); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 8); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 16); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 24); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 32); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 40); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 48); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 56)

static uint32_t serialize_float(float x)
{
	uint32_t ret, xx;
	uint8_t b;
	size_t i;
	size_t len = sizeof(float);

	memcpy(&xx, &x, len);

	ret = 0;
	for (i=0; i<len; ++i) {
		b = (xx >> (8*i)) & 0xFF;
		ret |= b << ((len - 1 - i) * 8);
	}

	return ret;
}

static float deserialize_float(uint32_t x)
{
	uint32_t xx;
	uint8_t b;
	float ret;
	size_t i;
	size_t len = sizeof(float);

	xx = 0;
	for (i=0; i<len; ++i) {
		b = (x >> 8*i) & 0xFF;
		xx |= b << ((len - 1 - i) * 8);
	}

	memcpy(&ret, &xx, len);

	return ret;
}

static uint64_t serialize_double(double x)
{
	uint64_t ret, xx;
	uint8_t b;
	size_t i;
	size_t len = sizeof(double);

	memcpy(&xx, &x, len);

	ret = 0;
	for (i=0; i<len; ++i) {
		b = (xx >> (8*i)) & 0xFF;
		ret |= b << ((len - 1 - i) * 8);
	}

	return ret;
}

static double deserialize_double(uint64_t x)
{
	uint64_t xx;
	uint8_t b;
	double ret;
	size_t i;
	size_t len = sizeof(double);

	xx = 0;
	for (i=0; i<len; ++i) {
		b = (x >> 8*i) & 0xFF;
		xx |= b << ((len - 1 - i) * 8);
	}

	memcpy(&ret, &xx, len);

	return ret;
}

struct VM {
	uint8_t *env; /* variable env */

	uint8_t *code; /* executable code */
	uint8_t *stack; /* data stack */

	size_t pc; /* program counter */
	size_t sp; /* stack pointer */
	size_t fp; /* frame pointer */
};

VM* make_vm(uint8_t *code, size_t stack_size, size_t env_size)
{
	VM *vm = malloc(sizeof(VM));
	if (vm == NULL) goto cleanup;

	vm->stack = malloc(stack_size * sizeof(uint8_t));
	if (vm->stack == NULL) goto cleanup;

	vm->env = malloc(env_size * sizeof(uint8_t));
	if (vm->env == NULL) goto cleanup;

	vm->code = code;
	vm->pc = 0;
	vm->fp = 0;
	vm->sp = 0;

	return vm;

cleanup:
	if (vm != NULL) {
		free(vm->stack);
		free(vm->env);
		free(vm);
	}

	return NULL;
}

void free_vm(VM *vm)
{
	free(vm->stack);
	free(vm->env);
	free(vm);
}

int run_vm(VM *vm, uint8_t *code, size_t pc)
{
	uint8_t opcode;

	vm->pc = pc;
	if (code != NULL) vm->code=code;

	while (1) {
		opcode = GETCODE(vm); /* get next instruction */

		if (opcode == ADD_u8) {
			uint8_t b = POP(vm);
			uint8_t a = POP(vm);
			uint16_t ret  = a + b;

			PUSH_16(vm, ret);
		} else if (opcode == ADD_i8) {
			int8_t b = POP(vm);
			int8_t a = POP(vm);
			int16_t ret = a + b;

			PUSH_16(vm, ret);
		} else if (opcode == ADD_u16) {
			uint16_t a, b;
			uint16_t buf;
			uint32_t ret;

			POP_16(vm, b, buf);
			POP_16(vm, a, buf);
			ret = a + b;

			PUSH_32(vm, ret);
		} else if (opcode == ADD_i16) {
			int16_t a, b;
			uint16_t buf;
			int32_t ret;

			POP_16(vm, b, buf);
			POP_16(vm, a, buf);
			ret = a + b;

			PUSH_32(vm, ret);
		} else if (opcode == ADD_u32) {
			uint32_t a, b;
			uint32_t buf;
			uint64_t ret;

			POP_32(vm, b, buf);
			POP_32(vm, a, buf);
			ret = a + b;

			PUSH_64(vm, ret);
		} else if (opcode == ADD_i32) {
			int32_t a, b;
			uint32_t buf;
			int64_t ret;

			POP_32(vm, b, buf);
			POP_32(vm, a, buf);
			ret = a + b;

			PUSH_64(vm, ret);
		} else if (opcode == ADD_u64) {
			uint64_t a, b, buf, ret;

			POP_64(vm, b, buf);
			POP_64(vm, a, buf);
			ret = a + b;

			PUSH_64(vm, ret);
		} else if (opcode == ADD_i64) {
			int64_t a, b, ret;
			uint64_t buf;

			POP_64(vm, b, buf);
			POP_64(vm, a, buf);
			ret = a + b;

			PUSH_64(vm, ret);
		}
	}
}
