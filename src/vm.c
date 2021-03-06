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

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
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

/* REL for relational operator */
#define REL_u8(vm, op) \
	uint8_t b = POP((vm)); \
	uint8_t a = POP((vm)); \
	uint8_t ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_i8(vm, op) \
	int8_t b = POP((vm)); \
	int8_t a = POP((vm)); \
	uint8_t ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_u16(vm, op) \
	uint16_t a, b; \
	uint16_t buf; \
	uint8_t ret; \
\
	POP_16((vm), b, buf); \
	POP_16((vm), a, buf); \
	ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_i16(vm, op) \
	int16_t a, b; \
	uint16_t buf; \
	uint8_t ret; \
\
	POP_16((vm), b, buf); \
	POP_16((vm), a, buf); \
	ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_u32(vm, op) \
	uint32_t a, b; \
	uint32_t buf; \
	uint8_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
	ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_i32(vm, op) \
	int32_t a, b; \
	uint32_t buf; \
	uint8_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
	ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_u64(vm, op) \
	uint64_t a, b; \
	uint64_t buf; \
	uint8_t ret; \
\
	POP_64((vm), b, buf); \
	POP_64((vm), a, buf); \
	ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_i64(vm, op) \
	int64_t a, b; \
	uint64_t buf; \
	uint8_t ret; \
\
	POP_64((vm), b, buf); \
	POP_64((vm), a, buf); \
	ret = a op b ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_f(vm, op) \
	uint32_t a, b, buf; \
	float aa, bb; \
	uint8_t ret; \
\
	POP_32((vm), b, buf); \
	POP_32((vm), a, buf); \
\
	aa = deserialize_float(a); \
	bb = deserialize_float(b); \
	ret = aa op bb ? 1 : 0; \
\
	PUSH((vm), ret)

#define REL_d(vm, op) \
	uint64_t a, b, buf; \
	double aa, bb; \
	uint8_t ret; \
\
	POP_64((vm), b, buf); \
	POP_64((vm), a, buf); \
\
	aa = deserialize_double(a); \
	bb = deserialize_double(b); \
	ret = aa op bb ? 1 : 0; \
\
	PUSH((vm), ret)

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
		} else if (opcode == MOD_u8) {
			BINARY_u8(vm, %);
		} else if (opcode == MOD_i8) {
			BINARY_i8(vm, %);
		} else if (opcode == MOD_u16) {
			BINARY_u16(vm, %);
		} else if (opcode == MOD_i16) {
			BINARY_i16(vm, %);
		} else if (opcode == MOD_u32) {
			BINARY_u32(vm, %);
		} else if (opcode == MOD_i32) {
			BINARY_i32(vm, %);
		} else if (opcode == MOD_u64) {
			BINARY_u64(vm, %);
		} else if (opcode == MOD_i64) {
			BINARY_i64(vm, %);
		} else if (opcode == EQ_u8) {
			REL_u8(vm, ==);
		} else if (opcode == EQ_u16) {
			REL_u16(vm, ==);
		} else if (opcode == EQ_u32) {
			REL_u32(vm, ==);
		} else if (opcode == EQ_u64) {
			REL_u64(vm, ==);
		} else if (opcode == EQ_f) {
			REL_f(vm, ==);
		} else if (opcode == EQ_d) {
			REL_d(vm, ==);
		} else if (opcode == NEQ_u8) {
			REL_u8(vm, !=);
		} else if (opcode == NEQ_u16) {
			REL_u16(vm, !=);
		} else if (opcode == NEQ_u32) {
			REL_u32(vm, !=);
		} else if (opcode == NEQ_u64) {
			REL_u64(vm, !=);
		} else if (opcode == NEQ_f) {
			REL_f(vm, !=);
		} else if (opcode == NEQ_d) {
			REL_d(vm, !=);
		} else if (opcode == LT_u8) {
			REL_u8(vm, <);
		} else if (opcode == LT_i8) {
			REL_i8(vm, <);
		} else if (opcode == LT_u16) {
			REL_u16(vm, <);
		} else if (opcode == LT_i16) {
			REL_i16(vm, <);
		} else if (opcode == LT_u32) {
			REL_u32(vm, <);
		} else if (opcode == LT_i32) {
			REL_i32(vm, <);
		} else if (opcode == LT_u64) {
			REL_u64(vm, <);
		} else if (opcode == LT_i64) {
			REL_i64(vm, <);
		} else if (opcode == LT_f) {
			REL_f(vm, <);
		} else if (opcode == LT_d) {
			REL_d(vm, <);
		} else if (opcode == LTEQ_u8) {
			REL_u8(vm, <=);
		} else if (opcode == LTEQ_i8) {
			REL_i8(vm, <=);
		} else if (opcode == LTEQ_u16) {
			REL_u16(vm, <=);
		} else if (opcode == LTEQ_i16) {
			REL_i16(vm, <=);
		} else if (opcode == LTEQ_u32) {
			REL_u32(vm, <=);
		} else if (opcode == LTEQ_i32) {
			REL_i32(vm, <=);
		} else if (opcode == LTEQ_u64) {
			REL_u64(vm, <=);
		} else if (opcode == LTEQ_i64) {
			REL_i64(vm, <=);
		} else if (opcode == LTEQ_f) {
			REL_f(vm, <=);
		} else if (opcode == LTEQ_d) {
			REL_d(vm, <=);
		} else if (opcode == GT_u8) {
			REL_u8(vm, >);
		} else if (opcode == GT_i8) {
			REL_i8(vm, >);
		} else if (opcode == GT_u16) {
			REL_u16(vm, >);
		} else if (opcode == GT_i16) {
			REL_i16(vm, >);
		} else if (opcode == GT_u32) {
			REL_u32(vm, >);
		} else if (opcode == GT_i32) {
			REL_i32(vm, >);
		} else if (opcode == GT_u64) {
			REL_u64(vm, >);
		} else if (opcode == GT_i64) {
			REL_i64(vm, >);
		} else if (opcode == GT_f) {
			REL_f(vm, >);
		} else if (opcode == GT_d) {
			REL_d(vm, >);
		} else if (opcode == GTEQ_u8) {
			REL_u8(vm, >=);
		} else if (opcode == GTEQ_i8) {
			REL_i8(vm, >=);
		} else if (opcode == GTEQ_u16) {
			REL_u16(vm, >=);
		} else if (opcode == GTEQ_i16) {
			REL_i16(vm, >=);
		} else if (opcode == GTEQ_u32) {
			REL_u32(vm, >=);
		} else if (opcode == GTEQ_i32) {
			REL_i32(vm, >=);
		} else if (opcode == GTEQ_u64) {
			REL_u64(vm, >=);
		} else if (opcode == GTEQ_i64) {
			REL_i64(vm, >=);
		} else if (opcode == GTEQ_f) {
			REL_f(vm, >=);
		} else if (opcode == GTEQ_d) {
			REL_d(vm, >=);
		} else if (opcode == AND) {
			REL_u8(vm, &&);
		} else if (opcode == OR) {
			REL_u8(vm, ||);
		} else if (opcode == XOR) {
			uint8_t b = POP(vm);
			uint8_t a = POP(vm);
			uint8_t ret = (a || b) && !(a && b) ? 1 : 0;

			PUSH(vm, ret);
		} else if (opcode == NOT) {
			uint8_t a = POP(vm);
			uint8_t ret = a ? 0 : 1;

			PUSH(vm, ret);
		} else if (opcode == AND_u8) {
			BINARY_u8(vm, &);
		} else if (opcode == AND_u16) {
			BINARY_u16(vm, &);
		} else if (opcode == AND_u32) {
			BINARY_u32(vm, &);
		} else if (opcode == AND_u64) {
			BINARY_u64(vm, &);
		} else if (opcode == OR_u8) {
			BINARY_u8(vm, |);
		} else if (opcode == OR_u16) {
			BINARY_u16(vm, |);
		} else if (opcode == OR_u32) {
			BINARY_u32(vm, |);
		} else if (opcode == OR_u64) {
			BINARY_u64(vm, |);
		} else if (opcode == XOR_u8) {
			BINARY_u8(vm, ^);
		} else if (opcode == XOR_u16) {
			BINARY_u16(vm, ^);
		} else if (opcode == XOR_u32) {
			BINARY_u32(vm, ^);
		} else if (opcode == XOR_u64) {
			BINARY_u64(vm, ^);
		} else if (opcode == NOT_u8) {
			uint8_t a = POP(vm);
			uint8_t ret = ~a;

			PUSH(vm, ret);
		} else if (opcode == NOT_u16) {
			uint16_t a, buf, ret;

			POP_16(vm, a, buf);
			ret = ~a;

			PUSH_16(vm, ret);
		} else if (opcode == NOT_u32) {
			uint32_t a, buf, ret;

			POP_32(vm, a, buf);
			ret = ~a;

			PUSH_32(vm, ret);
		} else if (opcode == NOT_u64) {
			uint64_t a, buf, ret;

			POP_64(vm, a, buf);
			ret = ~a;

			PUSH_64(vm, ret);
		} else if (opcode == LSHFT_u8) {
			uint8_t b = POP(vm);
			uint8_t a = POP(vm);
			uint8_t ret = a << b;

			PUSH(vm, ret);
		} else if (opcode == LSHFT_u16) {
			uint8_t b = POP(vm);
			uint16_t a = POP(vm);
			uint16_t ret = a << b;

			PUSH_16(vm, ret);
		} else if (opcode == LSHFT_u32) {
			uint8_t b = POP(vm);
			uint32_t a = POP(vm);
			uint32_t ret = a << b;

			PUSH_32(vm, ret);
		} else if (opcode == LSHFT_u64) {
			uint8_t b = POP(vm);
			uint64_t a = POP(vm);
			uint64_t ret = a << b;

			PUSH_64(vm, ret);
		} else if (opcode == RSHFT_u8) {
			uint8_t b = POP(vm);
			uint8_t a = POP(vm);
			uint8_t ret = a >> b;

			PUSH(vm, ret);
		} else if (opcode == RSHFT_u16) {
			uint8_t b = POP(vm);
			uint16_t a = POP(vm);
			uint16_t ret = a >> b;

			PUSH_16(vm, ret);
		} else if (opcode == RSHFT_u32) {
			uint8_t b = POP(vm);
			uint32_t a = POP(vm);
			uint32_t ret = a >> b;

			PUSH_32(vm, ret);
		} else if (opcode == RSHFT_u64) {
			uint8_t b = POP(vm);
			uint64_t a = POP(vm);
			uint64_t ret = a >> b;

			PUSH_64(vm, ret);
		} else if (opcode == JMP_u8) {
			uint8_t a = POP(vm);
			vm->pc = a;
		} else if (opcode == JMP_u16) {
			uint16_t a, buf;
			POP_16(vm, a, buf);
			vm->pc = a;
		} else if (opcode == JMP_u32) {
			uint32_t a, buf;
			POP_32(vm, a, buf);
			vm->pc = a;
		} else if (opcode == JMP_u64) {
			uint64_t a, buf;
			POP_64(vm, a, buf);
			vm->pc = a;
		} else if (opcode == JMPIF_u8) {
			uint8_t b = POP(vm);
			uint8_t a = POP(vm);
			if (a) vm->pc = b;
		} else if (opcode == JMPIF_u16) {
			uint8_t a;
			uint16_t b, buf;

			POP_16(vm, b, buf);
			a = POP(vm);

			if (a) vm->pc = b;
		} else if (opcode == JMPIF_u32) {
			uint8_t a;
			uint32_t b, buf;

			POP_32(vm, b, buf);
			a = POP(vm);

			if (a) vm->pc = b;
		} else if (opcode == JMPIF_u64) {
			uint8_t a;
			uint64_t b, buf;

			POP_64(vm, b, buf);
			a = POP(vm);

			if (a) vm->pc = b;
		} else if (opcode == PUSH_u8) {
			PUSH(vm, GETCODE(vm));
		} else if (opcode == PUSH_u16) {
			size_t i;
			for (i=0; i<2; ++i) PUSH(vm, GETCODE(vm));
		} else if (opcode == PUSH_u32) {
			size_t i;
			for (i=0; i<4; ++i) PUSH(vm, GETCODE(vm));
		} else if (opcode == PUSH_u64) {
			size_t i;
			for (i=0; i<8; ++i) PUSH(vm, GETCODE(vm));
		} else if (opcode == POP_u8) {
			--vm->sp;
		} else if (opcode == POP_u16) {
			vm->sp -= 2;
		} else if (opcode == POP_u32) {
			vm->sp -= 4;
		} else if (opcode == POP_u64) {
			vm->sp -= 8;
		} else if (opcode == LOAD_u8) {
			uint8_t a = POP(vm);
			PUSH(vm, vm->env[a]);
		} else if (opcode == LOAD_u16) {
			uint16_t a, buf;
			POP_16(vm, a, buf);
			PUSH(vm, vm->env[a]);
		} else if (opcode == LOAD_u32) {
			uint32_t a, buf;
			POP_32(vm, a, buf);
			PUSH(vm, vm->env[a]);
		} else if (opcode == LOAD_u64) {
			uint64_t a, buf;
			POP_64(vm, a, buf);
			PUSH(vm, vm->env[a]);
		} else if (opcode == STORE_u8) {
			uint8_t addr = POP(vm);
			uint8_t val = POP(vm);

			vm->env[addr] = val;
		} else if (opcode == STORE_u16) {
			uint8_t val;
			uint16_t addr, buf;

			POP_16(vm, addr, buf);
			val = POP(vm);

			vm->env[addr] = val;
		} else if (opcode == STORE_u32) {
			uint8_t val;
			uint32_t addr, buf;

			POP_32(vm, addr, buf);
			val = POP(vm);

			vm->env[addr] = val;
		} else if (opcode == STORE_u64) {
			uint8_t val;
			uint64_t addr, buf;

			POP_64(vm, addr, buf);
			val = POP(vm);

			vm->env[addr] = val;
		} else if (opcode == CALL_u8) {
			/* we expect a uint8_t as the first arg: argc */
			uint8_t addr = POP(vm);

			PUSH_64(vm, vm->pc);
			PUSH_64(vm, vm->fp);

			vm->fp = vm->sp;
			vm->pc = addr;
		} else if (opcode == CALL_u16) {
			uint16_t addr, buf;
			POP_16(vm, addr, buf);

			PUSH_64(vm, vm->pc);
			PUSH_64(vm, vm->fp);

			vm->fp = vm->sp;
			vm->pc = addr;
		} else if (opcode == CALL_u32) {
			uint32_t addr, buf;
			POP_32(vm, addr, buf);

			PUSH_64(vm, vm->pc);
			PUSH_64(vm, vm->fp);

			vm->fp = vm->sp;
			vm->pc = addr;
		} else if (opcode == CALL_u64) {
			uint64_t addr, buf;
			POP_64(vm, addr, buf);

			PUSH_64(vm, vm->pc);
			PUSH_64(vm, vm->fp);

			vm->fp = vm->sp;
			vm->pc = addr;
		} else if (opcode == RET_u8) {
			uint8_t val, argc;
			uint64_t buf;

			val = POP(vm);

			vm->sp = vm->fp;
			POP_64(vm, vm->fp, buf);
			POP_64(vm, vm->pc, buf);

			argc = POP(vm);
			vm->sp -= argc;

			PUSH(vm, val);
		} else if (opcode == RET_u16) {
			uint8_t argc;
			uint16_t val;
			uint64_t buf;

			POP_16(vm, val, buf);

			vm->sp = vm->fp;
			POP_64(vm, vm->fp, buf);
			POP_64(vm, vm->pc, buf);

			argc = POP(vm);
			vm->sp -= argc;

			PUSH(vm, val);
		} else if (opcode == RET_u32) {
			uint8_t argc;
			uint32_t val;
			uint64_t buf;

			POP_32(vm, val, buf);

			vm->sp = vm->fp;
			POP_64(vm, vm->fp, buf);
			POP_64(vm, vm->pc, buf);

			argc = POP(vm);
			vm->sp -= argc;

			PUSH(vm, val);
		} else if (opcode == RET_u64) {
			uint8_t argc;
			uint64_t val;
			uint64_t buf;

			POP_64(vm, val, buf);

			vm->sp = vm->fp;
			POP_64(vm, vm->fp, buf);
			POP_64(vm, vm->pc, buf);

			argc = POP(vm);
			vm->sp -= argc;

			PUSH(vm, val);
		} else if (opcode == ARGC) {
			uint8_t argc = vm->fp - 8*2 - 1;
			PUSH(vm, argc);
		} else if (opcode == ARG) {
			uint8_t arg_num = POP(vm);
			uint8_t arg = vm->stack[vm->fp - 8*2 - 1 - arg_num];
			PUSH(vm, arg);
		} else if (opcode == HALT) {
			return 0;
		} else if (opcode == SYSCALL) {
			uint64_t syscall_num, ret, buf;
			uint64_t args[5];
			size_t i;

			uint8_t argc = POP(vm);

			switch (argc) {
			case 0:
				syscall_num = POP(vm);

				ret = syscall(syscall_num);
				PUSH_64(vm, ret);
				break;
			case 1:
				POP_64(vm, args[0], buf);
				syscall_num = POP(vm);

				ret = syscall(syscall_num, args[0]);
				PUSH_64(vm, ret);
				break;
			case 2:
				for (i=0;i<argc; ++i) POP_64(vm, args[i], buf);
				syscall_num = POP(vm);

				ret = syscall(syscall_num, args[0], args[1]);
				PUSH_64(vm, ret);
				break;
			case 3:
				for (i=0;i<argc; ++i) POP_64(vm, args[i], buf);
				syscall_num = POP(vm);

				ret = syscall(syscall_num, args[0], args[1],
					args[2]);
				PUSH_64(vm, ret);
				break;
			case 4:
				for (i=0;i<argc; ++i) POP_64(vm, args[i], buf);
				syscall_num = POP(vm);

				ret = syscall(syscall_num, args[0], args[1],
					args[2], args[3]);
				PUSH_64(vm, ret);
				break;
			case 5:
				for (i=0;i<argc; ++i) POP_64(vm, args[i], buf);
				syscall_num = POP(vm);

				ret = syscall(syscall_num, args[0], args[1],
					args[2], args[3], args[4]);
				PUSH_64(vm, ret);
				break;
			}
		}
	}
}
