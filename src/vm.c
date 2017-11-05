/******************************************************************************
 * Copyright (C) 2017 Sahil Kang <sahil.kang@asilaycomputing.com>
 *
 * This file is part of stacker-vm.
 *
 * stacker-vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * stacker-vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with stacker-vm.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
	PUSH_16((vm), (v))

#define PUSH_64(vm, v) \
	PUSH((vm), ((v) >> 56) & 0xFF); \
	PUSH((vm), ((v) >> 48) & 0xFF); \
	PUSH((vm), ((v) >> 40) & 0xFF); \
	PUSH((vm), ((v) >> 32) & 0xFF); \
	PUSH_32((vm), (v))

#define POP_16(vm, v, buf) \
	(buf) = POP((vm)); \
	(v) = (buf); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 8)

#define POP_32(vm, v, buf) \
	POP_16((vm), (v), (buf)); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 16); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 24)

#define POP_64(vm, v, buf) \
	POP_32((vm), (v), (buf)); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 32); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 40); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 48); \
	(buf) = POP((vm)); \
	(v) |= ((buf) << 56)

#define BINARY_u8(vm, op) \
	uint8_t b = POP((vm)); \
	uint8_t a = POP((vm)); \
	uint8_t ret  = a op b; \
\
	PUSH((vm), ret)

/* PROM for promote: we promote the u8 size to u16 on return */
#define BINARY_u8_PROM(vm, op) \
	uint8_t b = POP((vm)); \
	uint8_t a = POP((vm)); \
	uint16_t ret  = a op b; \
\
	PUSH_16((vm), ret)

#define BINARY_i8(vm, op) \
	int8_t b = POP((vm)); \
	int8_t a = POP((vm)); \
	int8_t ret = a op b; \
\
	PUSH((vm), ret)

#define BINARY_i8_PROM(vm, op) \
	int8_t b = POP((vm)); \
	int8_t a = POP((vm)); \
	int16_t ret = a op b; \
\
	PUSH_16((vm), ret)

#define BINARY_u16(vm, op) \
	uint16_t a, b; \
	uint16_t buf; \
	uint16_t ret; \
\
	POP_16((vm), b, buf); \
	POP_16((vm), a, buf); \
	ret = a op b; \
\
	PUSH_16((vm), ret)

#define BINARY_u16_PROM(vm, op) \
	uint16_t a, b; \
	uint16_t buf; \
	uint32_t ret; \
\
	POP_16((vm), b, buf); \
	POP_16((vm), a, buf); \
	ret = a op b; \
\
	PUSH_32((vm), ret)

#define BINARY_i16(vm, op) \
	int16_t a, b; \
	uint16_t buf; \
	int16_t ret; \
\
	POP_16((vm), b, buf); \
	POP_16((vm), a, buf); \
	ret = a op b; \
\
	PUSH_16((vm), ret)

#define BINARY_i16_PROM(vm, op) \
	int16_t a, b; \
	uint16_t buf; \
	int32_t ret; \
\
	POP_16((vm), b, buf); \
	POP_16((vm), a, buf); \
	ret = a op b; \
\
	PUSH_32((vm), ret)

#define BINARY_u32(vm, op) \
	uint32_t a, b; \
	uint32_t buf; \
	uint32_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
	ret = a op b; \
\
	PUSH_32((vm), ret)

#define BINARY_u32_PROM(vm, op) \
	uint32_t a, b; \
	uint32_t buf; \
	uint64_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
	ret = a op b; \
\
	PUSH_64((vm), ret)

#define BINARY_i32(vm, op) \
	int32_t a, b; \
	uint32_t buf; \
	int32_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
	ret = a op b; \
\
	PUSH_32((vm), ret)

#define BINARY_i32_PROM(vm, op) \
	int32_t a, b; \
	uint32_t buf; \
	int64_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
	ret = a op b; \
\
	PUSH_64((vm), ret)

#define BINARY_u64(vm, op) \
	uint64_t a, b, buf, ret; \
\
	POP_64((vm), b, buf); \
	POP_64((vm), a, buf); \
	ret = a op b; \
\
	PUSH_64((vm), ret)

#define BINARY_i64(vm, op) \
	int64_t a, b, ret; \
	uint64_t buf; \
\
	POP_64((vm), b, buf); \
	POP_64((vm), a, buf); \
	ret = a op b; \
\
	PUSH_64((vm), ret)

#define BINARY_f(vm, op) \
	uint32_t a, b, c, buf; \
	float aa, bb, cc; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
\
	aa = deserialize_float(a); \
	bb = deserialize_float(b); \
	cc = aa op bb; \
\
	c = serialize_float(cc); \
\
	PUSH_32((vm), c)

#define BINARY_d(vm, op) \
	uint64_t a, b, c, buf; \
	double aa, bb, cc; \
\
	POP_64((vm), b, buf); \
	POP_64((vm), a, buf); \
\
	aa = deserialize_double(a); \
	bb = deserialize_double(b); \
	cc = aa op bb; \
\
	c = serialize_double(cc); \
\
	PUSH_64((vm), c)

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
			BINARY_u8_PROM(vm, +);
		} else if (opcode == ADD_i8) {
			BINARY_i8_PROM(vm, +);
		} else if (opcode == ADD_u16) {
			BINARY_u16_PROM(vm, +);
		} else if (opcode == ADD_i16) {
			BINARY_i16_PROM(vm, +);
		} else if (opcode == ADD_u32) {
			BINARY_u32_PROM(vm, +);
		} else if (opcode == ADD_i32) {
			BINARY_i32_PROM(vm, +);
		} else if (opcode == ADD_u64) {
			BINARY_u64(vm, +);
		} else if (opcode == ADD_i64) {
			BINARY_i64(vm, +);
		} else if (opcode == ADD_f) {
			BINARY_f(vm, +);
		} else if (opcode == ADD_d) {
			BINARY_d(vm, +);
		} else if (opcode == SUB_u8) {
			BINARY_u8_PROM(vm, -);
		} else if (opcode == SUB_i8) {
			BINARY_i8_PROM(vm, -);
		} else if (opcode == SUB_u16) {
			BINARY_u16_PROM(vm, -);
		} else if (opcode == SUB_i16) {
			BINARY_i16_PROM(vm, -);
		} else if (opcode == SUB_u32) {
			BINARY_u32_PROM(vm, -);
		} else if (opcode == SUB_i32) {
			BINARY_i32_PROM(vm, -);
		} else if (opcode == SUB_u64) {
			BINARY_u64(vm, -);
		} else if (opcode == SUB_i64) {
			BINARY_i64(vm, -);
		} else if (opcode == SUB_f) {
			BINARY_f(vm, -);
		} else if (opcode == SUB_d) {
			BINARY_d(vm, -);
		} else if (opcode == MUL_u8) {
			BINARY_u8_PROM(vm, *);
		} else if (opcode == MUL_i8) {
			BINARY_i8_PROM(vm, *);
		} else if (opcode == MUL_u16) {
			BINARY_u16_PROM(vm, *);
		} else if (opcode == MUL_i16) {
			BINARY_i16_PROM(vm, *);
		} else if (opcode == MUL_u32) {
			BINARY_u32_PROM(vm, *);
		} else if (opcode == MUL_i32) {
			BINARY_i32_PROM(vm, *);
		} else if (opcode == MUL_u64) {
			BINARY_u64(vm, *);
		} else if (opcode == MUL_i64) {
			BINARY_i64(vm, *);
		} else if (opcode == MUL_f) {
			BINARY_f(vm, *);
		} else if (opcode == MUL_d) {
			BINARY_d(vm, *);
		} else if (opcode == DIV_u8) {
			BINARY_u8(vm, /);
		} else if (opcode == DIV_i8) {
			BINARY_i8(vm, /);
		} else if (opcode == DIV_u16) {
			BINARY_u16(vm, /);
		} else if (opcode == DIV_i16) {
			BINARY_i16(vm, /);
		} else if (opcode == DIV_u32) {
			BINARY_u32(vm, /);
		} else if (opcode == DIV_i32) {
			BINARY_i32(vm, /);
		} else if (opcode == DIV_u64) {
			BINARY_u64(vm, /);
		} else if (opcode == DIV_i64) {
			BINARY_i64(vm, /);
		} else if (opcode == DIV_f) {
			BINARY_f(vm, /);
		} else if (opcode == DIV_d) {
			BINARY_d(vm, /);
		}
	}
}
