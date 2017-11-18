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

#ifndef VM_HEADER
#define VM_HEADER

#include <stddef.h>
#include <stdint.h>

typedef struct VM VM;

VM* make_vm(uint8_t *code, size_t stack_size, size_t env_size);

void free_vm(VM *vm);

int run_vm(VM *vm, uint8_t *code, size_t pc);

enum opcode {
	ADD_u8 = 0x01, /* add uint8_t */
	ADD_i8 = 0x02, /* add int8_t */
	ADD_u16 = 0x03,
	ADD_i16 = 0x04,
	ADD_u32 = 0x05,
	ADD_i32 = 0x06,
	ADD_u64 = 0x07,
	ADD_i64 = 0x08,
	ADD_f = 0x09, /* add float */
	ADD_d = 0x0A, /* add double */

	SUB_u8 = 0x0B, /* subtract uint8_t */
	SUB_i8 = 0x0C,
	SUB_u16 = 0x0D,
	SUB_i16 = 0x0E,
	SUB_u32 = 0x0F,
	SUB_i32 = 0x10,
	SUB_u64 = 0x11,
	SUB_i64 = 0x12,
	SUB_f = 0x13, /* subtract float */
	SUB_d = 0x14, /* subtract double */

	MUL_u8 = 0x15, /* multiply uint8_t */
	MUL_i8 = 0x16, /* multiply int8_t */
	MUL_u16 = 0x17,
	MUL_i16 = 0x18,
	MUL_u32 = 0x19,
	MUL_i32 = 0x1A,
	MUL_u64 = 0x1B,
	MUL_i64 = 0x1C,
	MUL_f = 0x1D, /* multiply float */
	MUL_d = 0x1E, /* multiply double */

	DIV_u8 = 0x1F, /* divide uint8_t */
	DIV_i8 = 0x20,
	DIV_u16 = 0x21,
	DIV_i16 = 0x22,
	DIV_u32 = 0x23,
	DIV_i32 = 0x24,
	DIV_u64 = 0x25,
	DIV_i64 = 0x26,
	DIV_f = 0x27, /* divide float */
	DIV_d = 0x28, /* divide double */

	MOD_u8 = 0x29, /* modulo uint8_t */
	MOD_i8 = 0x2A, /* modulo int8_t */
	MOD_u16 = 0x2B,
	MOD_i16 = 0x2C,
	MOD_u32 = 0x2D,
	MOD_i32 = 0x2E,
	MOD_u64 = 0x2F,
	MOD_i64 = 0x30,

	EQ_u8 = 0x31,
	EQ_u16 = 0x33,
	EQ_u32 = 0x35,
	EQ_u64 = 0x37, /* equal-to uint64_t */
	EQ_f = 0x39, /* equal-to float */
	EQ_d = 0x3A, /* less than double */

	NEQ_u8 = 0x3B,
	NEQ_u16 = 0x3D,
	NEQ_u32 = 0x3F,
	NEQ_u64 = 0x41, /* not equal to uint64_t */
	NEQ_f = 0x43,
	NEQ_d = 0x44,

	LT_u8 = 0x45,
	LT_i8 = 0x46,
	LT_u16 = 0x47,
	LT_i16 = 0x48,
	LT_u32 = 0x49,
	LT_i32 = 0x4A,
	LT_u64 = 0x4B, /* less-than uint64_t */
	LT_i64 = 0x4C,
	LT_f = 0x4D,
	LT_d = 0x4E,

	LTEQ_u8 = 0x4F,
	LTEQ_i8 = 0x50,
	LTEQ_u16 = 0x51,
	LTEQ_i16 = 0x52,
	LTEQ_u32 = 0x53,
	LTEQ_i32 = 0x54,
	LTEQ_u64 = 0x55, /* less-than or equal-to uint64_t */
	LTEQ_i64 = 0x56,
	LTEQ_f = 0x57,
	LTEQ_d = 0x58,

	GT_u8 = 0x59,
	GT_i8 = 0x5A,
	GT_u16 = 0x5B,
	GT_i16 = 0x5C,
	GT_u32 = 0x5D,
	GT_i32 = 0x5E,
	GT_u64 = 0x5F, /* greater-than uint64_t */
	GT_i64 = 0x60,
	GT_f = 0x61,
	GT_d = 0x62,

	GTEQ_u8 = 0x63,
	GTEQ_i8 = 0x64,
	GTEQ_u16 = 0x65,
	GTEQ_i16 = 0x66,
	GTEQ_u32 = 0x67,
	GTEQ_i32 = 0x68,
	GTEQ_u64 = 0x69, /* greater-than or equal-to uint64_t */
	GTEQ_i64 = 0x6A,
	GTEQ_f = 0x6B,
	GTEQ_d = 0x6C,

	AND = 0x6D, /* logical and on uint8_t */
	OR = 0x6E, /* logical or on uint8_t */
	XOR = 0x6F, /* logical xor on uint8_t */
	NOT = 0x70, /* logical negation on uint8_t */

	AND_u8 = 0x71, /* bitwise and on uint8_t */
	AND_u16 = 0x72, /* bitwise and on uint16_t */
	AND_u32 = 0x73,
	AND_u64 = 0x74,

	OR_u8 = 0x75, /* bitwise or */
	OR_u16 = 0x76,
	OR_u32 = 0x77,
	OR_u64 = 0x78,

	XOR_u8 = 0x79, /* bitwise xor */
	XOR_u16 = 0x7A,
	XOR_u32 = 0x7B,
	XOR_u64 = 0x7C,

	NOT_u8 = 0x7D, /* bitwise negation */
	NOT_u16 = 0x7E,
	NOT_u32 = 0x7F,
	NOT_u64 = 0x80,

	LSHFT_u8 = 0x81, /* left-shift uint8_t */
	LSHFT_u16 = 0x82,
	LSHFT_u32 = 0x83,
	LSHFT_u64 = 0x84,

	RSHFT_u8 = 0x85, /* right-shift uint8_t */
	RSHFT_u16 = 0x86,
	RSHFT_u32 = 0x87,
	RSHFT_u64 = 0x88,

	JMP_u8 = 0x89, /* unconditinal branch */
	JMP_u16 = 0x8A,
	JMP_u32 = 0x8B,
	JMP_u64 = 0x8C,

	JMPIF_u8 = 0x8D, /* conditional branch */
	JMPIF_u16 = 0x8E,
	JMPIF_u32 = 0x8F,
	JMPIF_u64 = 0x90,

	PUSH_u8 = 0x91,
	PUSH_u16 = 0x92,
	PUSH_u32 = 0x93,
	PUSH_u64 = 0x94,

	POP_u8 = 0x95,
	POP_u16 = 0x96,
	POP_u32 = 0x97,
	POP_u64 = 0x98,

	LOAD_u8 = 0x99,
	LOAD_u16 = 0x9A,
	LOAD_u32 = 0x9B,
	LOAD_u64 = 0x9C,

	STORE_u8 = 0x9D,
	STORE_u16 = 0x9E,
	STORE_u32 = 0x9F,
	STORE_u64 = 0xA0,

	CALL_u8 = 0xA1,
	CALL_u16 = 0xA2,
	CALL_u32 = 0xA3,
	CALL_u64 = 0xA4,

	RET_u8 = 0xA5,
	RET_u16 = 0xA6,
	RET_u32 = 0xA7,
	RET_u64 = 0xA8,

	ARGC = 0xA9,
	ARG = 0xAA,

	HALT = 0xAB,

	SYSCALL = 0xAC
};

#endif
